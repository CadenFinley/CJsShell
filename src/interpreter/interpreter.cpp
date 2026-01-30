#include "interpreter.h"

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <unordered_set>
#include <utility>

#include "arithmetic_evaluator.h"
#include "builtin.h"
#include "case_evaluator.h"
#include "cjsh.h"
#include "command_substitution_evaluator.h"
#include "completions/suggestion_utils.h"
#include "conditional_evaluator.h"
#include "error_out.h"
#include "exec.h"
#include "function_evaluator.h"
#include "interpreter_utils.h"
#include "job_control.h"
#include "loop_evaluator.h"
#include "parameter_expansion_evaluator.h"
#include "parser/parser.h"
#include "parser/quote_info.h"
#include "parser/tokenizer.h"
#include "readonly_command.h"
#include "shell.h"
#include "signal_handler.h"
#include "utils/pipeline_status_utils.h"

using shell_script_interpreter::detail::contains_token;
using shell_script_interpreter::detail::is_control_flow_exit_code;
using shell_script_interpreter::detail::is_readable_file;
using shell_script_interpreter::detail::process_line_for_validation;
using shell_script_interpreter::detail::should_skip_line;
using shell_script_interpreter::detail::strip_inline_comment;
using shell_script_interpreter::detail::trim;

namespace {

int report_error_with_code(ErrorType type, ErrorSeverity severity, const std::string& command,
                           const std::string& message, const std::vector<std::string>& suggestions,
                           int code) {
    print_error({type, severity, command, message, suggestions});
    setenv("?", std::to_string(code).c_str(), 1);
    return code;
}

std::string sanitize_context(const std::string& text) {
    std::string sanitized = text;
    sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), '\n'), sanitized.end());
    sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), '\r'), sanitized.end());
    sanitized = trim(sanitized);
    if (sanitized.size() > 160) {
        sanitized.resize(157);
        sanitized.append("...");
    }
    return sanitized;
}

void append_context_hint(std::vector<std::string>& suggestions, const std::string& text,
                         size_t line_number) {
    if (text.empty() && line_number == 0) {
        return;
    }

    std::ostringstream builder;
    std::string sanitized = sanitize_context(text);
    if (line_number > 0) {
        builder << "line " << line_number;
        if (!sanitized.empty()) {
            builder << ": " << sanitized;
        }
    } else if (!sanitized.empty()) {
        builder << sanitized;
    }

    if (builder.tellp() > 0) {
        suggestions.push_back("Context: " + builder.str());
    }
}

std::string strip_cjsh_prefix(std::string message) {
    const std::string prefix = "cjsh:";
    if (message.rfind(prefix, 0) == 0) {
        message.erase(0, prefix.size());
        while (!message.empty() && std::isspace(static_cast<unsigned char>(message.front()))) {
            message.erase(message.begin());
        }
    }
    if (message.rfind("cjsh ", 0) == 0) {
        message.erase(0, 5);
        while (!message.empty() && std::isspace(static_cast<unsigned char>(message.front()))) {
            message.erase(message.begin());
        }
    }
    return message;
}

std::vector<std::string> build_command_suggestions(const std::string& command_name) {
    auto suggestions = suggestion_utils::generate_command_suggestions(command_name);
    return suggestions;
}

int handle_runtime_exception(const std::string& text, const std::runtime_error& e,
                             size_t line_number) {
    const std::string raw_message = e.what() ? std::string(e.what()) : "runtime error";
    std::string message = strip_cjsh_prefix(raw_message);
    std::vector<std::string> suggestions;
    auto add_context = [&]() { append_context_hint(suggestions, text, line_number); };

    const std::string needle = "command not found: ";
    size_t pos = raw_message.find(needle);
    if (pos != std::string::npos) {
        std::string command_name = trim(raw_message.substr(pos + needle.length()));
        if (command_name.empty()) {
            command_name = trim(text);
        }
        suggestions = build_command_suggestions(command_name);
        add_context();
        return report_error_with_code(ErrorType::COMMAND_NOT_FOUND, ErrorSeverity::ERROR,
                                      command_name, "", suggestions,
                                      ShellScriptInterpreter::exit_command_not_found);
    }

    if (message.find("Unclosed quote") != std::string::npos ||
        message.find("missing closing") != std::string::npos ||
        message.find("syntax error near unexpected token") != std::string::npos) {
        suggestions.push_back("Make sure all quotes and delimiters are balanced.");
        add_context();
        return report_error_with_code(ErrorType::SYNTAX_ERROR, ErrorSeverity::ERROR, "", message,
                                      suggestions, 2);
    }

    if (message.find("Failed to open") != std::string::npos ||
        message.find("Failed to redirect") != std::string::npos ||
        message.find("Failed to write") != std::string::npos) {
        suggestions.push_back("Check file permissions and paths.");
        add_context();
        return report_error_with_code(ErrorType::FILE_NOT_FOUND, ErrorSeverity::ERROR, "", message,
                                      suggestions, 2);
    }

    suggestions.push_back("Check command syntax and system resources.");
    add_context();
    return report_error_with_code(ErrorType::RUNTIME_ERROR, ErrorSeverity::ERROR, "", message,
                                  suggestions, 2);
}

}  // namespace

ShellScriptInterpreter::ShellScriptInterpreter() : shell_parser(nullptr) {
}

ShellScriptInterpreter::~ShellScriptInterpreter() = default;

void ShellScriptInterpreter::set_parser(Parser* parser) {
    shell_parser = parser;
}

std::vector<std::string> ShellScriptInterpreter::parse_into_lines(const std::string& script) {
    if (!shell_parser) {
        return {};
    }
    return shell_parser->parse_into_lines(script);
}

ShellScriptInterpreter::SyntaxError::SyntaxError(size_t line_num, const std::string& msg,
                                                 const std::string& line_content)
    : position({line_num, 0, 0, 0}),
      severity(ErrorSeverity::ERROR),
      category(ErrorCategory::SYNTAX),
      error_code("SYN001"),
      message(msg),
      line_content(line_content) {
}

ShellScriptInterpreter::SyntaxError::SyntaxError(ErrorPosition pos, ErrorSeverity sev,
                                                 ErrorCategory cat, const std::string& code,
                                                 const std::string& msg,
                                                 const std::string& line_content,
                                                 const std::string& suggestion)
    : position(pos),
      severity(sev),
      category(cat),
      error_code(code),
      message(msg),
      line_content(line_content),
      suggestion(suggestion) {
}

VariableManager& ShellScriptInterpreter::get_variable_manager() {
    return variable_manager;
}

int ShellScriptInterpreter::execute_subshell(const std::string& subshell_content) {
    pid_t pid = fork();
    if (pid == 0) {
        if (setpgid(0, 0) < 0) {
            perror("cjsh: setpgid failed in subshell child");
        }

        int exit_code = g_shell->execute(subshell_content, true);
        exit_code = read_exit_code_or(exit_code);

        int child_status = 0;
        while (waitpid(-1, &child_status, WNOHANG) > 0) {
        }

        exit(exit_code);
    } else if (pid > 0) {
        int status = 0;
        waitpid(pid, &status, 0);
        int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
        return set_last_status(exit_code);
    } else {
        print_error({ErrorType::RUNTIME_ERROR,
                     ErrorSeverity::ERROR,
                     "subshell",
                     "failed to fork for subshell execution",
                     {"Check system process limits."}});
        return 1;
    }
}

int ShellScriptInterpreter::execute_function_call(const std::vector<std::string>& expanded_args) {
    push_function_scope();

    std::vector<std::string> saved_params;
    if (g_shell) {
        saved_params = g_shell->get_positional_parameters();
    }

    std::vector<std::string> func_params;
    for (size_t pi = 1; pi < expanded_args.size(); ++pi) {
        func_params.push_back(expanded_args[pi]);
    }
    if (g_shell) {
        g_shell->set_positional_parameters(func_params);
    }

    std::vector<std::string> param_names;
    for (size_t pi = 1; pi < expanded_args.size() && pi <= 9; ++pi) {
        std::string name = std::to_string(pi);
        param_names.push_back(name);
        setenv(name.c_str(), expanded_args[pi].c_str(), 1);
    }

    int exit_code = execute_block(functions[expanded_args[0]]);

    if (exit_code == exit_break) {
        const char* return_code_env = getenv("CJSH_RETURN_CODE");
        if (return_code_env) {
            try {
                exit_code = std::stoi(return_code_env);
                unsetenv("CJSH_RETURN_CODE");
            } catch (const std::exception&) {
                exit_code = 0;
            }
        }
    }

    if (g_shell) {
        g_shell->set_positional_parameters(saved_params);
    }

    for (const auto& n : param_names)
        unsetenv(n.c_str());

    pop_function_scope();

    return set_last_status(exit_code);
}

int ShellScriptInterpreter::invoke_function(const std::vector<std::string>& args) {
    if (args.empty()) {
        return set_last_status(0);
    }

    auto it = functions.find(args[0]);
    if (it == functions.end()) {
        return set_last_status(127);
    }

    return execute_function_call(args);
}

int ShellScriptInterpreter::handle_env_assignment(const std::vector<std::string>& expanded_args) {
    std::string var_name;
    std::string var_value;
    if (shell_parser->is_env_assignment(expanded_args[0], var_name, var_value)) {
        shell_parser->expand_env_vars(var_value);

        if (variable_manager.is_local_variable(var_name)) {
            variable_manager.set_local_variable(var_name, var_value);
        } else {
            variable_manager.set_environment_variable(var_name, var_value);
        }

        int status =
            pending_assignment_exit_status.value_or(last_substitution_exit_status.value_or(0));
        last_substitution_exit_status.reset();
        pending_assignment_exit_status.reset();
        return status;
    }
    return -1;
}

int ShellScriptInterpreter::execute_block(const std::vector<std::string>& lines,
                                          bool skip_validation) {
    struct ValidationScope {
        ShellScriptInterpreter* self;
        bool previous;
        ValidationScope(ShellScriptInterpreter* s, bool skip)
            : self(s), previous(s->skip_validation_mode) {
            if (skip) {
                self->skip_validation_mode = true;
            }
        }

        ~ValidationScope() {
            self->skip_validation_mode = previous;
        }
    } validation_scope(this, skip_validation);

    const bool effective_skip = skip_validation_mode;

    if (g_shell == nullptr) {
        print_error({ErrorType::RUNTIME_ERROR, "", "No shell instance available", {}});
    }

    if (shell_parser == nullptr) {
        std::vector<std::string> empty_suggestions;
        ErrorInfo error(ErrorType::RUNTIME_ERROR, ErrorSeverity::CRITICAL, "",
                        "Script interpreter not properly initialized", empty_suggestions);
        print_error(error);
        return 1;
    }

    if (!effective_skip && has_syntax_errors(lines)) {
        std::vector<std::string> empty_suggestions;
        ErrorInfo error(ErrorType::SYNTAX_ERROR, ErrorSeverity::CRITICAL, "",
                        "Critical syntax errors detected in script block, process aborted",
                        empty_suggestions);
        print_error(error);
        return 2;
    }

    std::function<int(const std::string&, bool)> execute_simple_or_pipeline_impl;
    std::function<int(const std::string&)> execute_simple_or_pipeline;

    std::function<int(const std::string&)> evaluate_logical_condition;
    std::function<std::optional<int>(const std::string&, bool)> try_handle_inline_case;

    execute_simple_or_pipeline = [&](const std::string& cmd_text) -> int {
        return execute_simple_or_pipeline_impl(cmd_text, true);
    };

    evaluate_logical_condition = [&](const std::string& condition) -> int {
        return evaluate_logical_condition_internal(condition, execute_simple_or_pipeline);
    };

    execute_simple_or_pipeline_impl = [&](const std::string& cmd_text,
                                          bool allow_semicolon_split) -> int {
        std::string text = process_line_for_validation(cmd_text);
        if (text.empty())
            return 0;

        auto command_has_redirection = [](const Command& command) {
            return command.stderr_to_stdout || command.stdout_to_stderr ||
                   !command.input_file.empty() || !command.output_file.empty() ||
                   !command.append_file.empty() || !command.stderr_file.empty() ||
                   !command.here_doc.empty();
        };

        if (shell_parser) {
            std::vector<LogicalCommand> logical_cmds = shell_parser->parse_logical_commands(text);
            bool has_logical_op = false;
            for (const auto& lc : logical_cmds) {
                if (!lc.op.empty()) {
                    has_logical_op = true;
                    break;
                }
            }

            if (has_logical_op) {
                int logical_status = 0;
                for (size_t idx = 0; idx < logical_cmds.size(); ++idx) {
                    if (idx > 0) {
                        const std::string& prev_op = logical_cmds[idx - 1].op;
                        bool is_control_flow = is_control_flow_exit_code(logical_status);
                        if (prev_op == "&&" && logical_status != 0 && !is_control_flow) {
                            continue;
                        }
                        if (prev_op == "||" && logical_status == 0) {
                            continue;
                        }
                        if (is_control_flow) {
                            break;
                        }
                    }

                    logical_status =
                        execute_simple_or_pipeline_impl(logical_cmds[idx].command, true);
                }
                return logical_status;
            }
        }

        if (allow_semicolon_split && shell_parser && text.find(';') != std::string::npos) {
            auto semicolon_commands = shell_parser->parse_semicolon_commands(text);

            if (semicolon_commands.size() > 1) {
                int last_code = 0;
                for (const auto& part : semicolon_commands) {
                    last_code = execute_simple_or_pipeline_impl(part, false);

                    if (g_shell && g_shell->should_abort_on_nonzero_exit(last_code) &&
                        last_code != 0 && !is_control_flow_exit_code(last_code)) {
                        return last_code;
                    }
                }
                return last_code;
            }
        }

        std::vector<std::string> parsed_args;
        std::vector<Command> cmds;
        bool has_redir_or_pipe = false;
        bool has_multiple_commands = false;
        std::string trimmed_text = trim(text);
        try {
            text = expand_all_substitutions(text, execute_simple_or_pipeline);

            auto raw_tokens = Tokenizer::tokenize_command(text);
            auto merged_tokens = Tokenizer::merge_redirection_tokens(raw_tokens);
            if (!merged_tokens.empty()) {
                static const std::unordered_set<std::string> kRedirectOperators = {
                    "<", ">", ">>", ">|", "<<", "<<-", "<<<", "&>", "<>", "<&", ">&"};

                auto requires_operand = [&](const std::string& token) -> bool {
                    if (kRedirectOperators.count(token) > 0) {
                        return true;
                    }
                    size_t digits = 0;
                    while (digits < token.size() &&
                           (std::isdigit(static_cast<unsigned char>(token[digits])) != 0)) {
                        digits++;
                    }
                    if (digits == 0 || digits >= token.size()) {
                        return false;
                    }
                    std::string suffix = token.substr(digits);
                    if (kRedirectOperators.count(suffix) > 0) {
                        return true;
                    }
                    if (suffix == ">" || suffix == ">>" || suffix == "<" || suffix == "<<") {
                        return true;
                    }
                    return false;
                };

                QuoteInfo last_token(merged_tokens.back());
                if (requires_operand(last_token.value)) {
                    std::vector<std::string> suggestions = {
                        "Provide a destination after the redirection operator."};
                    append_context_hint(suggestions, text, current_line_number);
                    return report_error_with_code(ErrorType::SYNTAX_ERROR, ErrorSeverity::ERROR, "",
                                                  "syntax error near unexpected token `newline'",
                                                  suggestions, 2);
                }
            }

            cmds = shell_parser->parse_pipeline_with_preprocessing(text);

            has_multiple_commands = cmds.size() > 1;
            has_redir_or_pipe = has_multiple_commands;
            if (!has_multiple_commands && !cmds.empty()) {
                const auto& c = cmds[0];
                has_redir_or_pipe = c.background || !c.input_file.empty() ||
                                    !c.output_file.empty() || !c.append_file.empty() ||
                                    c.stderr_to_stdout || c.stdout_to_stderr ||
                                    !c.stderr_file.empty() || !c.here_doc.empty() ||
                                    c.both_output || !c.here_string.empty() ||
                                    !c.fd_redirections.empty() || !c.fd_duplications.empty();

                if (c.negate_pipeline) {
                    has_redir_or_pipe = true;
                }
            }

            if (has_multiple_commands) {
                bool looks_like_case =
                    !trimmed_text.empty() &&
                    (trimmed_text.rfind("case ", 0) == 0 || trimmed_text == "case");
                if (looks_like_case) {
                    has_multiple_commands = false;
                    has_redir_or_pipe = false;
                    cmds.clear();
                }
            }

            if (!has_multiple_commands) {
                parsed_args = shell_parser->parse_command(text);
                if (!parsed_args.empty()) {
                    const std::string& prog = parsed_args[0];
                    if (should_interpret_as_cjsh_script(prog)) {
                        std::ifstream f(prog);
                        if (!f) {
                            print_error({ErrorType::RUNTIME_ERROR,
                                         "",
                                         "Failed to open script file: " + prog,
                                         {}});
                            return 1;
                        }
                        std::stringstream buffer;
                        buffer << f.rdbuf();
                        auto nested_lines = shell_parser->parse_into_lines(buffer.str());
                        return execute_block(nested_lines);
                    }

                    if (prog == "if" || prog.rfind("if ", 0) == 0 || prog == "for" ||
                        prog.rfind("for ", 0) == 0 || prog == "while" ||
                        prog.rfind("while ", 0) == 0 || prog == "until" ||
                        prog.rfind("until ", 0) == 0) {
                        std::vector<std::string> block_lines;
                        if (shell_parser) {
                            block_lines = shell_parser->parse_into_lines(text);
                        }
                        if (block_lines.empty()) {
                            block_lines.push_back(text);
                        }

                        auto run_block = [&]() -> int { return execute_block(block_lines); };
                        int exit_code = 0;
                        bool handled_with_redirections = false;

                        if (g_shell && g_shell->shell_exec) {
                            try {
                                std::vector<Command> control_cmds =
                                    shell_parser->parse_pipeline_with_preprocessing(text);
                                if (!control_cmds.empty()) {
                                    Command control_cmd = control_cmds[0];
                                    std::string command_name =
                                        control_cmd.args.empty() ? prog : control_cmd.args[0];
                                    bool action_invoked = false;
                                    exit_code = g_shell->shell_exec->run_with_command_redirections(
                                        control_cmd, run_block, command_name, false,
                                        &action_invoked);
                                    if (!action_invoked) {
                                        return exit_code;
                                    }
                                    handled_with_redirections = true;
                                }
                            } catch (const std::exception&) {
                            }
                        }

                        if (!handled_with_redirections) {
                            exit_code = run_block();
                        }

                        return exit_code;
                    }
                }
            }
        } catch (const std::runtime_error& e) {
            throw;
        }

        if (!has_multiple_commands && (text == "case" || text.rfind("case ", 0) == 0) &&
            text.find("esac") == std::string::npos) {
            std::string completed_case = text + ";; esac";
            return execute_simple_or_pipeline(completed_case);
        }

        if (!has_multiple_commands) {
            if (auto inline_case_result = try_handle_inline_case(text, false)) {
                return *inline_case_result;
            }
        }

        try {
            if (!has_redir_or_pipe && !cmds.empty()) {
                const auto& c = cmds[0];

                if (!c.args.empty() && c.args[0] == "__INTERNAL_SUBSHELL__") {
                    if (command_has_redirection(c)) {
                        return run_pipeline(cmds);
                    }
                    if (c.args.size() >= 2) {
                        return execute_subshell(c.args[1]);
                    } else {
                        return 1;
                    }

                } else if (!c.args.empty() && c.args[0] == "__INTERNAL_BRACE_GROUP__") {
                    if (command_has_redirection(c)) {
                        return run_pipeline(cmds);
                    }

                    if (c.args.size() >= 2) {
                        int exit_code = g_shell ? g_shell->execute(c.args[1]) : 1;
                        return set_last_status(exit_code);
                    }

                    return 0;

                } else {
                    std::vector<std::string> expanded_args = std::move(parsed_args);
                    if (expanded_args.empty() && !c.args.empty()) {
                        expanded_args = c.args;
                    }
                    if (expanded_args.empty())
                        return 0;

                    if (expanded_args.size() == 2 && expanded_args[0] == "__ALIAS_PIPELINE__") {
                        std::vector<Command> pipeline_cmds =
                            shell_parser->parse_pipeline_with_preprocessing(expanded_args[1]);
                        return run_pipeline(pipeline_cmds);
                    }

                    if (expanded_args.size() == 1) {
                        int env_result = handle_env_assignment(expanded_args);
                        if (env_result >= 0) {
                            return env_result;
                        }
                    }

                    if (!expanded_args.empty() && functions.count(expanded_args[0])) {
                        return execute_function_call(expanded_args);
                    }
                    int exit_code = g_shell->execute_command(expanded_args, c.background);
                    return set_last_status(exit_code);
                }
            }

            if (cmds.empty())
                return 0;
            return run_pipeline(cmds);
        } catch (const std::bad_alloc&) {
            std::vector<std::string> suggestions = {
                "Command may be too complex or system is low on memory."};
            append_context_hint(suggestions, text, current_line_number);
            return report_error_with_code(ErrorType::RUNTIME_ERROR, ErrorSeverity::ERROR,
                                          "interpreter", "memory allocation failed", suggestions,
                                          3);
        } catch (const std::system_error& e) {
            std::vector<std::string> suggestions = {"Check system resources and permissions."};
            append_context_hint(suggestions, text, current_line_number);
            return report_error_with_code(ErrorType::RUNTIME_ERROR, ErrorSeverity::ERROR,
                                          "interpreter", strip_cjsh_prefix(e.what()), suggestions,
                                          4);
        } catch (const std::runtime_error& e) {
            return handle_runtime_exception(text, e, current_line_number);
        } catch (const std::exception& e) {
            std::vector<std::string> suggestions = {
                "Please report this issue along with steps to reproduce."};
            append_context_hint(suggestions, text, current_line_number);
            return report_error_with_code(ErrorType::UNKNOWN_ERROR, ErrorSeverity::ERROR,
                                          "interpreter", strip_cjsh_prefix(e.what()), suggestions,
                                          5);
        } catch (...) {
            std::vector<std::string> suggestions = {
                "Please report this issue along with steps to reproduce."};
            append_context_hint(suggestions, text, current_line_number);
            return report_error_with_code(ErrorType::UNKNOWN_ERROR, ErrorSeverity::ERROR,
                                          "interpreter", "unknown interpreter error", suggestions,
                                          6);
        }
    };

    try_handle_inline_case = [this, &execute_simple_or_pipeline](
                                 const std::string& candidate,
                                 bool allow_command_substitution) -> std::optional<int> {
        auto pattern_match_fn = [this](const std::string& text, const std::string& pattern) {
            return pattern_matcher.matches_pattern(text, pattern);
        };
        auto cmd_sub_expander = [this, &execute_simple_or_pipeline](const std::string& input) {
            std::string expanded = expand_all_substitutions(input, execute_simple_or_pipeline);
            return std::make_pair(expanded, std::vector<std::string>{});
        };
        return case_evaluator::handle_inline_case(candidate, execute_simple_or_pipeline,
                                                  allow_command_substitution, true, shell_parser,
                                                  pattern_match_fn, cmd_sub_expander);
    };

    auto check_pending_signals = [&]() -> std::optional<int> {
        if (!g_shell || !SignalHandler::has_pending_signals()) {
            return std::nullopt;
        }

        SignalProcessingResult pending = g_shell->process_pending_signals();
#ifdef SIGTERM
        if (pending.sigterm) {
            return 128 + SIGTERM;
        }
#endif
#ifdef SIGHUP
        if (pending.sighup) {
            return 128 + SIGHUP;
        }
#endif
#ifdef SIGINT
        if (pending.sigint) {
            return 128 + SIGINT;
        }
#endif
        return std::nullopt;
    };

    int last_code = 0;

    auto execute_block_wrapper = [&](const std::vector<std::string>& block_lines) -> int {
        return execute_block(block_lines);
    };

    auto execute_block_skip_validation = [&](const std::vector<std::string>& block_lines) -> int {
        return execute_block(block_lines, true);
    };

    auto handle_if_block = [&](const std::vector<std::string>& src_lines, size_t& idx) -> int {
        return conditional_evaluator::handle_if_block(src_lines, idx, execute_block_wrapper,
                                                      execute_simple_or_pipeline,
                                                      evaluate_logical_condition, shell_parser);
    };

    auto handle_for_block = [&](const std::vector<std::string>& src_lines, size_t& idx) -> int {
        return loop_evaluator::handle_for_block(src_lines, idx, execute_block_skip_validation,
                                                shell_parser);
    };

    auto handle_case_block = [&](const std::vector<std::string>& src_lines, size_t& idx) -> int {
        std::string first = trim(strip_inline_comment(src_lines[idx]));
        if (first != "case" && first.rfind("case ", 0) != 0)
            return 1;

        if (auto inline_case_result = try_handle_inline_case(first, true)) {
            return *inline_case_result;
        }

        std::string header_accum = first;
        size_t j = idx;
        bool found_in = false;

        auto header_tokens = shell_parser->parse_command(header_accum);
        if (std::find(header_tokens.begin(), header_tokens.end(), "in") != header_tokens.end())
            found_in = true;

        while (!found_in && ++j < src_lines.size()) {
            std::string cur = trim(strip_inline_comment(src_lines[j]));
            if (cur.empty())
                continue;
            header_accum += " " + cur;
            header_tokens = shell_parser->parse_command(header_accum);
            if (std::find(header_tokens.begin(), header_tokens.end(), "in") !=
                header_tokens.end()) {
                found_in = true;
                break;
            }
        }

        if (!found_in) {
            idx = j;
            return 1;
        }

        std::string expanded_header = header_accum;
        if (header_accum.find("$(") != std::string::npos) {
            expanded_header = expand_all_substitutions(header_accum, execute_simple_or_pipeline);
        }

        std::vector<std::string> expanded_tokens = shell_parser->parse_command(expanded_header);
        size_t tok_idx = 0;
        if (tok_idx < expanded_tokens.size() && expanded_tokens[tok_idx] == "case")
            ++tok_idx;

        std::string raw_case_value;
        if (tok_idx < expanded_tokens.size())
            raw_case_value = expanded_tokens[tok_idx++];

        if (std::find(expanded_tokens.begin(), expanded_tokens.end(), "in") ==
                expanded_tokens.end() ||
            raw_case_value.empty()) {
            idx = j;
            return 1;
        }

        std::string case_value = case_evaluator::normalize_case_value(raw_case_value, shell_parser);

        size_t in_pos = expanded_header.find(" in ");
        std::string inline_segment;
        if (in_pos != std::string::npos)
            inline_segment = trim(expanded_header.substr(in_pos + 4));

        size_t esac_index = j;
        bool inline_has_esac = false;
        size_t inline_esac_pos = inline_segment.find("esac");
        if (inline_esac_pos != std::string::npos) {
            inline_has_esac = true;
            inline_segment = trim(inline_segment.substr(0, inline_esac_pos));
        }

        std::string combined_patterns;
        if (!inline_segment.empty())
            combined_patterns = inline_segment;

        if (!inline_has_esac) {
            auto body_pair = case_evaluator::collect_case_body(src_lines, j + 1, shell_parser);
            std::string body_content = body_pair.first;
            esac_index = body_pair.second;
            if (esac_index >= src_lines.size()) {
                idx = esac_index;
                return 1;
            }
            if (!body_content.empty()) {
                if (!combined_patterns.empty())
                    combined_patterns += '\n';
                combined_patterns += body_content;
            }
        } else {
            esac_index = j;
        }

        auto case_pattern_match_fn = [this](const std::string& text, const std::string& pattern) {
            return pattern_matcher.matches_pattern(text, pattern);
        };

        auto case_result = case_evaluator::evaluate_case_patterns(
            combined_patterns, case_value, false, execute_simple_or_pipeline, shell_parser,
            case_pattern_match_fn);
        idx = esac_index;
        return case_result.first ? case_result.second : 0;
    };

    auto handle_while_block = [&](const std::vector<std::string>& src_lines, size_t& idx) -> int {
        return loop_evaluator::handle_condition_loop_block(
            loop_evaluator::LoopCondition::WHILE, src_lines, idx, execute_block_skip_validation,
            execute_simple_or_pipeline, shell_parser);
    };

    auto handle_until_block = [&](const std::vector<std::string>& src_lines, size_t& idx) -> int {
        return loop_evaluator::handle_condition_loop_block(
            loop_evaluator::LoopCondition::UNTIL, src_lines, idx, execute_block_skip_validation,
            execute_simple_or_pipeline, shell_parser);
    };

    for (size_t line_index = 0; line_index < lines.size(); ++line_index) {
        current_line_number = line_index + 1;

        if (auto pending_code = check_pending_signals()) {
            last_code = *pending_code;
            return set_last_status(last_code);
        }

        const auto& raw_line = lines[line_index];
        std::string line = trim(strip_inline_comment(raw_line));

        if (line.empty()) {
            continue;
        }

        if (should_skip_line(line)) {
            if (g_shell != nullptr && g_shell->get_shell_option("verbose")) {
                std::cerr << line << '\n';
            }
            continue;
        }

        auto block_result =
            try_dispatch_block_statement(lines, line_index, line, handle_if_block, handle_for_block,
                                         handle_while_block, handle_until_block, handle_case_block);

        if (block_result.handled) {
            last_code = block_result.exit_code;
            line_index = block_result.next_line_index;
            if (is_control_flow_exit_code(last_code) || g_exit_flag) {
                return last_code;
            }
            continue;
        }

        bool handled_pipeline_loop = false;
        size_t pipe_search_pos = 0;
        while (pipe_search_pos < line.size()) {
            size_t pipe_pos = line.find('|', pipe_search_pos);
            if (pipe_pos == std::string::npos) {
                break;
            }

            std::string after_pipe = trim(line.substr(pipe_pos + 1));
            bool is_loop_keyword = after_pipe.rfind("while", 0) == 0 ||
                                   after_pipe.rfind("until", 0) == 0 ||
                                   after_pipe.rfind("for", 0) == 0;

            if (!is_loop_keyword) {
                pipe_search_pos = pipe_pos + 1;
                continue;
            }

            size_t gather_index = line_index;
            int loop_depth = 0;
            std::vector<std::string> block_lines;
            block_lines.reserve(4);

            while (gather_index < lines.size()) {
                const std::string& gather_raw = lines[gather_index];
                std::string gather_trimmed = trim(strip_inline_comment(gather_raw));

                block_lines.push_back(gather_raw);

                if (contains_token(gather_trimmed, "do")) {
                    loop_depth++;
                }
                if (contains_token(gather_trimmed, "done")) {
                    loop_depth--;
                    if (loop_depth <= 0) {
                        break;
                    }
                }

                gather_index++;
            }

            if (loop_depth <= 0 && !block_lines.empty()) {
                std::string combined;
                combined.reserve(128);
                for (size_t idx = 0; idx < block_lines.size(); ++idx) {
                    if (idx > 0) {
                        combined.push_back('\n');
                    }
                    combined += block_lines[idx];
                }

                last_code = execute_simple_or_pipeline(combined);
                line_index = gather_index;
                handled_pipeline_loop = true;
            }

            break;
        }

        if (handled_pipeline_loop) {
            continue;
        }

        std::string trimmed_line = trim(line);
        bool is_function_def = false;
        if (line.find("()") != std::string::npos && line.find('{') != std::string::npos) {
            is_function_def = true;
        } else if (trimmed_line.rfind("function", 0) == 0 && trimmed_line.length() > 8 &&
                   std::isspace(static_cast<unsigned char>(trimmed_line[8])) &&
                   line.find('{') != std::string::npos) {
            is_function_def = true;
        }

        if (is_function_def) {
            auto parse_result = function_evaluator::parse_and_register_functions(
                line, lines, line_index, functions, trim, strip_inline_comment);

            if (!parse_result.remaining_line.empty()) {
                line = parse_result.remaining_line;
            } else {
                continue;
            }
        }

        std::vector<LogicalCommand> lcmds = shell_parser->parse_logical_commands(line);
        if (lcmds.empty())
            continue;

        last_code = 0;
        for (size_t i = 0; i < lcmds.size(); ++i) {
            const auto& lc = lcmds[i];

            if (i > 0) {
                const std::string& prev_op = lcmds[i - 1].op;

                bool is_control_flow = is_control_flow_exit_code(last_code);
                if (prev_op == "&&" && last_code != 0 && !is_control_flow) {
                    continue;
                }
                if (prev_op == "||" && last_code == 0) {
                    continue;
                }

                if (is_control_flow) {
                    break;
                }
            }

            std::string cmd_to_parse = lc.command;
            std::string trimmed_cmd = trim(strip_inline_comment(cmd_to_parse));

            if (!trimmed_cmd.empty() && (trimmed_cmd[0] == '(' || trimmed_cmd[0] == '{')) {
                int code = execute_simple_or_pipeline(cmd_to_parse);
                last_code = code;
                continue;
            }

            if ((trimmed_cmd == "if" || trimmed_cmd.rfind("if ", 0) == 0) &&
                (trimmed_cmd.find("; then") != std::string::npos) &&
                (trimmed_cmd.find(" fi") != std::string::npos ||
                 trimmed_cmd.find("; fi") != std::string::npos ||
                 trimmed_cmd.rfind("fi") == trimmed_cmd.length() - 2)) {
                size_t local_idx = 0;
                std::vector<std::string> one{trimmed_cmd};
                int code = handle_if_block(one, local_idx);
                last_code = code;
                continue;
            }

            if ((trimmed_cmd == "case" || trimmed_cmd.rfind("case ", 0) == 0) &&
                (trimmed_cmd.find(" in ") != std::string::npos) &&
                (trimmed_cmd.find("esac") != std::string::npos)) {
                size_t local_idx = 0;
                std::vector<std::string> one{trimmed_cmd};
                int code = handle_case_block(one, local_idx);
                last_code = code;
                continue;
            }

            auto semis = shell_parser->parse_semicolon_commands(lc.command);
            if (semis.empty()) {
                last_code = 0;
                continue;
            }
            for (size_t k = 0; k < semis.size(); ++k) {
                const std::string& semi = semis[k];
                auto segs = shell_script_interpreter::detail::split_ampersand(semi);
                if (segs.empty())
                    segs.push_back(semi);
                for (const auto& cmd_text : segs) {
                    if (g_shell != nullptr && g_shell->get_shell_option("verbose")) {
                        std::string verbose_text = trim(strip_inline_comment(cmd_text));
                        if (!verbose_text.empty()) {
                            std::cerr << verbose_text << '\n';
                        }
                    }

                    std::string t = trim(strip_inline_comment(cmd_text));

                    bool is_inline_function = false;
                    std::string func_name;
                    size_t brace_pos = t.find('{');

                    if (t.rfind("function", 0) == 0 && t.length() > 8 &&
                        std::isspace(static_cast<unsigned char>(t[8])) &&
                        brace_pos != std::string::npos) {
                        size_t name_start = 8;
                        while (name_start < t.length() &&
                               std::isspace(static_cast<unsigned char>(t[name_start]))) {
                            name_start++;
                        }
                        if (name_start < t.length()) {
                            size_t name_end = name_start;
                            while (name_end < t.length() &&
                                   !std::isspace(static_cast<unsigned char>(t[name_end])) &&
                                   t[name_end] != '(' && t[name_end] != '{') {
                                name_end++;
                            }
                            func_name = t.substr(name_start, name_end - name_start);
                            is_inline_function = true;
                        }
                    }

                    if (!is_inline_function && t.find("()") != std::string::npos &&
                        brace_pos != std::string::npos) {
                        size_t name_end = t.find("()");
                        if (name_end != std::string::npos && brace_pos != std::string::npos &&
                            name_end < brace_pos) {
                            func_name = trim(t.substr(0, name_end));
                            is_inline_function = true;
                        }
                    }

                    if (is_inline_function && !func_name.empty() &&
                        func_name.find(' ') == std::string::npos) {
                        std::vector<std::string> body_lines;
                        std::string after_brace = trim(t.substr(brace_pos + 1));
                        if (!after_brace.empty()) {
                            size_t end_brace = after_brace.find('}');
                            if (end_brace != std::string::npos) {
                                std::string body_part = trim(after_brace.substr(0, end_brace));
                                if (!body_part.empty())
                                    body_lines.push_back(body_part);
                                functions[func_name] = body_lines;
                                last_code = 0;
                                continue;
                            }
                        }
                    }

                    if ((t.rfind("for ", 0) == 0 || t == "for") &&
                        t.find("; do") != std::string::npos) {
                        size_t local_idx = 0;
                        std::vector<std::string> one{t};
                        int code = handle_for_block(one, local_idx);
                        last_code = code;
                        continue;
                    }
                    if ((t.rfind("while ", 0) == 0 || t == "while") &&
                        t.find("; do") != std::string::npos) {
                        size_t local_idx = 0;
                        std::vector<std::string> one{t};
                        int code = handle_while_block(one, local_idx);
                        last_code = code;
                        continue;
                    }
                    if ((t.rfind("until ", 0) == 0 || t == "until") &&
                        t.find("; do") != std::string::npos) {
                        size_t local_idx = 0;
                        std::vector<std::string> one{t};
                        int code = handle_until_block(one, local_idx);
                        last_code = code;
                        continue;
                    }

                    if ((t.rfind("if ", 0) == 0 || t == "if") &&
                        t.find("; then") != std::string::npos &&
                        t.find(" fi") != std::string::npos) {
                        size_t local_idx = 0;
                        std::vector<std::string> one{t};
                        int code = handle_if_block(one, local_idx);
                        last_code = code;
                        continue;
                    }

                    if ((t.rfind("for ", 0) == 0 || t == "for")) {
                        if (auto inline_result = loop_evaluator::try_execute_inline_do_block(
                                t, semis, k, handle_for_block)) {
                            last_code = *inline_result;
                            break;
                        }
                    }
                    if ((t.rfind("while ", 0) == 0 || t == "while")) {
                        if (auto inline_result = loop_evaluator::try_execute_inline_do_block(
                                t, semis, k, handle_while_block)) {
                            last_code = *inline_result;
                            break;
                        }
                    }

                    if ((t.rfind("until ", 0) == 0 || t == "until")) {
                        if (auto inline_result = loop_evaluator::try_execute_inline_do_block(
                                t, semis, k, handle_until_block)) {
                            last_code = *inline_result;
                            break;
                        }
                    }

                    int code = 0;
                    bool is_function_call = false;
                    {
                        auto command_has_pipeline = [](const std::string& command) {
                            bool in_single = false;
                            bool in_double = false;
                            bool escaped = false;
                            int paren_depth = 0;

                            for (char ch : command) {
                                if (escaped) {
                                    escaped = false;
                                    continue;
                                }

                                if (ch == '\\') {
                                    escaped = true;
                                    continue;
                                }

                                if (ch == '\'' && !in_double) {
                                    in_single = !in_single;
                                    continue;
                                }

                                if (ch == '"' && !in_single) {
                                    in_double = !in_double;
                                    continue;
                                }

                                if (in_single) {
                                    continue;
                                }

                                if (!in_double) {
                                    if (ch == '(') {
                                        ++paren_depth;
                                    } else if (ch == ')' && paren_depth > 0) {
                                        --paren_depth;
                                    }
                                }

                                if (!in_double && paren_depth == 0 && ch == '|') {
                                    return true;
                                }
                            }

                            return false;
                        };

                        bool contains_pipeline = command_has_pipeline(cmd_text);
                        std::vector<std::string> first_toks = shell_parser->parse_command(cmd_text);

                        if (!first_toks.empty() && (functions.count(first_toks[0]) != 0U)) {
                            std::string expanded_cmd = cmd_text;
                            try {
                                expanded_cmd =
                                    expand_all_substitutions(cmd_text, execute_simple_or_pipeline);
                            } catch (const std::runtime_error&) {
                                expanded_cmd = cmd_text;
                            }

                            contains_pipeline = command_has_pipeline(expanded_cmd);
                            first_toks = shell_parser->parse_command(expanded_cmd);
                        }

                        if (!contains_pipeline && !first_toks.empty() &&
                            (functions.count(first_toks[0]) != 0U)) {
                            is_function_call = true;

                            push_function_scope();

                            std::vector<std::string> saved_params;
                            if (g_shell) {
                                saved_params = g_shell->get_positional_parameters();
                            }

                            std::vector<std::string> func_params;
                            for (size_t pi = 1; pi < first_toks.size(); ++pi) {
                                func_params.push_back(first_toks[pi]);
                            }
                            if (g_shell) {
                                g_shell->set_positional_parameters(func_params);
                            }

                            std::vector<std::string> param_names;
                            for (size_t pi = 1; pi < first_toks.size() && pi <= 9; ++pi) {
                                std::string name = std::to_string(pi);
                                param_names.push_back(name);
                                setenv(name.c_str(), first_toks[pi].c_str(), 1);
                            }

                            code = execute_block(functions[first_toks[0]]);

                            if (code == exit_break) {
                                const char* return_code_env = getenv("CJSH_RETURN_CODE");
                                if (return_code_env != nullptr) {
                                    try {
                                        code = std::stoi(return_code_env);
                                        unsetenv("CJSH_RETURN_CODE");
                                    } catch (const std::exception&) {
                                        code = 0;
                                    }
                                }
                            }

                            if (g_shell) {
                                g_shell->set_positional_parameters(saved_params);
                            }

                            for (const auto& n : param_names)
                                unsetenv(n.c_str());

                            pop_function_scope();
                        } else {
                            try {
                                code = execute_simple_or_pipeline(cmd_text);
                            } catch (const std::runtime_error& e) {
                                code = 1;
                            }
                        }
                    }
                    last_code = code;

                    set_last_status(last_code);

                    if (auto pending_code = check_pending_signals()) {
                        last_code = *pending_code;
                        return set_last_status(last_code);
                    }

                    if (g_shell && g_shell->should_abort_on_nonzero_exit(code) && code != 0) {
                        if (code != 253 && code != 254 && code != 255) {
                            return code;
                        }
                    }

                    if (!is_function_call && is_control_flow_exit_code(code)) {
                        goto control_flow_exit;
                    }
                }
            }
        }

    control_flow_exit:

        if (last_code == exit_command_not_found) {
            if (g_shell && g_shell->should_abort_on_nonzero_exit(last_code)) {
                return last_code;
            }
        } else if (is_control_flow_exit_code(last_code)) {
            return last_code;
        } else if (last_code != 0) {
            continue;
        }
    }

    return last_code;
}

bool ShellScriptInterpreter::should_interpret_as_cjsh_script(const std::string& path) const {
    if (!is_readable_file(path))
        return false;

    std::filesystem::path candidate(path);
    std::ifstream f(path);
    if (!f)
        return false;
    std::string first_line;
    std::getline(f, first_line);
    if (first_line.rfind("#!", 0) == 0 && first_line.find("cjsh") != std::string::npos)
        return true;
    if (first_line.find("cjsh") != std::string::npos)
        return true;
    return false;
}

int ShellScriptInterpreter::evaluate_logical_condition_internal(
    const std::string& condition, cjsh::FunctionRef<int(const std::string&)> executor) {
    std::string cond = trim(condition);
    if (cond.empty())
        return 1;

    std::string processed_cond = cond;
    size_t pos = 0;
    while ((pos = processed_cond.find("$((", pos)) != std::string::npos) {
        size_t start = pos + 3;
        size_t depth = 1;
        size_t end = start;

        while (end < processed_cond.length() && depth > 0) {
            if (end + 1 < processed_cond.length() && processed_cond.substr(end, 2) == "((") {
                depth++;
                end += 2;
            } else if (end + 1 < processed_cond.length() && processed_cond.substr(end, 2) == "))") {
                depth--;
                if (depth == 0) {
                    break;
                }
                end += 2;
            } else {
                end++;
            }
        }

        if (depth == 0 && end + 1 < processed_cond.length()) {
            std::string expr = processed_cond.substr(start, end - start);

            if (shell_parser != nullptr) {
                shell_parser->expand_env_vars(expr);
            }

            try {
                long long result = evaluate_arithmetic_expression(expr);
                std::string result_str = std::to_string(result);

                std::string new_cond;
                new_cond.reserve(processed_cond.size() - (end + 2 - pos) + result_str.size());
                new_cond.append(processed_cond, 0, pos);
                new_cond.append(result_str);
                new_cond.append(processed_cond, end + 2, std::string::npos);
                processed_cond = std::move(new_cond);
                pos = pos + result_str.length();
            } catch (const std::exception&) {
                pos = end + 2;
            }
        } else {
            pos++;
        }
    }
    int condition_status =
        conditional_evaluator::evaluate_logical_condition(processed_cond, executor);

    set_last_status(condition_status);
    return condition_status;
}

long long ShellScriptInterpreter::evaluate_arithmetic_expression(const std::string& expr) {
    auto var_reader = [this](const std::string& name) -> long long {
        std::string var_value = variable_manager.get_variable_value(name);
        if (!var_value.empty() || variable_manager.variable_is_set(name)) {
            try {
                return std::stoll(var_value);
            } catch (...) {
                return 0;
            }
        }
        return 0;
    };

    auto var_writer = [this](const std::string& name, long long value) {
        std::string value_str = std::to_string(value);

        if (variable_manager.is_local_variable(name)) {
            variable_manager.set_local_variable(name, value_str);
            return;
        }

        if (g_shell) {
            g_shell->get_env_vars()[name] = value_str;

            if (name == "PATH" || name == "PWD" || name == "HOME" || name == "USER" ||
                name == "SHELL") {
                setenv(name.c_str(), value_str.c_str(), 1);
            }

            if (shell_parser) {
                shell_parser->set_env_vars(g_shell->get_env_vars());
            }
        }
    };

    ArithmeticEvaluator evaluator(var_reader, var_writer);
    return evaluator.evaluate(expr);
}

int ShellScriptInterpreter::set_last_status(int code) {
    std::string value = std::to_string(code);
    setenv("?", value.c_str(), 1);

    Exec* exec_ptr = (g_shell && g_shell->shell_exec) ? g_shell->shell_exec.get() : nullptr;
    pipeline_status_utils::apply_pipeline_status_env(
        exec_ptr,
        [this](const std::string& value) {
            variable_manager.set_environment_variable("PIPESTATUS", value);
        },
        []() {
            if (g_shell) {
                auto& env_map = g_shell->get_env_vars();
                env_map.erase("PIPESTATUS");
                if (auto* parser = g_shell->get_parser()) {
                    parser->set_env_vars(env_map);
                }
            }
        });

    return code;
}

int ShellScriptInterpreter::run_pipeline(const std::vector<Command>& cmds) {
    if (!g_shell || !g_shell->shell_exec)
        return set_last_status(1);

    int exit_code = g_shell->shell_exec->execute_pipeline(cmds);
    g_shell->shell_exec->print_error_if_needed(exit_code);
    return set_last_status(exit_code);
}

std::string ShellScriptInterpreter::expand_parameter_expression(const std::string& param_expr) {
    auto var_reader = [this](const std::string& name) -> std::string {
        return variable_manager.get_variable_value(name);
    };

    auto var_writer = [this](const std::string& name, const std::string& value) {
        if (readonly_manager_is(name)) {
            print_error({ErrorType::INVALID_ARGUMENT, name, "readonly variable", {}});
            return;
        }

        variable_manager.set_environment_variable(name, value);
    };

    auto var_checker = [this](const std::string& name) -> bool {
        return variable_manager.variable_is_set(name);
    };

    auto pattern_match_fn = [this](const std::string& text, const std::string& pattern) -> bool {
        return pattern_matcher.matches_pattern(text, pattern);
    };

    ParameterExpansionEvaluator evaluator(var_reader, var_writer, var_checker, pattern_match_fn);
    return evaluator.expand(param_expr);
}

std::string ShellScriptInterpreter::get_variable_value(const std::string& var_name) {
    return variable_manager.get_variable_value(var_name);
}

bool ShellScriptInterpreter::variable_is_set(const std::string& var_name) {
    return variable_manager.variable_is_set(var_name);
}

bool ShellScriptInterpreter::has_function(const std::string& name) const {
    return function_evaluator::has_function(functions, name);
}

std::vector<std::string> ShellScriptInterpreter::get_function_names() const {
    return function_evaluator::get_function_names(functions);
}

void ShellScriptInterpreter::push_function_scope() {
    variable_manager.push_scope();
}

void ShellScriptInterpreter::pop_function_scope() {
    variable_manager.pop_scope();
}

void ShellScriptInterpreter::set_local_variable(const std::string& name, const std::string& value) {
    variable_manager.set_local_variable(name, value);
}

bool ShellScriptInterpreter::is_local_variable(const std::string& name) const {
    return variable_manager.is_local_variable(name);
}

bool ShellScriptInterpreter::unset_local_variable(const std::string& name) {
    return variable_manager.unset_local_variable(name);
}

void ShellScriptInterpreter::mark_local_as_exported(const std::string& name) {
    variable_manager.mark_local_as_exported(name);
}

bool ShellScriptInterpreter::in_function_scope() const {
    return variable_manager.in_function_scope();
}

ShellScriptInterpreter::BlockHandlerResult ShellScriptInterpreter::try_dispatch_block_statement(
    const std::vector<std::string>& lines, size_t line_index, const std::string& line,
    cjsh::FunctionRef<int(const std::vector<std::string>&, size_t&)> handle_if_block,
    cjsh::FunctionRef<int(const std::vector<std::string>&, size_t&)> handle_for_block,
    cjsh::FunctionRef<int(const std::vector<std::string>&, size_t&)> handle_while_block,
    cjsh::FunctionRef<int(const std::vector<std::string>&, size_t&)> handle_until_block,
    cjsh::FunctionRef<int(const std::vector<std::string>&, size_t&)> handle_case_block) {
    if (line == "if" || line.rfind("if ", 0) == 0) {
        size_t idx = line_index;
        int rc = handle_if_block(lines, idx);
        return {true, rc, idx};
    }

    if (line == "for" || line.rfind("for ", 0) == 0) {
        size_t idx = line_index;
        int rc = handle_for_block(lines, idx);
        return {true, rc, idx};
    }

    if (line == "while" || line.rfind("while ", 0) == 0) {
        size_t idx = line_index;
        int rc = handle_while_block(lines, idx);
        return {true, rc, idx};
    }

    if (line == "until" || line.rfind("until ", 0) == 0) {
        size_t idx = line_index;
        int rc = handle_until_block(lines, idx);
        return {true, rc, idx};
    }

    if (line == "case" || line.rfind("case ", 0) == 0) {
        size_t idx = line_index;
        int rc = handle_case_block(lines, idx);
        return {true, rc, idx};
    }

    return {false, 0, line_index};
}

std::string ShellScriptInterpreter::expand_all_substitutions(
    const std::string& input, cjsh::FunctionRef<int(const std::string&)> executor) {
    CommandSubstitutionEvaluator cmd_subst_evaluator(
        CommandSubstitutionEvaluator::create_command_executor(executor));

    auto expansion_result = cmd_subst_evaluator.expand_substitutions(input);
    if (!expansion_result.exit_codes.empty()) {
        last_substitution_exit_status = expansion_result.exit_codes.back();
        pending_assignment_exit_status = last_substitution_exit_status;
    } else {
        last_substitution_exit_status.reset();
    }
    std::string result = expansion_result.text;

    std::string out;
    out.reserve(result.size());

    bool in_quotes = false;
    char q = '\0';
    bool escaped = false;

    for (size_t i = 0; i < result.size(); ++i) {
        char c = result[i];

        if (escaped) {
            out += '\\';
            out += c;
            escaped = false;
            continue;
        }

        if (c == '\\' && (!in_quotes || q != '\'')) {
            escaped = true;
            continue;
        }

        if ((c == '"' || c == '\'') && (!in_quotes)) {
            in_quotes = true;
            q = c;
            out += c;
            continue;
        }
        if (in_quotes && c == q) {
            in_quotes = false;
            q = '\0';
            out += c;
            continue;
        }

        if (!in_quotes || q == '"') {
            if (c == '$' && i + 2 < result.size() && result[i + 1] == '(' && result[i + 2] == '(') {
                size_t inner_start = i + 3;
                int depth = 1;
                size_t j = inner_start;
                bool found = false;

                for (; j < result.size(); ++j) {
                    if (j + 1 < result.size() && result[j] == '(' && result[j - 1] != '\\') {
                        depth++;
                    } else if (result[j] == ')' && (j == 0 || result[j - 1] != '\\')) {
                        depth--;
                        if (depth == 0 && j + 1 < result.size() && result[j + 1] == ')') {
                            found = true;
                            break;
                        }
                    }
                }

                if (found) {
                    size_t expr_len = (j > inner_start) ? (j - inner_start) : 0;
                    std::string expr = result.substr(inner_start, expr_len);

                    std::string expanded_expr;
                    for (size_t k = 0; k < expr.size(); ++k) {
                        if (expr[k] == '$' && k + 1 < expr.size()) {
                            if (isdigit(expr[k + 1])) {
                                std::string param_name(1, expr[k + 1]);
                                expanded_expr += get_variable_value(param_name);
                                k++;
                            } else if (isalpha(expr[k + 1]) || expr[k + 1] == '_') {
                                size_t var_start = k + 1;
                                size_t var_end = var_start;
                                while (var_end < expr.size() &&
                                       (isalnum(expr[var_end]) || expr[var_end] == '_')) {
                                    var_end++;
                                }
                                std::string var_name = expr.substr(var_start, var_end - var_start);
                                expanded_expr += get_variable_value(var_name);
                                k = var_end - 1;
                            } else if (expr[k + 1] == '{') {
                                size_t close_brace = expr.find('}', k + 2);
                                if (close_brace != std::string::npos) {
                                    std::string var_name =
                                        expr.substr(k + 2, close_brace - (k + 2));
                                    expanded_expr += get_variable_value(var_name);
                                    k = close_brace;
                                } else {
                                    expanded_expr += expr[k];
                                }
                            } else if (expr[k + 1] == '(' && k + 2 < expr.size() &&
                                       expr[k + 2] == '(') {
                                int nested_depth = 1;
                                size_t nested_start = k + 3;
                                size_t nested_end = nested_start;
                                for (; nested_end < expr.size(); ++nested_end) {
                                    if (expr[nested_end] == '(' &&
                                        (nested_end == 0 || expr[nested_end - 1] != '\\')) {
                                        nested_depth++;
                                    } else if (expr[nested_end] == ')' &&
                                               (nested_end == 0 || expr[nested_end - 1] != '\\')) {
                                        nested_depth--;
                                        if (nested_depth == 0 && nested_end + 1 < expr.size() &&
                                            expr[nested_end + 1] == ')') {
                                            std::string nested_expr = expr.substr(
                                                nested_start, nested_end - nested_start);
                                            try {
                                                expanded_expr += std::to_string(
                                                    evaluate_arithmetic_expression(nested_expr));
                                            } catch (...) {
                                                expanded_expr += "0";
                                            }
                                            k = nested_end + 1;
                                            break;
                                        }
                                    }
                                }
                            } else {
                                expanded_expr += expr[k];
                            }
                        } else {
                            expanded_expr += expr[k];
                        }
                    }

                    try {
                        out += std::to_string(evaluate_arithmetic_expression(expanded_expr));
                    } catch (const std::runtime_error& e) {
                        throw std::runtime_error(std::string(e.what()) + " while evaluating $(" +
                                                 expr + "))");
                    }
                    i = j + 1;
                    continue;
                }
            }

            if (c == '$' && i + 1 < result.size() && result[i + 1] == '{') {
                size_t brace_depth = 1;
                size_t j = i + 2;
                bool found = false;

                while (j < result.size() && brace_depth > 0) {
                    if (result[j] == '{') {
                        brace_depth++;
                    } else if (result[j] == '}') {
                        brace_depth--;
                        if (brace_depth == 0) {
                            found = true;
                            break;
                        }
                    }
                    j++;
                }

                if (found) {
                    std::string param_expr = result.substr(i + 2, j - (i + 2));
                    std::string expanded_result = expand_parameter_expression(param_expr);

                    if (expanded_result.find('$') != std::string::npos) {
                        size_t dollar_pos = 0;
                        while ((dollar_pos = expanded_result.find('$', dollar_pos)) !=
                               std::string::npos) {
                            size_t var_start = dollar_pos + 1;
                            size_t var_end = var_start;
                            while (var_end < expanded_result.length() &&
                                   (std::isalnum(expanded_result[var_end]) ||
                                    expanded_result[var_end] == '_')) {
                                var_end++;
                            }
                            if (var_end > var_start) {
                                std::string var_name =
                                    expanded_result.substr(var_start, var_end - var_start);
                                std::string var_value = get_variable_value(var_name);
                                expanded_result.replace(dollar_pos, var_end - dollar_pos,
                                                        var_value);
                                dollar_pos += var_value.length();
                            } else {
                                dollar_pos++;
                            }
                        }
                    }

                    out += expanded_result;
                    i = j;
                    continue;
                } else {
                    throw std::runtime_error("syntax error near unexpected token '{'");
                }
            }
        }

        out += c;
    }

    return out;
}

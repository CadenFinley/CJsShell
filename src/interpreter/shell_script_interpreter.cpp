#include "shell_script_interpreter.h"
#include "shell_script_interpreter_error_reporter.h"

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
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
#include <utility>

#include "arithmetic_evaluator.h"
#include "builtin.h"
#include "case_evaluator.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "command_substitution_evaluator.h"
#include "conditional_evaluator.h"
#include "error_out.h"
#include "exec.h"
#include "function_evaluator.h"
#include "job_control.h"
#include "loop_evaluator.h"
#include "parameter_expansion_evaluator.h"
#include "readonly_command.h"
#include "shell.h"
#include "shell_script_interpreter_utils.h"
#include "suggestion_utils.h"
#include "theme.h"

using shell_script_interpreter::detail::process_line_for_validation;
using shell_script_interpreter::detail::strip_inline_comment;
using shell_script_interpreter::detail::trim;

namespace {

std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool is_readable_file(const std::string& path) {
    struct stat st{};
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && access(path.c_str(), R_OK) == 0;
}

}  // namespace

ShellScriptInterpreter::ShellScriptInterpreter() : shell_parser(nullptr) {
}

ShellScriptInterpreter::~ShellScriptInterpreter() = default;

namespace {
std::string execute_command_for_substitution(
    const std::string& command, const std::function<int(const std::string&)>& executor) {
    char tmpl[] = "/tmp/cjsh_subst_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd >= 0)
        close(fd);
    std::string path = tmpl;

    int saved_stdout = dup(STDOUT_FILENO);

    auto temp_file_result = cjsh_filesystem::safe_fopen(path, "w");
    if (temp_file_result.is_error()) {
        int pipefd[2];
        if (pipe(pipefd) != 0) {
            return "";
        }

        pid_t pid = fork();
        if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);

            int exit_code = executor(command);
            exit(exit_code);
        } else if (pid > 0) {
            close(pipefd[1]);
            std::string result;
            char buf[4096];
            ssize_t n = 0;
            while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
                result.append(buf, n);
            }
            close(pipefd[0]);

            int status = 0;
            waitpid(pid, &status, 0);

            while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
                result.pop_back();

            return result;
        } else {
            close(pipefd[0]);
            close(pipefd[1]);
            return "";
        }
    }

    FILE* temp_file = temp_file_result.value();
    int temp_fd = fileno(temp_file);
    auto dup_result = cjsh_filesystem::safe_dup2(temp_fd, STDOUT_FILENO);
    if (dup_result.is_error()) {
        cjsh_filesystem::safe_fclose(temp_file);
        cjsh_filesystem::safe_close(saved_stdout);
        return "";
    }

    executor(command);

    (void)fflush(stdout);
    cjsh_filesystem::safe_fclose(temp_file);
    auto restore_result = cjsh_filesystem::safe_dup2(saved_stdout, STDOUT_FILENO);
    cjsh_filesystem::safe_close(saved_stdout);

    auto content_result = cjsh_filesystem::read_file_content(path);
    cjsh_filesystem::cleanup_temp_file(path);

    if (content_result.is_error()) {
        return "";
    }

    std::string out = content_result.value();

    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();

    return out;
}
}  // namespace

int ShellScriptInterpreter::execute_block(const std::vector<std::string>& lines) {
    if (g_shell == nullptr) {
        print_error({ErrorType::RUNTIME_ERROR, "", "No shell instance available", {}});
    }

    if (shell_parser == nullptr) {
        print_error(
            {ErrorType::RUNTIME_ERROR, "", "Script interpreter not properly initialized", {}});
        return 1;
    }

    if (has_syntax_errors(lines)) {
        print_error({ErrorType::SYNTAX_ERROR,
                     "",
                     "Critical syntax errors detected in script block, process aborted",
                     {}});
        return 2;
    }

    std::function<int(const std::string&, bool)> execute_simple_or_pipeline_impl;
    std::function<int(const std::string&)> execute_simple_or_pipeline;

    std::function<int(const std::string&)> evaluate_logical_condition;

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

        if (allow_semicolon_split && text.find(';') != std::string::npos) {
            auto semicolon_commands = shell_parser->parse_semicolon_commands(text);

            if (semicolon_commands.size() > 1) {
                int last_code = 0;
                for (const auto& part : semicolon_commands) {
                    last_code = execute_simple_or_pipeline_impl(part, false);

                    if (g_shell && g_shell->is_errexit_enabled() && last_code != 0 &&
                        last_code != 253 && last_code != 254 && last_code != 255) {
                        return last_code;
                    }
                }
                return last_code;
            }
        }

        // Create command substitution evaluator with executor callback
        CommandSubstitutionEvaluator cmd_subst_evaluator(
            [&](const std::string& command) -> std::string {
                return execute_command_for_substitution(command, execute_simple_or_pipeline);
            });

        auto expand_substitutions = [&](const std::string& in) -> std::string {
            // First, expand command substitutions using the evaluator
            auto expansion_result = cmd_subst_evaluator.expand_substitutions(in);
            std::string result = expansion_result.text;

            // Now handle arithmetic expansion and parameter expansion that the
            // CommandSubstitutionEvaluator passes through
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

                // Handle arithmetic expansion $((expr))
                if (!in_quotes || q == '"') {
                    if (c == '$' && i + 2 < result.size() && result[i + 1] == '(' &&
                        result[i + 2] == '(') {
                        size_t inner_start = i + 3;
                        int depth = 1;
                        size_t j = inner_start;
                        bool found = false;

                        for (; j < result.size(); ++j) {
                            if (j + 1 < result.size() && result[j] == '(' &&
                                result[j - 1] != '\\') {
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

                            // Expand variables in arithmetic expression
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
                                        std::string var_name =
                                            expr.substr(var_start, var_end - var_start);
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
                                    } else {
                                        expanded_expr += expr[k];
                                    }
                                } else {
                                    expanded_expr += expr[k];
                                }
                            }

                            try {
                                out +=
                                    std::to_string(evaluate_arithmetic_expression(expanded_expr));
                            } catch (const std::runtime_error& e) {
                                shell_script_interpreter::print_runtime_error(
                                    "cjsh: " + std::string(e.what()), "$((" + expr + "))");
                                throw;
                            }
                            i = j + 1;  // Skip past the closing ))
                            continue;
                        }
                    }

                    // Handle ${parameter} expansion
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

                            // Re-expand any remaining variable references
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
                        }
                    }
                }

                out += c;
            }

            return out;
        };

        try {
            text = expand_substitutions(text);

            std::vector<std::string> head = shell_parser->parse_command(text);
            if (!head.empty()) {
                const std::string& prog = head[0];
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
                    prog.rfind("for ", 0) == 0 || prog == "while" || prog.rfind("while ", 0) == 0) {
                    std::vector<std::string> lines = {text};
                    return execute_block(lines);
                }
            }
        } catch (const std::runtime_error& e) {
            throw;
        }

        if ((text == "case" || text.rfind("case ", 0) == 0) &&
            (text.find(" in ") != std::string::npos) && (text.find("esac") == std::string::npos)) {
            std::string completed_case = text + ";; esac";

            return execute_simple_or_pipeline(completed_case);
        }

        auto pattern_matcher = [this](const std::string& text, const std::string& pattern) {
            return matches_pattern(text, pattern);
        };
        auto cmd_sub_expander = [this](const std::string& input) {
            auto exp = expand_command_substitutions(input);
            return std::make_pair(exp.text, exp.outputs);
        };

        if (auto inline_case_result = case_evaluator::handle_inline_case(
                text, execute_simple_or_pipeline, false, true, shell_parser, pattern_matcher,
                cmd_sub_expander)) {
            return *inline_case_result;
        }

        try {
            std::vector<Command> cmds = shell_parser->parse_pipeline_with_preprocessing(text);

            bool has_redir_or_pipe = cmds.size() > 1;
            if (!has_redir_or_pipe && !cmds.empty()) {
                const auto& c = cmds[0];
                has_redir_or_pipe =
                    c.background || !c.input_file.empty() || !c.output_file.empty() ||
                    !c.append_file.empty() || c.stderr_to_stdout || !c.stderr_file.empty() ||
                    !c.here_doc.empty() || c.both_output || !c.here_string.empty() ||
                    !c.fd_redirections.empty() || !c.fd_duplications.empty();
            }

            if (!has_redir_or_pipe && !cmds.empty()) {
                const auto& c = cmds[0];

                if (!c.args.empty() && c.args[0] == "__INTERNAL_SUBSHELL__") {
                    bool has_redir = c.stderr_to_stdout || c.stdout_to_stderr ||
                                     !c.input_file.empty() || !c.output_file.empty() ||
                                     !c.append_file.empty() || !c.stderr_file.empty() ||
                                     !c.here_doc.empty();

                    if (has_redir) {
                        return run_pipeline(cmds);
                    }
                    if (c.args.size() >= 2) {
                        std::string subshell_content = c.args[1];

                        pid_t pid = fork();
                        if (pid == 0) {
                            if (setpgid(0, 0) < 0) {
                                perror(
                                    "cjsh: setpgid failed in subshell "
                                    "child");
                            }

                            int exit_code = g_shell->execute(subshell_content);

                            const char* exit_code_str = getenv("EXIT_CODE");
                            if (exit_code_str) {
                                exit_code = std::atoi(exit_code_str);
                                unsetenv("EXIT_CODE");
                            }

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
                            std::cerr << "Failed to fork for subshell execution" << '\n';
                            return 1;
                        }
                    } else {
                        return 1;
                    }

                } else {
                    std::vector<std::string> expanded_args = shell_parser->parse_command(text);
                    if (expanded_args.empty())
                        return 0;

                    if (expanded_args.size() == 2 && expanded_args[0] == "__ALIAS_PIPELINE__") {
                        std::vector<Command> pipeline_cmds =
                            shell_parser->parse_pipeline_with_preprocessing(expanded_args[1]);
                        return run_pipeline(pipeline_cmds);
                    }

                    if (expanded_args.size() == 1) {
                        std::string var_name;
                        std::string var_value;
                        if (shell_parser->is_env_assignment(expanded_args[0], var_name,
                                                            var_value)) {
                            shell_parser->expand_env_vars(var_value);

                            if (g_shell) {
                                auto& env_vars = g_shell->get_env_vars();
                                env_vars[var_name] = var_value;

                                if (var_name == "PATH" || var_name == "PWD" || var_name == "HOME" ||
                                    var_name == "USER" || var_name == "SHELL") {
                                    setenv(var_name.c_str(), var_value.c_str(), 1);
                                }

                                if (shell_parser) {
                                    shell_parser->set_env_vars(env_vars);
                                }
                            }

                            return 0;
                        }
                    }

                    if (!expanded_args.empty() && g_shell && g_shell->get_built_ins() &&
                        !g_shell->get_built_ins()->is_builtin_command(expanded_args[0])) {
                        bool is_function = functions.count(expanded_args[0]) > 0;

                        if (!is_function) {
                            std::vector<std::string> external_args =
                                shell_parser->parse_command_exported_vars_only(text);
                            if (!external_args.empty()) {
                                expanded_args = external_args;
                            }
                        }
                    }

                    if (!expanded_args.empty() && functions.count(expanded_args[0])) {
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

                        if (exit_code == 253) {
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
                    int exit_code = g_shell->execute_command(expanded_args, c.background);
                    return set_last_status(exit_code);
                }
            }

            if (cmds.empty())
                return 0;
            return run_pipeline(cmds);
        } catch (const std::bad_alloc& e) {
            std::vector<SyntaxError> errors;
            SyntaxError error(1, "Memory allocation failed", text);
            error.severity = ErrorSeverity::ERROR;
            error.category = ErrorCategory::COMMANDS;
            error.error_code = "MEM001";
            error.suggestion = "Command may be too complex or system is low on memory";
            errors.push_back(error);

            shell_script_interpreter::print_error_report(errors, true, true);

            return set_last_status(3);
        } catch (const std::system_error& e) {
            std::vector<SyntaxError> errors;
            SyntaxError error(1, "System error: " + std::string(e.what()), text);
            error.severity = ErrorSeverity::ERROR;
            error.category = ErrorCategory::COMMANDS;
            error.error_code = "SYS001";
            error.suggestion = "Check system resources and permissions";
            errors.push_back(error);

            shell_script_interpreter::print_error_report(errors, true, true);

            return set_last_status(4);
        } catch (const std::runtime_error& e) {
            std::vector<SyntaxError> errors;
            SyntaxError error(1, e.what(), text);
            std::string error_msg = e.what();

            if (error_msg.find("command not found: ") != std::string::npos) {
                size_t pos = error_msg.find("command not found: ");
                if (pos != std::string::npos) {
                    std::string command_name = error_msg.substr(pos + 19);

                    auto suggestions = suggestion_utils::generate_command_suggestions(command_name);

                    error.message = "cjsh: command not found: " + command_name;
                    error.severity = ErrorSeverity::ERROR;
                    error.category = ErrorCategory::COMMANDS;
                    error.error_code = "RUN001";

                    if (!suggestions.empty()) {
                        std::string suggestion_text;

                        std::vector<std::string> commands;
                        for (const auto& suggestion : suggestions) {
                            if (suggestion.find("Did you mean") != std::string::npos) {
                                size_t start = suggestion.find("'");
                                if (start != std::string::npos) {
                                    start++;
                                    size_t end = suggestion.find("'", start);
                                    if (end != std::string::npos) {
                                        commands.push_back(suggestion.substr(start, end - start));
                                    }
                                }
                            }
                        }

                        if (!commands.empty()) {
                            suggestion_text = "Did you mean: ";
                            for (size_t i = 0; i < commands.size(); ++i) {
                                suggestion_text += commands[i];
                                if (i < commands.size() - 1) {
                                    suggestion_text += ", ";
                                }
                            }
                            suggestion_text += "?";
                        } else {
                            suggestion_text = suggestions.empty() ? "Check command syntax and "
                                                                    "system resources"
                                                                  : suggestions[0];
                        }

                        error.suggestion = suggestion_text;
                    } else {
                        error.suggestion = "Check command syntax and system resources";
                    }
                } else {
                    error.severity = ErrorSeverity::ERROR;
                    error.category = ErrorCategory::COMMANDS;
                    error.error_code = "RUN001";
                    error.suggestion = "Check command syntax and system resources";
                }

                errors.push_back(error);
                shell_script_interpreter::print_error_report(errors, true, true);

                return set_last_status(127);
            }
            if (error_msg.find("Unclosed quote") != std::string::npos ||
                error_msg.find("missing closing") != std::string::npos ||
                error_msg.find("syntax error near unexpected token") != std::string::npos) {
                error.severity = ErrorSeverity::ERROR;
                error.category = ErrorCategory::SYNTAX;
                error.error_code = "SYN001";
                if (error_msg.find("syntax error near unexpected token") != std::string::npos) {
                    error.suggestion =
                        "Check for incomplete redirections or missing command "
                        "arguments";
                } else {
                    error.suggestion = "Make sure all quotes are properly closed";
                }
            } else if (error_msg.find("Failed to open") != std::string::npos ||
                       error_msg.find("Failed to redirect") != std::string::npos ||
                       error_msg.find("Failed to write") != std::string::npos) {
                error.severity = ErrorSeverity::ERROR;
                error.category = ErrorCategory::REDIRECTION;
                error.error_code = "IO001";
                error.suggestion = "Check file permissions and paths";
            } else {
                error.severity = ErrorSeverity::ERROR;
                error.category = ErrorCategory::COMMANDS;
                error.error_code = "RUN001";
                error.suggestion = "Check command syntax and system resources";
            }

            errors.push_back(error);
            shell_script_interpreter::print_error_report(errors, true, true);

            return set_last_status(2);
        } catch (const std::exception& e) {
            std::vector<SyntaxError> errors;
            SyntaxError error(1, "Unexpected error: " + std::string(e.what()), text);
            error.severity = ErrorSeverity::ERROR;
            error.category = ErrorCategory::COMMANDS;
            error.error_code = "UNK001";
            error.suggestion =
                "An unexpected error occurred, please report this as an issue, "
                "and "
                "how to replicate it.";
            errors.push_back(error);

            shell_script_interpreter::print_error_report(errors, true, true);

            return set_last_status(5);
        } catch (...) {
            std::vector<SyntaxError> errors;
            SyntaxError error(1, "Unknown error occurred", text);
            error.severity = ErrorSeverity::ERROR;
            error.category = ErrorCategory::COMMANDS;
            error.error_code = "UNK002";
            error.suggestion =
                "An unexpected error occurred, please report this as an issue, "
                "and "
                "how to replicate it.";
            errors.push_back(error);

            shell_script_interpreter::print_error_report(errors, true, true);

            return set_last_status(6);
        }
    };

    int last_code = 0;

    auto handle_if_block = [&](const std::vector<std::string>& src_lines, size_t& idx) -> int {
        auto execute_block_wrapper = [&](const std::vector<std::string>& block_lines) -> int {
            return execute_block(block_lines);
        };

        return conditional_evaluator::handle_if_block(src_lines, idx, execute_block_wrapper,
                                                      execute_simple_or_pipeline,
                                                      evaluate_logical_condition, shell_parser);
    };

    auto handle_for_block = [&](const std::vector<std::string>& src_lines, size_t& idx) -> int {
        auto execute_block_wrapper = [&](const std::vector<std::string>& block_lines) -> int {
            return execute_block(block_lines);
        };

        return loop_evaluator::handle_for_block(src_lines, idx, execute_block_wrapper,
                                                shell_parser);
    };

    auto handle_case_block = [&](const std::vector<std::string>& src_lines, size_t& idx) -> int {
        std::string first = trim(strip_inline_comment(src_lines[idx]));
        if (first != "case" && first.rfind("case ", 0) != 0)
            return 1;

        auto pattern_matcher = [this](const std::string& text, const std::string& pattern) {
            return matches_pattern(text, pattern);
        };
        auto cmd_sub_expander = [this](const std::string& input) {
            auto exp = expand_command_substitutions(input);
            return std::make_pair(exp.text, exp.outputs);
        };

        if (auto inline_case_result = case_evaluator::handle_inline_case(
                first, execute_simple_or_pipeline, true, true, shell_parser, pattern_matcher,
                cmd_sub_expander)) {
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
            auto expansion = expand_command_substitutions(header_accum);
            expanded_header = expansion.text;
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

        std::string case_value = raw_case_value;
        if (case_value.length() >= 2) {
            if ((case_value.front() == '"' && case_value.back() == '"') ||
                (case_value.front() == '\'' && case_value.back() == '\''))
                case_value = case_value.substr(1, case_value.length() - 2);
        }

        if (!case_value.empty())
            shell_parser->expand_env_vars(case_value);

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

        auto case_result = case_evaluator::evaluate_case_patterns(combined_patterns, case_value,
                                                                  false, execute_simple_or_pipeline,
                                                                  shell_parser, pattern_matcher);
        idx = esac_index;
        return case_result.first ? case_result.second : 0;
    };

    auto handle_while_block = [&](const std::vector<std::string>& src_lines, size_t& idx) -> int {
        auto execute_block_wrapper = [&](const std::vector<std::string>& block_lines) -> int {
            return execute_block(block_lines);
        };

        return loop_evaluator::handle_while_block(src_lines, idx, execute_block_wrapper,
                                                  execute_simple_or_pipeline, shell_parser);
    };

    auto handle_until_block = [&](const std::vector<std::string>& src_lines, size_t& idx) -> int {
        auto execute_block_wrapper = [&](const std::vector<std::string>& block_lines) -> int {
            return execute_block(block_lines);
        };

        return loop_evaluator::handle_until_block(src_lines, idx, execute_block_wrapper,
                                                  execute_simple_or_pipeline, shell_parser);
    };

    for (size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const auto& raw_line = lines[line_index];
        std::string line = trim(strip_inline_comment(raw_line));

        if (line.empty()) {
            continue;
        }

        if (line == "fi" || line == "then" || line == "else" || line == "done" || line == "esac" ||
            line == "}" || line == ";;") {
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
            continue;
        }

        if (line.rfind("theme_definition", 0) == 0) {
            size_t block_index = line_index;
            int brace_depth = 0;
            bool seen_open_brace = false;
            bool in_string = false;
            char string_delim = '\0';
            bool escape_next = false;
            std::string theme_block;

            for (; block_index < lines.size(); ++block_index) {
                const std::string& block_line = lines[block_index];
                theme_block.append(block_line);
                theme_block.push_back('\n');

                for (char ch : block_line) {
                    if (escape_next) {
                        escape_next = false;
                        continue;
                    }

                    if (in_string) {
                        if (ch == '\\') {
                            escape_next = true;
                            continue;
                        }
                        if (ch == string_delim) {
                            in_string = false;
                        }
                        continue;
                    }

                    if (ch == '"' || ch == '\'') {
                        in_string = true;
                        string_delim = ch;
                        continue;
                    }

                    if (ch == '{') {
                        brace_depth++;
                        seen_open_brace = true;
                    } else if (ch == '}') {
                        if (brace_depth > 0) {
                            brace_depth--;
                        }
                    }
                }

                if (seen_open_brace && brace_depth == 0) {
                    break;
                }
            }

            if (!seen_open_brace || brace_depth != 0) {
                print_error(
                    {ErrorType::SYNTAX_ERROR,
                     "theme",
                     "Inline theme block missing closing '}'",
                     {"Ensure theme_definition blocks in configuration files are complete."}});
                last_code = 1;
                line_index = block_index;
                continue;
            }

            if (!config::themes_enabled) {
                print_error({ErrorType::RUNTIME_ERROR,
                             "theme",
                             "Themes are disabled",
                             {"Enable themes in configuration or remove inline theme blocks."}});
                last_code = 1;
                line_index = block_index;
                continue;
            }

            if (!g_theme) {
                initialize_themes();
            }

            if (!g_theme) {
                print_error({ErrorType::RUNTIME_ERROR,
                             "theme",
                             "Theme manager not initialized",
                             {"Try running 'theme' again after initialization completes."}});
                last_code = 1;
                line_index = block_index;
                continue;
            }

            std::string label = "inline_theme_line_" + std::to_string(line_index + 1);
            bool loaded = g_theme->load_theme_from_string(theme_block, label, true);
            last_code = loaded ? 0 : 2;
            line_index = block_index;
            continue;
        }

        if (line.find("()") != std::string::npos && line.find('{') != std::string::npos) {
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

                bool is_control_flow = (last_code == 253 || last_code == 254 || last_code == 255);
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
                for (size_t si = 0; si < segs.size(); ++si) {
                    const std::string& cmd_text = segs[si];

                    if (g_shell != nullptr && g_shell->get_shell_option("verbose")) {
                        std::string verbose_text = trim(strip_inline_comment(cmd_text));
                        if (!verbose_text.empty()) {
                            std::cerr << verbose_text << '\n';
                        }
                    }

                    std::string t = trim(strip_inline_comment(cmd_text));

                    if (t.find("()") != std::string::npos && t.find('{') != std::string::npos) {
                        size_t name_end = t.find("()");
                        size_t brace_pos = t.find('{');
                        if (name_end != std::string::npos && brace_pos != std::string::npos &&
                            name_end < brace_pos) {
                            std::string func_name = trim(t.substr(0, name_end));
                            if (!func_name.empty() && func_name.find(' ') == std::string::npos) {
                                std::vector<std::string> body_lines;
                                std::string after_brace = trim(t.substr(brace_pos + 1));
                                if (!after_brace.empty()) {
                                    size_t end_brace = after_brace.find('}');
                                    if (end_brace != std::string::npos) {
                                        std::string body_part =
                                            trim(after_brace.substr(0, end_brace));
                                        if (!body_part.empty())
                                            body_lines.push_back(body_part);
                                        functions[func_name] = body_lines;
                                        last_code = 0;
                                        continue;
                                    }
                                }
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
                        std::vector<std::string> first_toks = shell_parser->parse_command(cmd_text);
                        if (!first_toks.empty() && (functions.count(first_toks[0]) != 0u)) {
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

                            if (code == 253) {
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

                    if (g_shell && g_shell->is_errexit_enabled() && code != 0) {
                        if (code != 253 && code != 254 && code != 255) {
                            return code;
                        }
                    }

                    if (!is_function_call && (code == 253 || code == 254 || code == 255)) {
                        goto control_flow_exit;
                    }
                }
            }
        }

    control_flow_exit:

        if (last_code == 127) {
            return last_code;
        } else if (last_code == 253 || last_code == 254 || last_code == 255) {
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
    if (candidate.has_extension()) {
        std::string ext_lower = to_lower_copy(candidate.extension().string());
        std::string theme_ext = to_lower_copy(std::string(Theme::kThemeFileExtension));
        if (ext_lower == theme_ext)
            return true;
    }

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

std::optional<size_t> ShellScriptInterpreter::find_matching_paren(const std::string& text,
                                                                  size_t start_index) {
    int depth = 1;
    for (size_t i = start_index; i < text.size(); ++i) {
        if (text[i] == '(') {
            depth++;
        } else if (text[i] == ')') {
            depth--;
            if (depth == 0)
                return i;
        }
    }
    return std::nullopt;
}

ShellScriptInterpreter::CommandSubstitutionExpansion
ShellScriptInterpreter::expand_command_substitutions(const std::string& input) const {
    CommandSubstitutionExpansion info{input, {}};
    std::string text = input;
    size_t search_pos = 0;
    while (true) {
        size_t start = text.find("$(", search_pos);
        if (start == std::string::npos)
            break;
        auto end_opt = find_matching_paren(text, start + 2);
        if (!end_opt)
            break;
        size_t end = *end_opt;
        std::string command = text.substr(start + 2, end - start - 2);
        auto cmd_output = cjsh_filesystem::read_command_output(command);
        if (!cmd_output.is_ok()) {
            search_pos = end + 1;
            continue;
        }
        std::string result = cmd_output.value();
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
        info.outputs.push_back(result);

        std::string new_text;
        new_text.reserve(text.size() - (end - start) + result.size());
        new_text.append(text, 0, start);
        new_text.append(result);
        new_text.append(text, end + 1, std::string::npos);
        text = std::move(new_text);

        search_pos = start + result.size();
    }
    info.text = text;
    return info;
}

std::string ShellScriptInterpreter::simplify_parentheses_in_condition(
    const std::string& condition, const std::function<int(const std::string&)>& evaluator) const {
    std::string result = condition;

    while (true) {
        size_t start = std::string::npos;
        size_t end = std::string::npos;
        int depth = 0;
        bool in_quotes = false;
        char quote_char = '\0';
        bool escaped = false;

        for (size_t i = 0; i < result.length(); ++i) {
            char c = result[i];

            if (escaped) {
                escaped = false;
                continue;
            }

            if (c == '\\') {
                escaped = true;
                continue;
            }

            if (!in_quotes) {
                if (c == '"' || c == '\'' || c == '`') {
                    in_quotes = true;
                    quote_char = c;
                    continue;
                }
                if (c == '(') {
                    if (depth == 0) {
                        start = i;
                    }
                    depth++;
                } else if (c == ')') {
                    if (depth > 0) {
                        depth--;
                        if (depth == 0 && start != std::string::npos) {
                            end = i;
                            break;
                        }
                    }
                }
            } else {
                if (c == quote_char) {
                    in_quotes = false;
                    quote_char = '\0';
                }
            }
        }

        if (start == std::string::npos || end == std::string::npos) {
            break;
        }

        std::string inner = result.substr(start + 1, end - start - 1);
        int inner_result = evaluator(inner);
        std::string replacement = (inner_result == 0) ? "true" : "false";

        std::string temp = result.substr(0, start);
        temp.append(replacement);
        temp.append(result.substr(end + 1));
        result = std::move(temp);
    }

    return result;
}

int ShellScriptInterpreter::evaluate_logical_condition_internal(
    const std::string& condition, const std::function<int(const std::string&)>& executor) {
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
                long long result =
                    shell_parser != nullptr ? shell_parser->evaluate_arithmetic(expr) : 0;
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

    cond = processed_cond;

    std::function<int(const std::string&)> self_eval = [&](const std::string& inner) -> int {
        return evaluate_logical_condition_internal(inner, executor);
    };

    cond = simplify_parentheses_in_condition(cond, self_eval);

    bool has_logical_ops = false;
    bool in_quotes = false;
    char quote_char = '\0';
    bool escaped = false;
    int bracket_depth = 0;
    int paren_depth = 0;

    for (size_t i = 0; i + 1 < cond.length(); ++i) {
        char c = cond[i];

        if (escaped) {
            escaped = false;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            continue;
        }

        if (!in_quotes) {
            if (c == '"' || c == '\'' || c == '`') {
                in_quotes = true;
                quote_char = c;
                continue;
            }
            if (c == '[') {
                bracket_depth++;
            } else if (c == ']') {
                bracket_depth--;
            } else if (c == '(') {
                paren_depth++;
            } else if (c == ')') {
                paren_depth--;
            }
        } else {
            if (c == quote_char) {
                in_quotes = false;
                quote_char = '\0';
            }
            continue;
        }

        if (!in_quotes && bracket_depth == 0 && paren_depth == 0) {
            if ((cond[i] == '&' && cond[i + 1] == '&') || (cond[i] == '|' && cond[i + 1] == '|')) {
                has_logical_ops = true;
                break;
            }
        }
    }

    if (!has_logical_ops) {
        return executor(cond);
    }

    std::vector<std::pair<std::string, std::string>> parts;
    std::string current_part;
    in_quotes = false;
    quote_char = '\0';
    escaped = false;
    bracket_depth = 0;
    paren_depth = 0;

    for (size_t i = 0; i < cond.length(); ++i) {
        char c = cond[i];

        if (escaped) {
            current_part += c;
            escaped = false;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            current_part += c;
            continue;
        }

        if (!in_quotes) {
            if (c == '"' || c == '\'' || c == '`') {
                in_quotes = true;
                quote_char = c;
                current_part += c;
                continue;
            }
            if (c == '[') {
                bracket_depth++;
            } else if (c == ']') {
                bracket_depth--;
            } else if (c == '(') {
                paren_depth++;
            } else if (c == ')') {
                paren_depth--;
            }
        } else {
            if (c == quote_char) {
                in_quotes = false;
                quote_char = '\0';
            }
            current_part += c;
            continue;
        }

        if (!in_quotes && bracket_depth == 0 && paren_depth == 0 && i + 1 < cond.length()) {
            if (cond[i] == '&' && cond[i + 1] == '&') {
                parts.push_back({trim(current_part), "&&"});
                current_part.clear();
                ++i;
                continue;
            }
            if (cond[i] == '|' && cond[i + 1] == '|') {
                parts.push_back({trim(current_part), "||"});
                current_part.clear();
                ++i;
                continue;
            }
        }

        current_part += c;
    }

    if (!current_part.empty()) {
        parts.push_back({trim(current_part), ""});
    }

    if (parts.empty())
        return 1;

    int result = executor(parts[0].first);

    for (size_t i = 1; i < parts.size(); ++i) {
        const std::string& op = parts[i - 1].second;
        const std::string& cond_part = parts[i].first;

        if (op == "&&") {
            if (result != 0) {
                break;
            }
        } else if (op == "||") {
            if (result == 0) {
                break;
            }
        }

        result = executor(cond_part);
    }

    return result;
}

long long ShellScriptInterpreter::evaluate_arithmetic_expression(const std::string& expr) {
    // Create callbacks for variable read/write operations
    auto var_reader = [](const std::string& name) -> long long {
        if (g_shell) {
            const auto& env_vars = g_shell->get_env_vars();
            auto it = env_vars.find(name);
            if (it != env_vars.end()) {
                try {
                    return std::stoll(it->second);
                } catch (...) {
                    return 0;
                }
            }
        }

        const char* env_val = getenv(name.c_str());
        if (env_val) {
            try {
                return std::stoll(env_val);
            } catch (...) {
                return 0;
            }
        }
        return 0;
    };

    auto var_writer = [this](const std::string& name, long long value) {
        std::string value_str = std::to_string(value);

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

    // Use the dedicated arithmetic evaluator
    ArithmeticEvaluator evaluator(var_reader, var_writer);
    return evaluator.evaluate(expr);
}

int ShellScriptInterpreter::set_last_status(int code) {
    std::string value = std::to_string(code);
    setenv("?", value.c_str(), 1);
    return code;
}

int ShellScriptInterpreter::run_pipeline(const std::vector<Command>& cmds) {
    if (!g_shell || !g_shell->shell_exec)
        return set_last_status(1);

    int exit_code = g_shell->shell_exec->execute_pipeline(cmds);
    if (exit_code != 0) {
        ErrorInfo error = g_shell->shell_exec->get_error();
        if (error.type != ErrorType::RUNTIME_ERROR ||
            error.message.find("command failed with exit code") == std::string::npos) {
            g_shell->shell_exec->print_last_error();
        }
    }
    return set_last_status(exit_code);
}

std::string ShellScriptInterpreter::expand_parameter_expression(const std::string& param_expr) {
    // Create callbacks for the parameter expansion evaluator
    auto var_reader = [this](const std::string& name) -> std::string {
        return get_variable_value(name);
    };

    auto var_writer = [](const std::string& name, const std::string& value) {
        if (readonly_manager_is(name)) {
            std::cerr << "cjsh: " << name << ": readonly variable" << '\n';
            return;
        }

        if (g_shell) {
            auto& env_vars = g_shell->get_env_vars();
            env_vars[name] = value;

            if (name == "PATH" || name == "PWD" || name == "HOME" || name == "USER" ||
                name == "SHELL") {
                setenv(name.c_str(), value.c_str(), 1);
            }
        }
    };

    auto var_checker = [this](const std::string& name) -> bool { return variable_is_set(name); };

    auto pattern_matcher = [this](const std::string& text, const std::string& pattern) -> bool {
        return matches_pattern(text, pattern);
    };

    // Create the evaluator and delegate to it
    ParameterExpansionEvaluator evaluator(var_reader, var_writer, var_checker, pattern_matcher);
    return evaluator.expand(param_expr);
}

std::string ShellScriptInterpreter::get_variable_value(const std::string& var_name) {
    if (!local_variable_stack.empty()) {
        auto& current_scope = local_variable_stack.back();
        auto it = current_scope.find(var_name);
        if (it != current_scope.end()) {
            return it->second;
        }
    }

    if (var_name == "?") {
        const char* status_env = getenv("?");
        return (status_env != nullptr) ? status_env : "0";
    }
    if (var_name == "$") {
        return std::to_string(getpid());
    }
    if (var_name == "#") {
        if (g_shell) {
            return std::to_string(g_shell->get_positional_parameter_count());
        }
        return "0";
    }
    if (var_name == "*" || var_name == "@") {
        if (g_shell) {
            auto params = g_shell->get_positional_parameters();
            std::string result;
            for (size_t i = 0; i < params.size(); ++i) {
                if (i > 0)
                    result += " ";
                result += params[i];
            }
            return result;
        }
        return "";
    }
    if (var_name == "!") {
        const char* last_bg_pid = getenv("!");
        if (last_bg_pid != nullptr) {
            return last_bg_pid;
        }
        pid_t last_pid = JobManager::instance().get_last_background_pid();
        if (last_pid > 0) {
            return std::to_string(last_pid);
        }
        return "";
    }
    if (var_name.length() == 1 && (isdigit(var_name[0]) != 0)) {
        const char* env_val = getenv(var_name.c_str());
        if (env_val != nullptr) {
            return env_val;
        }

        int param_num = var_name[0] - '0';
        if (g_shell && param_num > 0) {
            auto params = g_shell->get_positional_parameters();
            if (static_cast<size_t>(param_num - 1) < params.size()) {
                return params[param_num - 1];
            }
        }
        return "";
    }

    if (g_shell) {
        const auto& env_vars = g_shell->get_env_vars();
        auto it = env_vars.find(var_name);
        if (it != env_vars.end()) {
            return it->second;
        }
    }

    const char* env_val = getenv(var_name.c_str());
    return (env_val != nullptr) ? env_val : "";
}

bool ShellScriptInterpreter::variable_is_set(const std::string& var_name) {
    if (!local_variable_stack.empty()) {
        auto& current_scope = local_variable_stack.back();
        if (current_scope.find(var_name) != current_scope.end()) {
            return true;
        }
    }

    if (var_name == "?" || var_name == "$" || var_name == "#" || var_name == "*" ||
        var_name == "@" || var_name == "!") {
        return true;
    }
    if (var_name.length() == 1 && (isdigit(var_name[0]) != 0)) {
        if (getenv(var_name.c_str()) != nullptr) {
            return true;
        }

        int param_num = var_name[0] - '0';
        if (g_shell && param_num > 0) {
            auto params = g_shell->get_positional_parameters();
            return static_cast<size_t>(param_num - 1) < params.size();
        }
        return false;
    }

    if (g_shell) {
        const auto& env_vars = g_shell->get_env_vars();
        if (env_vars.find(var_name) != env_vars.end()) {
            return true;
        }
    }

    return getenv(var_name.c_str()) != nullptr;
}

bool ShellScriptInterpreter::matches_char_class(char c, const std::string& char_class) {
    if (char_class.length() < 3 || char_class[0] != '[' || char_class.back() != ']') {
        return false;
    }

    std::string class_content = char_class.substr(1, char_class.length() - 2);
    bool negated = false;

    if (!class_content.empty() && (class_content[0] == '^' || class_content[0] == '!')) {
        negated = true;
        class_content = class_content.substr(1);
    }

    bool matches = false;

    for (size_t i = 0; i < class_content.length(); ++i) {
        if (i + 2 < class_content.length() && class_content[i + 1] == '-') {
            char start = class_content[i];
            char end = class_content[i + 2];
            if (c >= start && c <= end) {
                matches = true;
                break;
            }
            i += 2;
        } else {
            if (c == class_content[i]) {
                matches = true;
                break;
            }
        }
    }

    return negated ? !matches : matches;
}

bool ShellScriptInterpreter::matches_pattern(const std::string& text, const std::string& pattern) {
    if (pattern.find('|') != std::string::npos) {
        size_t start = 0;
        while (start < pattern.length()) {
            size_t pipe_pos = pattern.find('|', start);
            std::string sub_pattern;
            if (pipe_pos == std::string::npos) {
                sub_pattern = pattern.substr(start);
                start = pattern.length();
            } else {
                sub_pattern = pattern.substr(start, pipe_pos - start);
                start = pipe_pos + 1;
            }

            if (matches_pattern(text, sub_pattern)) {
                return true;
            }
        }
        return false;
    }

    size_t ti = 0;
    size_t pi = 0;
    size_t star_idx = std::string::npos;
    size_t match_idx = 0;

    while (ti < text.length() || pi < pattern.length()) {
        if (ti >= text.length()) {
            while (pi < pattern.length() && pattern[pi] == '*') {
                pi++;
            }
            return pi == pattern.length();
        }

        if (pi >= pattern.length()) {
            if (star_idx != std::string::npos) {
                pi = star_idx + 1;
                ti = ++match_idx;
            } else {
                return false;
            }
        } else if (pattern[pi] == '[') {
            size_t class_end = pattern.find(']', pi);
            if (class_end != std::string::npos) {
                std::string char_class = pattern.substr(pi, class_end - pi + 1);
                if (matches_char_class(text[ti], char_class)) {
                    ti++;
                    pi = class_end + 1;
                } else if (star_idx != std::string::npos) {
                    pi = star_idx + 1;
                    ti = ++match_idx;
                } else {
                    return false;
                }
            } else {
                if (pattern[pi] == text[ti]) {
                    ti++;
                    pi++;
                } else if (star_idx != std::string::npos) {
                    pi = star_idx + 1;
                    ti = ++match_idx;
                } else {
                    return false;
                }
            }
        } else if (pattern[pi] == '\\' && pi + 1 < pattern.length()) {
            char escaped_char = pattern[pi + 1];
            if (escaped_char == text[ti]) {
                ti++;
                pi += 2;
            } else if (star_idx != std::string::npos) {
                pi = star_idx + 1;
                ti = ++match_idx;
            } else {
                return false;
            }
        } else if (pattern[pi] == '?' || pattern[pi] == text[ti]) {
            ti++;
            pi++;
        } else if (pattern[pi] == '*') {
            star_idx = pi++;
            match_idx = ti;
        } else if (star_idx != std::string::npos) {
            pi = star_idx + 1;
            ti = ++match_idx;
        } else {
            return false;
        }
    }

    return true;
}

bool ShellScriptInterpreter::has_function(const std::string& name) const {
    return function_evaluator::has_function(functions, name);
}

std::vector<std::string> ShellScriptInterpreter::get_function_names() const {
    return function_evaluator::get_function_names(functions);
}

void ShellScriptInterpreter::push_function_scope() {
    function_evaluator::push_function_scope(local_variable_stack);
}

void ShellScriptInterpreter::pop_function_scope() {
    function_evaluator::pop_function_scope(local_variable_stack);
}

void ShellScriptInterpreter::set_local_variable(const std::string& name, const std::string& value) {
    auto set_global_var = [](const std::string& var_name, const std::string& var_value) {
        if (g_shell) {
            auto& env_vars = g_shell->get_env_vars();
            env_vars[var_name] = var_value;

            if (var_name == "PATH" || var_name == "PWD" || var_name == "HOME" ||
                var_name == "USER" || var_name == "SHELL") {
                setenv(var_name.c_str(), var_value.c_str(), 1);
            }
        }
    };

    function_evaluator::set_local_variable(local_variable_stack, name, value, set_global_var);
}

bool ShellScriptInterpreter::is_local_variable(const std::string& name) const {
    return function_evaluator::is_local_variable(local_variable_stack, name);
}

ShellScriptInterpreter::BlockHandlerResult ShellScriptInterpreter::try_dispatch_block_statement(
    const std::vector<std::string>& lines, size_t line_index, const std::string& line,
    const std::function<int(const std::vector<std::string>&, size_t&)>& handle_if_block,
    const std::function<int(const std::vector<std::string>&, size_t&)>& handle_for_block,
    const std::function<int(const std::vector<std::string>&, size_t&)>& handle_while_block,
    const std::function<int(const std::vector<std::string>&, size_t&)>& handle_until_block,
    const std::function<int(const std::vector<std::string>&, size_t&)>& handle_case_block) {
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

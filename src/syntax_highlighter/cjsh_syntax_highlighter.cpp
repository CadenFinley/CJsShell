#include "cjsh_syntax_highlighter.h"

#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>

#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "highlight_helpers.h"
#include "shell.h"
#include "token_classifier.h"
#include "token_constants.h"
#include "utils/quote_state.h"

namespace {

bool extract_next_token(const std::string& cmd, size_t& cursor, size_t& token_start,
                        size_t& token_end) {
    const size_t len = cmd.length();

    while (cursor < len && (std::isspace(static_cast<unsigned char>(cmd[cursor])) != 0)) {
        ++cursor;
    }

    if (cursor >= len) {
        return false;
    }

    size_t start = cursor;
    utils::QuoteState quote_state;

    while (cursor < len) {
        char ch = cmd[cursor];
        auto action = quote_state.consume_forward(ch);
        if (action == utils::QuoteAdvanceResult::Process && !quote_state.inside_quotes() &&
            (std::isspace(static_cast<unsigned char>(ch)) != 0)) {
            break;
        }
        ++cursor;
    }

    token_start = start;
    token_end = cursor;
    return true;
}

}  // namespace

void SyntaxHighlighter::initialize_syntax_highlighting() {
    if (config::syntax_highlighting_enabled) {
        ic_set_default_highlighter(SyntaxHighlighter::highlight, nullptr);
        ic_enable_highlight(true);
    } else {
        ic_set_default_highlighter(nullptr, nullptr);
        ic_enable_highlight(false);
    }
}

void SyntaxHighlighter::highlight(ic_highlight_env_t* henv, const char* input, void*) {
    using namespace token_classifier;
    using namespace highlight_helpers;
    using namespace token_constants;

    size_t len = std::strlen(input);
    if (len == 0)
        return;

    std::string sanitized_input(input, len);

    if (config::history_expansion_enabled) {
        highlight_history_expansions(henv, input, len);
    }

    bool in_quotes = false;
    char quote_char = '\0';
    bool escaped = false;

    size_t i = 0;
    while (i < len) {
        char c = input[i];

        if (escaped) {
            escaped = false;
            ++i;
            continue;
        }

        if (c == '\\' && (!in_quotes || quote_char != '\'')) {
            escaped = true;
            ++i;
            continue;
        }

        if ((c == '"' || c == '\'') && !in_quotes) {
            in_quotes = true;
            quote_char = c;
            ++i;
            continue;
        }

        if (c == quote_char && in_quotes) {
            in_quotes = false;
            quote_char = '\0';
            ++i;
            continue;
        }

        if (!in_quotes && c == '#') {
            size_t comment_end = i;
            while (comment_end < len && input[comment_end] != '\n' && input[comment_end] != '\r') {
                sanitized_input[comment_end] = ' ';
                comment_end++;
            }
            if (comment_end > i) {
                ic_highlight(henv, static_cast<long>(i), static_cast<long>(comment_end - i),
                             "cjsh-comment");
            }
            i = comment_end;
            continue;
        }

        ++i;
    }

    const char* analysis = sanitized_input.c_str();

    size_t func_name_start = 0;
    size_t func_name_end = 0;
    std::string input_str = sanitized_input;
    if (is_function_definition(input_str, func_name_start, func_name_end)) {
        ic_highlight(henv, static_cast<long>(func_name_start),
                     static_cast<long>(func_name_end - func_name_start),
                     "cjsh-function-definition");

        size_t paren_pos = input_str.find("()", func_name_end);
        if (paren_pos != std::string::npos && paren_pos < len) {
            ic_highlight(henv, static_cast<long>(paren_pos), 2L, "cjsh-function-definition");
        }

        size_t brace_pos = input_str.find('{');
        if (brace_pos != std::string::npos && brace_pos < len) {
            ic_highlight(henv, static_cast<long>(brace_pos), 1L, "cjsh-operator");
        }
        return;
    }

    const auto& comparison_ops = token_constants::comparison_operators();

    size_t pos = 0;
    while (pos < len) {
        size_t cmd_end = pos;
        utils::QuoteState cmd_quote_state;
        while (cmd_end < len) {
            char current = analysis[cmd_end];
            auto action = cmd_quote_state.consume_forward(current);
            if (action == utils::QuoteAdvanceResult::Process && !cmd_quote_state.inside_quotes()) {
                if ((cmd_end + 1 < len && analysis[cmd_end] == '&' &&
                     analysis[cmd_end + 1] == '&') ||
                    (cmd_end + 1 < len && analysis[cmd_end] == '|' &&
                     analysis[cmd_end + 1] == '|') ||
                    analysis[cmd_end] == '|' || analysis[cmd_end] == ';' ||
                    analysis[cmd_end] == '\n' || analysis[cmd_end] == '\r') {
                    break;
                }
            }
            cmd_end++;
        }

        size_t cmd_start = pos;
        while (cmd_start < cmd_end && (std::isspace((unsigned char)analysis[cmd_start]) != 0)) {
            cmd_start++;
        }

        if (cmd_start < cmd_end) {
            std::string cmd_str(analysis + cmd_start, cmd_end - cmd_start);

            size_t token_cursor = 0;
            size_t first_token_start = 0;
            size_t first_token_end = 0;
            std::string token;
            if (extract_next_token(cmd_str, token_cursor, first_token_start, first_token_end)) {
                token = cmd_str.substr(first_token_start, first_token_end - first_token_start);
            }

            bool is_sudo_command = (token == "sudo");

            if (!token.empty()) {
                bool handled_first_token = false;
                size_t absolute_token_start = cmd_start + first_token_start;
                size_t first_token_length = first_token_end - first_token_start;

                if (is_variable_reference(token)) {
                    highlight_variable_assignment(henv, input, absolute_token_start, token);
                    handled_first_token = true;
                }

                if (!handled_first_token && config::history_expansion_enabled &&
                    (token[0] == '!' || (token[0] == '^' && cmd_start == 0))) {
                    handled_first_token = true;
                }

                if (!handled_first_token &&
                    (token.rfind("./", 0) == 0 || token.rfind("../", 0) == 0 ||
                     token.rfind("~/", 0) == 0 || token.rfind("-/", 0) == 0 || token[0] == '/' ||
                     token.find('/') != std::string::npos)) {
                    std::string path_to_check = token;
                    if (token.rfind("~/", 0) == 0) {
                        path_to_check =
                            cjsh_filesystem::g_user_home_path().string() + token.substr(1);
                    } else if (token.rfind("-/", 0) == 0) {
                        std::string prev_dir = g_shell->get_previous_directory();
                        if (!prev_dir.empty()) {
                            path_to_check = prev_dir + token.substr(1);
                        }
                    } else if (token[0] != '/' && token.rfind("./", 0) != 0 &&
                               token.rfind("../", 0) != 0 && token.rfind("~/", 0) != 0 &&
                               token.rfind("-/", 0) != 0) {
                        path_to_check = std::filesystem::current_path().string() + "/" + token;
                    }
                    if (std::filesystem::exists(path_to_check)) {
                        ic_highlight(henv, static_cast<long>(absolute_token_start),
                                     static_cast<long>(first_token_length), "cjsh-system");
                    } else {
                        ic_highlight(henv, static_cast<long>(absolute_token_start),
                                     static_cast<long>(first_token_length), "cjsh-unknown-command");
                    }
                    handled_first_token = true;
                }

                if (!handled_first_token && g_shell != nullptr && g_shell->get_interactive_mode()) {
                    const auto& abbreviations = g_shell->get_abbreviations();
                    if (abbreviations.find(token) != abbreviations.end()) {
                        ic_highlight(henv, static_cast<long>(absolute_token_start),
                                     static_cast<long>(first_token_length), "cjsh-builtin");
                        handled_first_token = true;
                    }
                }

                if (!handled_first_token && is_shell_keyword(token)) {
                    ic_highlight(henv, static_cast<long>(absolute_token_start),
                                 static_cast<long>(first_token_length), "cjsh-keyword");
                    handled_first_token = true;
                } else if (!handled_first_token && is_shell_builtin(token)) {
                    ic_highlight(henv, static_cast<long>(absolute_token_start),
                                 static_cast<long>(first_token_length), "cjsh-builtin");
                    handled_first_token = true;
                }

                if (!handled_first_token) {
                    auto cmds = g_shell->get_available_commands();
                    if (cmds.find(token) != cmds.end()) {
                        ic_highlight(henv, static_cast<long>(absolute_token_start),
                                     static_cast<long>(first_token_length), "cjsh-builtin");
                    } else if (is_external_command(token)) {
                        ic_highlight(henv, static_cast<long>(absolute_token_start),
                                     static_cast<long>(first_token_length), "cjsh-system");
                    } else {
                        ic_highlight(henv, static_cast<long>(absolute_token_start),
                                     static_cast<long>(first_token_length), "cjsh-unknown-command");
                    }
                }
            }

            bool is_cd_command = (token == "cd");
            size_t arg_cursor = token_cursor;
            size_t arg_index = 0;
            size_t arg_start = 0;
            size_t arg_end = 0;

            while (extract_next_token(cmd_str, arg_cursor, arg_start, arg_end)) {
                size_t absolute_arg_start = cmd_start + arg_start;
                size_t arg_length = arg_end - arg_start;
                std::string arg = cmd_str.substr(arg_start, arg_length);

                if (is_redirection_operator(arg) || comparison_ops.count(arg) > 0) {
                    ic_highlight(henv, static_cast<long>(absolute_arg_start),
                                 static_cast<long>(arg_length), "cjsh-operator");
                }

                else if (is_variable_reference(arg)) {
                    highlight_variable_assignment(henv, input, absolute_arg_start, arg);
                }

                else if (arg == "((" || arg == "))") {
                    ic_highlight(henv, static_cast<long>(absolute_arg_start),
                                 static_cast<long>(arg_length), "cjsh-arithmetic");
                }

                else if (is_shell_keyword(arg)) {
                    ic_highlight(henv, static_cast<long>(absolute_arg_start),
                                 static_cast<long>(arg_length), "cjsh-keyword");
                }

                else if (is_option(arg)) {
                    ic_highlight(henv, static_cast<long>(absolute_arg_start),
                                 static_cast<long>(arg_length), "cjsh-option");
                }

                else if (is_numeric_literal(arg)) {
                    ic_highlight(henv, static_cast<long>(absolute_arg_start),
                                 static_cast<long>(arg_length), "cjsh-number");
                }

                else {
                    char quote_type = 0;
                    if (is_quoted_string(arg, quote_type)) {
                        ic_highlight(henv, static_cast<long>(absolute_arg_start),
                                     static_cast<long>(arg_length), "cjsh-string");
                    } else if (is_sudo_command && arg_index == 0) {
                        if (arg.rfind("./", 0) == 0) {
                            if (!std::filesystem::exists(arg) ||
                                !std::filesystem::is_regular_file(arg)) {
                                ic_highlight(henv, static_cast<long>(absolute_arg_start),
                                             static_cast<long>(arg_length), "cjsh-unknown-command");
                            } else {
                                ic_highlight(henv, static_cast<long>(absolute_arg_start),
                                             static_cast<long>(arg_length), "cjsh-system");
                            }
                        } else {
                            bool is_abbreviation = false;
                            if (g_shell != nullptr && g_shell->get_interactive_mode()) {
                                const auto& abbreviations = g_shell->get_abbreviations();
                                is_abbreviation = abbreviations.find(arg) != abbreviations.end();
                            }

                            auto cmds = g_shell->get_available_commands();
                            if (is_abbreviation || cmds.find(arg) != cmds.end() ||
                                is_shell_builtin(arg)) {
                                ic_highlight(henv, static_cast<long>(absolute_arg_start),
                                             static_cast<long>(arg_length), "cjsh-builtin");
                            } else if (is_external_command(arg)) {
                                ic_highlight(henv, static_cast<long>(absolute_arg_start),
                                             static_cast<long>(arg_length), "cjsh-system");
                            } else {
                                ic_highlight(henv, static_cast<long>(absolute_arg_start),
                                             static_cast<long>(arg_length), "cjsh-unknown-command");
                            }
                        }
                    } else if (is_cd_command && (arg == "~" || arg == "-")) {
                        ic_highlight(henv, static_cast<long>(absolute_arg_start),
                                     static_cast<long>(arg_length), "cjsh-path-exists");
                    } else if (is_glob_pattern(arg)) {
                        ic_highlight(henv, static_cast<long>(absolute_arg_start),
                                     static_cast<long>(arg_length), "cjsh-glob-pattern");
                    } else if (is_cd_command || arg[0] == '/' || arg.rfind("./", 0) == 0 ||
                               arg.rfind("../", 0) == 0 || arg.rfind("~/", 0) == 0 ||
                               arg.rfind("-/", 0) == 0 || arg.find('/') != std::string::npos) {
                        std::string path_to_check = arg;

                        if (arg.rfind("~/", 0) == 0) {
                            path_to_check =
                                cjsh_filesystem::g_user_home_path().string() + arg.substr(1);
                        } else if (arg.rfind("-/", 0) == 0) {
                            std::string prev_dir = g_shell->get_previous_directory();
                            if (!prev_dir.empty()) {
                                path_to_check = prev_dir + arg.substr(1);
                            }
                        } else if (is_cd_command && arg[0] != '/' && arg.rfind("./", 0) != 0 &&
                                   arg.rfind("../", 0) != 0 && arg.rfind("~/", 0) != 0 &&
                                   arg.rfind("-/", 0) != 0) {
                            path_to_check = std::filesystem::current_path().string() + "/" + arg;
                        }

                        if (std::filesystem::exists(path_to_check)) {
                            ic_highlight(henv, static_cast<long>(absolute_arg_start),
                                         static_cast<long>(arg_length), "cjsh-path-exists");
                        } else {
                            ic_highlight(henv, static_cast<long>(absolute_arg_start),
                                         static_cast<long>(arg_length), "cjsh-path-not-exists");
                        }
                    }
                }

                if (!is_variable_reference(arg) &&
                    (arg.find('$') != std::string::npos || arg.find('`') != std::string::npos)) {
                    highlight_quotes_and_variables(henv, input, absolute_arg_start, arg_length);
                }

                ++arg_index;
            }
        }

        pos = cmd_end;
        if (pos < len) {
            if (pos + 1 < len && ((analysis[pos] == '&' && analysis[pos + 1] == '&') ||
                                  (analysis[pos] == '|' && analysis[pos + 1] == '|') ||
                                  (analysis[pos] == '>' && analysis[pos + 1] == '>') ||
                                  (analysis[pos] == '<' && analysis[pos + 1] == '<') ||
                                  (analysis[pos] == '&' && analysis[pos + 1] == '>'))) {
                ic_highlight(henv, static_cast<long>(pos), 2, "cjsh-operator");
                pos += 2;
            } else if (analysis[pos] == '|' || analysis[pos] == ';' || analysis[pos] == '>' ||
                       analysis[pos] == '<' ||
                       (analysis[pos] == '&' && (pos == len - 1 || analysis[pos + 1] != '&'))) {
                ic_highlight(henv, static_cast<long>(pos), 1, "cjsh-operator");
                pos += 1;
            } else {
                if (analysis[pos] == '\r' && pos + 1 < len && analysis[pos + 1] == '\n') {
                    pos += 2;
                } else {
                    pos += 1;
                }
            }
        }
    }
}

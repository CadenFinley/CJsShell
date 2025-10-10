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

void SyntaxHighlighter::initialize_syntax_highlighting() {
    if (config::syntax_highlighting_enabled) {
        token_classifier::initialize_external_cache();
        ic_set_default_highlighter(SyntaxHighlighter::highlight, nullptr);
        ic_enable_highlight(true);
    } else {
        ic_set_default_highlighter(nullptr, nullptr);
        ic_enable_highlight(false);
    }
}

void SyntaxHighlighter::refresh_executables_cache() {
    token_classifier::refresh_executables_cache();
}

void SyntaxHighlighter::highlight(ic_highlight_env_t* henv, const char* input, void*) {
    using namespace token_classifier;
    using namespace highlight_helpers;
    using namespace token_constants;

    size_t len = std::strlen(input);
    if (len == 0)
        return;

    std::string sanitized_input(input, len);

    // First pass: highlight history expansions (before sanitization)
    if (config::history_expansion_enabled) {
        highlight_history_expansions(henv, input, len);
    }

    // Second pass: sanitize comments
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

    // Check for function definition
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

    // Process commands
    size_t pos = 0;
    while (pos < len) {
        size_t cmd_end = pos;
        while (cmd_end < len) {
            if ((cmd_end + 1 < len && analysis[cmd_end] == '&' && analysis[cmd_end + 1] == '&') ||
                (cmd_end + 1 < len && analysis[cmd_end] == '|' && analysis[cmd_end + 1] == '|') ||
                analysis[cmd_end] == '|' || analysis[cmd_end] == ';' || analysis[cmd_end] == '\n' ||
                analysis[cmd_end] == '\r') {
                break;
            }
            cmd_end++;
        }

        size_t cmd_start = pos;
        while (cmd_start < cmd_end && (std::isspace((unsigned char)analysis[cmd_start]) != 0)) {
            cmd_start++;
        }

        std::string cmd_str(analysis + cmd_start, cmd_end - cmd_start);

        // Extract first token
        size_t token_end = 0;
        while (token_end < cmd_str.length() &&
               (std::isspace((unsigned char)cmd_str[token_end]) == 0)) {
            token_end++;
        }

        std::string token = token_end > 0 ? cmd_str.substr(0, token_end) : "";

        bool is_sudo_command = (token == "sudo");

        // Highlight first token (command)
        if (!token.empty()) {
            bool handled_first_token = false;

            if (is_variable_reference(token)) {
                highlight_variable_assignment(henv, input, cmd_start, token);
                handled_first_token = true;
            }

            if (!handled_first_token && (token.rfind("./", 0) == 0 || token.rfind("../", 0) == 0 ||
                                         token.rfind("~/", 0) == 0 || token.rfind("-/", 0) == 0 ||
                                         token[0] == '/' || token.find('/') != std::string::npos)) {
                std::string path_to_check = token;
                if (token.rfind("~/", 0) == 0) {
                    path_to_check = cjsh_filesystem::g_user_home_path.string() + token.substr(1);
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
                    ic_highlight(henv, static_cast<long>(cmd_start), static_cast<long>(token_end),
                                 "cjsh-installed");
                } else {
                    ic_highlight(henv, static_cast<long>(cmd_start), static_cast<long>(token_end),
                                 "cjsh-unknown-command");
                }
                handled_first_token = true;
            }

            if (!handled_first_token && is_shell_keyword(token)) {
                ic_highlight(henv, static_cast<long>(cmd_start), static_cast<long>(token_end),
                             "cjsh-keyword");
                handled_first_token = true;
            } else if (!handled_first_token && is_shell_builtin(token)) {
                ic_highlight(henv, static_cast<long>(cmd_start), static_cast<long>(token_end),
                             "cjsh-builtin");
                handled_first_token = true;
            }

            if (!handled_first_token) {
                auto cmds = g_shell->get_available_commands();
                if (cmds.find(token) != cmds.end()) {
                    ic_highlight(henv, static_cast<long>(cmd_start), static_cast<long>(token_end),
                                 "cjsh-builtin");
                } else if (basic_unix_commands.count(token) > 0) {
                    ic_highlight(henv, static_cast<long>(cmd_start), static_cast<long>(token_end),
                                 "cjsh-system");
                } else if (is_external_command(token)) {
                    ic_highlight(henv, static_cast<long>(cmd_start), static_cast<long>(token_end),
                                 "cjsh-installed");
                } else {
                    ic_highlight(henv, static_cast<long>(cmd_start), static_cast<long>(token_end),
                                 "cjsh-unknown-command");
                }
            }
        }

        bool is_cd_command = (token == "cd");
        size_t arg_start = token_end;

        // Highlight arguments
        while (arg_start < cmd_str.length()) {
            while (arg_start < cmd_str.length() &&
                   (std::isspace((unsigned char)cmd_str[arg_start]) != 0)) {
                arg_start++;
            }
            if (arg_start >= cmd_str.length())
                break;

            size_t arg_end = arg_start;
            while (arg_end < cmd_str.length() &&
                   (std::isspace((unsigned char)cmd_str[arg_end]) == 0)) {
                arg_end++;
            }

            std::string arg = cmd_str.substr(arg_start, arg_end - arg_start);

            if (is_redirection_operator(arg) || comparison_operators.count(arg) > 0) {
                ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                             static_cast<long>(arg_end - arg_start), "cjsh-operator");
            }

            else if (is_variable_reference(arg)) {
                highlight_variable_assignment(henv, input, cmd_start + arg_start, arg);
            }

            else if (arg == "((" || arg == "))") {
                ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                             static_cast<long>(arg_end - arg_start), "cjsh-arithmetic");
            }

            else if (is_shell_keyword(arg)) {
                ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                             static_cast<long>(arg_end - arg_start), "cjsh-keyword");
            }

            else if (is_option(arg)) {
                ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                             static_cast<long>(arg_end - arg_start), "cjsh-option");
            }

            else if (is_numeric_literal(arg)) {
                ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                             static_cast<long>(arg_end - arg_start), "cjsh-number");
            }

            else {
                char quote_type = 0;
                if (is_quoted_string(arg, quote_type)) {
                    ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                 static_cast<long>(arg_end - arg_start), "cjsh-string");
                } else if (is_sudo_command && arg_start == token_end + 1) {
                    if (arg.rfind("./", 0) == 0) {
                        if (!std::filesystem::exists(arg) ||
                            !std::filesystem::is_regular_file(arg)) {
                            ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                         static_cast<long>(arg_end - arg_start),
                                         "cjsh-unknown-command");
                        } else {
                            ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                         static_cast<long>(arg_end - arg_start), "cjsh-installed");
                        }
                    } else {
                        auto cmds = g_shell->get_available_commands();
                        if (cmds.find(arg) != cmds.end() || is_shell_builtin(arg)) {
                            ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                         static_cast<long>(arg_end - arg_start), "cjsh-builtin");
                        } else if (basic_unix_commands.count(arg) > 0) {
                            ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                         static_cast<long>(arg_end - arg_start), "cjsh-system");
                        } else if (is_external_command(arg)) {
                            ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                         static_cast<long>(arg_end - arg_start), "cjsh-installed");
                        } else {
                            ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                         static_cast<long>(arg_end - arg_start),
                                         "cjsh-unknown-command");
                        }
                    }
                } else if (is_cd_command && (arg == "~" || arg == "-")) {
                    ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                 static_cast<long>(arg_end - arg_start), "cjsh-path-exists");
                } else if (is_glob_pattern(arg)) {
                    ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                 static_cast<long>(arg_end - arg_start), "cjsh-glob-pattern");
                } else if (is_cd_command || arg[0] == '/' || arg.rfind("./", 0) == 0 ||
                           arg.rfind("../", 0) == 0 || arg.rfind("~/", 0) == 0 ||
                           arg.rfind("-/", 0) == 0 || arg.find('/') != std::string::npos) {
                    std::string path_to_check = arg;

                    if (arg.rfind("~/", 0) == 0) {
                        path_to_check = cjsh_filesystem::g_user_home_path.string() + arg.substr(1);
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
                        ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                     static_cast<long>(arg_end - arg_start), "cjsh-path-exists");
                    } else {
                        bool is_bookmark = false;
                        if (is_cd_command && g_shell && (g_shell->get_built_ins() != nullptr)) {
                            const auto& bookmarks =
                                g_shell->get_built_ins()->get_directory_bookmarks();
                            is_bookmark = bookmarks.find(arg) != bookmarks.end();
                        }

                        if (is_bookmark) {
                            ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                         static_cast<long>(arg_end - arg_start),
                                         "cjsh-path-exists");
                        } else {
                            ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                         static_cast<long>(arg_end - arg_start),
                                         "cjsh-path-not-exists");
                        }
                    }
                }
            }

            if (!is_variable_reference(arg) &&
                (arg.find('$') != std::string::npos || arg.find('`') != std::string::npos)) {
                highlight_quotes_and_variables(henv, input, cmd_start + arg_start,
                                               arg_end - arg_start);
            }

            arg_start = arg_end;
        }

        // Highlight operators between commands
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

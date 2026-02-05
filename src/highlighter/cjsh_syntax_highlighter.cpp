/*
  cjsh_syntax_highlighter.cpp

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "cjsh_syntax_highlighter.h"

#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_set>

#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "command_analysis.h"
#include "highlight_helpers.h"
#include "shell.h"
#include "shell_env.h"
#include "token_classifier.h"
#include "token_constants.h"

namespace {

void highlight_command_resolution(ic_highlight_env_t* henv, size_t start, size_t length,
                                  bool is_system_command) {
    ic_highlight(henv, static_cast<long>(start), static_cast<long>(length),
                 is_system_command ? "cjsh-system" : "cjsh-unknown-command");
}

void highlight_command_range(ic_highlight_env_t* henv, const char* input,
                             const std::string& analysis, size_t cmd_start, size_t cmd_end,
                             const std::unordered_set<std::string>& comparison_ops) {
    using namespace token_classifier;
    using namespace highlight_helpers;

    if (cmd_start >= cmd_end) {
        return;
    }

    std::string cmd_str(analysis.c_str() + cmd_start, cmd_end - cmd_start);

    size_t token_cursor = 0;
    size_t first_token_start = 0;
    size_t first_token_end = 0;
    if (!command_analysis::extract_next_token(cmd_str, token_cursor, first_token_start,
                                              first_token_end)) {
        return;
    }

    std::string token = cmd_str.substr(first_token_start, first_token_end - first_token_start);
    bool is_sudo_command = (token == "sudo");
    bool handled_first_token = false;
    size_t absolute_token_start = cmd_start + first_token_start;
    size_t first_token_length = first_token_end - first_token_start;

    if (is_variable_reference(token)) {
        highlight_variable_assignment(henv, input, absolute_token_start, token);
        handled_first_token = true;
    }

    if (!handled_first_token && command_analysis::token_is_history_expansion(token, cmd_start)) {
        handled_first_token = true;
    }

    if (!handled_first_token && command_analysis::token_has_explicit_path_hint(token)) {
        std::string path_to_check = command_analysis::resolve_token_path(token, g_shell.get());
        highlight_command_resolution(henv, absolute_token_start, first_token_length,
                                     std::filesystem::exists(path_to_check));
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
        } else {
            highlight_command_resolution(henv, absolute_token_start, first_token_length,
                                         is_external_command(token));
        }
    }

    static const std::unordered_set<std::string> kInlineCommandKeywords = {
        "do", "then", "else", "elif", "if", "while", "until", "time", "coproc"};

    if (kInlineCommandKeywords.find(token) != kInlineCommandKeywords.end()) {
        size_t nested_start = first_token_end;
        while (nested_start < cmd_str.size() &&
               (std::isspace(static_cast<unsigned char>(cmd_str[nested_start])) != 0)) {
            nested_start++;
        }
        if (nested_start < cmd_str.size()) {
            highlight_command_range(henv, input, analysis, cmd_start + nested_start, cmd_end,
                                    comparison_ops);
        }
        return;
    }

    bool is_cd_command = (token == "cd");
    size_t arg_cursor = token_cursor;
    size_t arg_index = 0;
    size_t arg_start = 0;
    size_t arg_end = 0;

    while (command_analysis::extract_next_token(cmd_str, arg_cursor, arg_start, arg_end)) {
        size_t absolute_arg_start = cmd_start + arg_start;
        size_t arg_length = arg_end - arg_start;
        std::string arg = cmd_str.substr(arg_start, arg_length);

        if (is_redirection_operator(arg) || comparison_ops.count(arg) > 0) {
            ic_highlight(henv, static_cast<long>(absolute_arg_start), static_cast<long>(arg_length),
                         "cjsh-operator");
        }

        else if (is_variable_reference(arg)) {
            highlight_variable_assignment(henv, input, absolute_arg_start, arg);
        }

        else if (arg == "((" || arg == "))") {
            ic_highlight(henv, static_cast<long>(absolute_arg_start), static_cast<long>(arg_length),
                         "cjsh-arithmetic");
        }

        else if (is_shell_keyword(arg)) {
            ic_highlight(henv, static_cast<long>(absolute_arg_start), static_cast<long>(arg_length),
                         "cjsh-keyword");
        }

        else if (is_option(arg)) {
            ic_highlight(henv, static_cast<long>(absolute_arg_start), static_cast<long>(arg_length),
                         "cjsh-option");
        }

        else if (is_numeric_literal(arg)) {
            ic_highlight(henv, static_cast<long>(absolute_arg_start), static_cast<long>(arg_length),
                         "cjsh-number");
        }

        else {
            char quote_type = 0;
            if (is_quoted_string(arg, quote_type)) {
                ic_highlight(henv, static_cast<long>(absolute_arg_start),
                             static_cast<long>(arg_length), "cjsh-string");
            } else if (is_sudo_command && arg_index == 0) {
                if (arg.rfind("./", 0) == 0) {
                    if (!std::filesystem::exists(arg) || !std::filesystem::is_regular_file(arg)) {
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
                    if (is_abbreviation || cmds.find(arg) != cmds.end() || is_shell_builtin(arg)) {
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
                    path_to_check = cjsh_filesystem::g_user_home_path().string() + arg.substr(1);
                } else if (arg.rfind("-/", 0) == 0) {
                    std::string prev_dir = g_shell->get_previous_directory();
                    if (!prev_dir.empty()) {
                        path_to_check = prev_dir + arg.substr(1);
                    }
                } else if (is_cd_command && arg[0] != '/' && arg.rfind("./", 0) != 0 &&
                           arg.rfind("../", 0) != 0 && arg.rfind("~/", 0) != 0 &&
                           arg.rfind("-/", 0) != 0) {
                    path_to_check = cjsh_filesystem::safe_current_directory() + "/" + arg;
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

    std::string raw_input(input, len);
    std::vector<command_analysis::CommentRange> comment_ranges;
    std::string sanitized_input =
        command_analysis::sanitize_input_for_analysis(raw_input, &comment_ranges);

    if (config::history_expansion_enabled) {
        highlight_history_expansions(henv, input, len);
    }

    for (const auto& range : comment_ranges) {
        if (range.end > range.start) {
            ic_highlight(henv, static_cast<long>(range.start),
                         static_cast<long>(range.end - range.start), "cjsh-comment");
        }
    }

    const char* analysis = sanitized_input.c_str();

    size_t func_name_start = 0;
    size_t func_name_end = 0;
    if (is_function_definition(sanitized_input, func_name_start, func_name_end)) {
        ic_highlight(henv, static_cast<long>(func_name_start),
                     static_cast<long>(func_name_end - func_name_start),
                     "cjsh-function-definition");

        size_t paren_pos = sanitized_input.find("()", func_name_end);
        if (paren_pos != std::string::npos && paren_pos < len) {
            ic_highlight(henv, static_cast<long>(paren_pos), 2L, "cjsh-function-definition");
        }

        size_t brace_pos = sanitized_input.find('{');
        if (brace_pos != std::string::npos && brace_pos < len) {
            ic_highlight(henv, static_cast<long>(brace_pos), 1L, "cjsh-operator");
        }
        return;
    }

    const auto& comparison_ops = token_constants::comparison_operators();

    size_t pos = 0;
    while (pos < len) {
        size_t cmd_end = command_analysis::find_command_end(sanitized_input, pos);

        size_t cmd_start = pos;
        while (cmd_start < cmd_end && (std::isspace((unsigned char)analysis[cmd_start]) != 0)) {
            cmd_start++;
        }

        if (cmd_start < cmd_end) {
            highlight_command_range(henv, input, sanitized_input, cmd_start, cmd_end,
                                    comparison_ops);
        }

        pos = cmd_end;
        if (pos < len) {
            auto separator = command_analysis::scan_command_separator(sanitized_input, pos);
            if (separator.length > 0) {
                if (separator.is_operator) {
                    ic_highlight(henv, static_cast<long>(pos), static_cast<long>(separator.length),
                                 "cjsh-operator");
                }
                pos += separator.length;
            } else {
                pos += 1;
            }
        }
    }
}

/*
  validation_redirection.cpp

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

#include "interpreter.h"

#include "error_out.h"
#include "parser_utils.h"
#include "validation_common.h"

#include <cctype>
#include <string>
#include <vector>

using namespace shell_validation::internal;
using ErrorCategory = ShellScriptInterpreter::ErrorCategory;

namespace {

bool check_pipe_missing_command(const std::string& line, size_t pipe_pos) {
    size_t after_pipe = pipe_pos + 1;
    while (after_pipe < line.length() && std::isspace(line[after_pipe])) {
        after_pipe++;
    }
    return after_pipe >= line.length() || line[after_pipe] == '|' || line[after_pipe] == '&';
}

ShellScriptInterpreter::SyntaxError create_pipe_error(size_t display_line, size_t start_pos,
                                                      size_t end_pos, const std::string& line,
                                                      const std::string& message,
                                                      const std::string& suggestion) {
    return ShellScriptInterpreter::SyntaxError({display_line, start_pos, end_pos, 0},
                                               ErrorSeverity::ERROR, ErrorCategory::REDIRECTION,
                                               "PIPE001", message, line, suggestion);
}

}  // namespace

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::validate_redirection_syntax(const std::vector<std::string>& lines) {
    return validate_default_char_iteration_with_context(lines, [](CharIterationContext& ctx) {
        auto& line_errors = ctx.line_errors;
        const std::string& line = ctx.line;
        size_t display_line = ctx.display_line;
        size_t i = ctx.index;
        char c = ctx.character;
        const QuoteState& state = ctx.state;
        size_t& next_index = ctx.next_index;

        if (state.in_quotes) {
            return;
        }

        if (c == '<' || c == '>') {
            size_t redir_start = i;
            std::string redir_op;

            if (c == '>' && i + 1 < line.length()) {
                if (line[i + 1] == '>') {
                    redir_op = ">>";
                    next_index = i + 1;
                } else if (line[i + 1] == '&') {
                    redir_op = ">&";
                    next_index = i + 1;
                } else if (line[i + 1] == '|') {
                    redir_op = ">|";
                    next_index = i + 1;
                } else {
                    redir_op = ">";
                }
            } else if (c == '<' && i + 1 < line.length()) {
                if (line[i + 1] == '<') {
                    if (i + 2 < line.length() && line[i + 2] == '<') {
                        redir_op = "<<<";
                        next_index = i + 2;
                    } else {
                        redir_op = "<<";
                        next_index = i + 1;
                    }
                } else {
                    redir_op = "<";
                }
            } else {
                redir_op = c;
            }

            size_t check_pos = next_index + 1;
            while (check_pos < line.length() && std::isspace(line[check_pos])) {
                check_pos++;
            }

            if (check_pos < line.length()) {
                char next_char = line[check_pos];
                if ((redir_op == ">" && next_char == '>') ||
                    (redir_op == "<" && next_char == '<') ||
                    (redir_op == ">>" && next_char == '>') ||
                    (redir_op == "<<" && next_char == '<')) {
                    line_errors.push_back(SyntaxError(
                        {display_line, redir_start, check_pos + 1, 0}, ErrorSeverity::ERROR,
                        ErrorCategory::REDIRECTION, "RED005",
                        "Invalid redirection syntax '" + redir_op + " " + next_char + "'", line,
                        "Use single redirection operator"));
                    return;
                }
            }

            size_t target_start = next_index + 1;
            while (target_start < line.length() && std::isspace(line[target_start])) {
                target_start++;
            }

            if (target_start >= line.length()) {
                line_errors.push_back(
                    SyntaxError({display_line, redir_start, next_index + 1, 0},
                                ErrorSeverity::ERROR, ErrorCategory::REDIRECTION, "RED001",
                                "Redirection '" + redir_op + "' missing target", line,
                                "Add filename or file descriptor after " + redir_op));
                return;
            }

            std::string target;
            size_t target_end = target_start;
            bool in_target_quotes = false;
            char target_quote = '\0';

            while (target_end < line.length()) {
                char tc = line[target_end];
                if (!in_target_quotes && std::isspace(tc)) {
                    break;
                }
                if ((tc == '"' || tc == '\'') && !in_target_quotes) {
                    in_target_quotes = true;
                    target_quote = tc;
                } else if (tc == target_quote && in_target_quotes) {
                    in_target_quotes = false;
                    target_quote = '\0';
                }
                target_end++;
            }

            target = line.substr(target_start, target_end - target_start);

            if (redir_op == ">&" || redir_op == "<&") {
                if (target.empty() ||
                    (!std::isdigit(static_cast<unsigned char>(target[0])) && target != "-")) {
                    line_errors.push_back(
                        SyntaxError({display_line, target_start, target_end, 0},
                                    ErrorSeverity::ERROR, ErrorCategory::REDIRECTION, "RED002",
                                    "File descriptor redirection requires digit or '-'", line,
                                    "Use format like 2>&1 or 2>&-"));
                }
            } else if (redir_op == "<<") {
                if (target.empty()) {
                    line_errors.push_back(SyntaxError(
                        {display_line, target_start, target_end, 0}, ErrorSeverity::ERROR,
                        ErrorCategory::REDIRECTION, "RED003", "Here document missing delimiter",
                        line, "Provide delimiter like: << EOF"));
                }
            }

            next_index = target_end - 1;
        }

        if (c == '|' && i + 1 < line.length()) {
            if (line[i + 1] == '|') {
                next_index = i + 1;
            } else {
                size_t pipe_pos = i;
                if (check_pipe_missing_command(line, pipe_pos)) {
                    line_errors.push_back(create_pipe_error(display_line, pipe_pos, pipe_pos + 1,
                                                            line, "Pipe missing command after '|'",
                                                            "Add command after pipe"));
                }
            }
        }
    });
}

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::validate_pipeline_syntax(
    const std::vector<std::string>& lines) {
    return process_lines_for_validation(
        lines,
        [](const std::string& line, const std::string& trimmed_line, size_t display_line,
           size_t first_non_space) -> std::vector<SyntaxError> {
            std::vector<SyntaxError> line_errors;

            size_t eq = trimmed_line.find('=');
            if (eq != std::string::npos) {
                std::string lhs = trimmed_line.substr(0, eq);

                while (!lhs.empty() && isspace(static_cast<unsigned char>(lhs.back())) != 0)
                    lhs.pop_back();

                size_t lb = lhs.find('[');
                size_t rb = lhs.rfind(']');
                if (lb != std::string::npos && rb != std::string::npos && rb > lb &&
                    rb == lhs.size() - 1) {
                    std::string name = lhs.substr(0, lb);
                    bool name_ok = is_valid_identifier(name);
                    std::string index_text = lhs.substr(lb + 1, rb - lb - 1);
                    std::string issue;
                    if (name_ok && !validate_array_index_expression(index_text, issue)) {
                        line_errors.push_back(SyntaxError(
                            {display_line, first_non_space + lb, first_non_space + rb + 1, 0},
                            ErrorSeverity::ERROR, ErrorCategory::VARIABLES, "VAR005",
                            issue + " for array '" + name + "'", line,
                            "Use a valid numeric or arithmetic expression index"));
                    }
                }
            }

            if (!trimmed_line.empty() && trimmed_line[0] == '|' &&
                (trimmed_line.size() <= 1 || trimmed_line[1] != '|')) {
                line_errors.push_back(
                    SyntaxError({display_line, first_non_space, first_non_space + 1, 0},
                                ErrorSeverity::ERROR, ErrorCategory::REDIRECTION, "PIPE002",
                                "Pipeline cannot start with pipe operator", line,
                                "Remove leading pipe or add command before pipe"));
            }

            for_each_effective_char(
                line, false, false,
                [&](size_t i, char c, const QuoteState& state,
                    size_t& next_index) -> IterationAction {
                    if (!state.in_quotes && c == '|' && i + 1 < line.length()) {
                        if (line[i + 1] == '|' && (i + 2 >= line.length() || line[i + 2] != '|')) {
                            size_t after_logical = i + 2;
                            while (after_logical < line.length() &&
                                   std::isspace(line[after_logical])) {
                                after_logical++;
                            }

                            if (after_logical < line.length() && line[after_logical] == '|') {
                                line_errors.push_back(SyntaxError(
                                    {display_line, i, after_logical + 1, 0}, ErrorSeverity::ERROR,
                                    ErrorCategory::REDIRECTION, "PIPE001",
                                    "Invalid pipeline syntax", line, "Check pipe operator usage"));
                            }
                            next_index = i + 1;
                        } else if (line[i + 1] != '|') {
                            if (check_pipe_missing_command(line, i)) {
                                line_errors.push_back(create_pipe_error(
                                    display_line, i, i + 1, line, "Pipe missing command after '|'",
                                    "Add command after pipe"));
                            }
                        }
                    }

                    return IterationAction::Continue;
                });

            return line_errors;
        });
}

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::validate_heredoc_syntax(
    const std::vector<std::string>& lines) {
    std::vector<SyntaxError> errors;

    std::vector<std::pair<std::string, size_t>> heredoc_stack;

    for (size_t line_num = 0; line_num < lines.size(); ++line_num) {
        const std::string& line = lines[line_num];
        size_t display_line = line_num + 1;

        if (!heredoc_stack.empty()) {
            std::string trimmed_line = line;
            trimmed_line.erase(0, trimmed_line.find_first_not_of(" \t"));
            trimmed_line.erase(trimmed_line.find_last_not_of(" \t\r\n") + 1);

            if (trimmed_line == heredoc_stack.back().first) {
                heredoc_stack.pop_back();
                continue;
            }
        }

        bool in_quotes = false;
        char quote_char = '\0';
        size_t i = 0;
        int arithmetic_depth = 0;

        while (i < line.length()) {
            char current_char = line[i];

            if (!in_quotes && (current_char == '"' || current_char == '\'')) {
                in_quotes = true;
                quote_char = current_char;
                ++i;
                continue;
            }

            if (in_quotes && current_char == quote_char) {
                in_quotes = false;
                quote_char = '\0';
                ++i;
                continue;
            }

            if (!in_quotes) {
                if (i + 2 < line.length() && line.compare(i, 3, "$((") == 0) {
                    arithmetic_depth++;
                    i += 3;
                    continue;
                }

                if (i + 1 < line.length() && line.compare(i, 2, "((") == 0) {
                    arithmetic_depth++;
                    i += 2;
                    continue;
                }

                if (arithmetic_depth > 0 && i + 1 < line.length() &&
                    line.compare(i, 2, "))") == 0) {
                    arithmetic_depth--;
                    i += 2;
                    continue;
                }

                if (arithmetic_depth == 0 && i + 1 < line.length() &&
                    line.compare(i, 2, "<<") == 0) {
                    size_t heredoc_pos = i;
                    size_t delim_start = heredoc_pos + 2;
                    while (delim_start < line.length() &&
                           (std::isspace(static_cast<unsigned char>(line[delim_start])) != 0)) {
                        ++delim_start;
                    }

                    size_t delim_end = delim_start;
                    if (delim_start < line.length()) {
                        while (delim_end < line.length() &&
                               (std::isspace(static_cast<unsigned char>(line[delim_end])) == 0) &&
                               line[delim_end] != ';' && line[delim_end] != '&' &&
                               line[delim_end] != '|') {
                            ++delim_end;
                        }

                        if (delim_start < delim_end) {
                            std::string delimiter =
                                line.substr(delim_start, delim_end - delim_start);

                            if ((delimiter.front() == '"' && delimiter.back() == '"') ||
                                (delimiter.front() == '\'' && delimiter.back() == '\'')) {
                                delimiter = delimiter.substr(1, delimiter.length() - 2);
                            }

                            if (!heredoc_stack.empty()) {
                                errors.push_back(SyntaxError(
                                    {display_line, heredoc_pos, delim_end, 0},
                                    ErrorSeverity::WARNING, ErrorCategory::SYNTAX, "SYN011",
                                    "Nested heredoc detected - may cause parsing issues", line,
                                    "Consider closing previous heredoc '" +
                                        heredoc_stack.back().first + "' before starting new one"));
                            }

                            heredoc_stack.push_back({delimiter, display_line});
                        }
                    }

                    i = delim_end;
                    continue;
                }
            }

            ++i;
        }
    }

    while (!heredoc_stack.empty()) {
        auto& unclosed = heredoc_stack.back();
        errors.push_back(SyntaxError({unclosed.second, 0, 0, 0}, ErrorSeverity::ERROR,
                                     ErrorCategory::SYNTAX, "SYN010",
                                     "Unclosed here document - missing '" + unclosed.first + "'",
                                     "", "Add closing delimiter: " + unclosed.first));
        heredoc_stack.pop_back();
    }

    return errors;
}

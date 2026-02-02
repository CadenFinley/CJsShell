/*
  function_evaluator.cpp

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

#include "function_evaluator.h"

#include <cctype>
#include <utility>

namespace function_evaluator {

FunctionParseResult parse_and_register_functions(
    const std::string& line, const std::vector<std::string>& lines, size_t& line_index,
    FunctionMap& functions, const std::function<std::string(const std::string&)>& trim_func,
    const std::function<std::string(const std::string&)>& strip_comment_func) {
    FunctionParseResult result{false, ""};

    std::string current_line = line;
    bool found_function = true;

    while (!current_line.empty() && found_function) {
        found_function = false;

        std::string trimmed_line = trim_func(current_line);
        bool has_function_keyword = false;
        std::string func_name;

        auto find_body_start = [](const std::string& source, const std::string& name) {
            if (name.empty()) {
                return std::pair<size_t, char>{std::string::npos, '}'};
            }

            size_t start = source.find(name);
            if (start == std::string::npos) {
                return std::pair<size_t, char>{std::string::npos, '}'};
            }

            start += name.length();
            while (start < source.length() &&
                   std::isspace(static_cast<unsigned char>(source[start]))) {
                start++;
            }

            if (start < source.length() && source[start] == '(') {
                size_t lookahead = start + 1;
                while (lookahead < source.length() &&
                       std::isspace(static_cast<unsigned char>(source[lookahead]))) {
                    lookahead++;
                }
                if (lookahead < source.length() && source[lookahead] == ')') {
                    start = lookahead + 1;
                    while (start < source.length() &&
                           std::isspace(static_cast<unsigned char>(source[start]))) {
                        start++;
                    }
                }
            }

            if (start < source.length() && (source[start] == '{' || source[start] == '(')) {
                char open = source[start];
                char close = open == '{' ? '}' : ')';
                return std::pair<size_t, char>{start, close};
            }

            return std::pair<size_t, char>{std::string::npos, '}'};
        };

        if (trimmed_line.rfind("function", 0) == 0 && trimmed_line.length() > 8 &&
            std::isspace(static_cast<unsigned char>(trimmed_line[8]))) {
            has_function_keyword = true;
            size_t name_start = 8;

            while (name_start < trimmed_line.length() &&
                   std::isspace(static_cast<unsigned char>(trimmed_line[name_start]))) {
                name_start++;
            }

            if (name_start < trimmed_line.length()) {
                size_t name_end = name_start;
                while (name_end < trimmed_line.length() &&
                       !std::isspace(static_cast<unsigned char>(trimmed_line[name_end])) &&
                       trimmed_line[name_end] != '(' && trimmed_line[name_end] != '{') {
                    name_end++;
                }

                func_name = trimmed_line.substr(name_start, name_end - name_start);
            }
        }

        size_t name_end = current_line.find("()");

        if (!has_function_keyword && name_end != std::string::npos) {
            std::string potential_name = trim_func(current_line.substr(0, name_end));
            auto potential_body = find_body_start(current_line, potential_name);
            size_t potential_pos = potential_body.first;
            if (potential_pos != std::string::npos && name_end <= potential_pos) {
                func_name = potential_name;
            }
        }

        auto body_info = find_body_start(current_line, func_name);
        size_t body_pos = body_info.first;
        char closing_delim = body_info.second;
        char opening_delim = body_pos != std::string::npos ? current_line[body_pos] : '{';

        if (!func_name.empty() && func_name.find(' ') == std::string::npos &&
            body_pos != std::string::npos) {
            std::vector<std::string> body_lines;
            bool handled_single_line = false;
            std::string after_body = trim_func(current_line.substr(body_pos + 1));

            if (!after_body.empty()) {
                size_t end_delim = after_body.find(closing_delim);
                if (end_delim != std::string::npos) {
                    std::string body_part = trim_func(after_body.substr(0, end_delim));
                    if (!body_part.empty())
                        body_lines.push_back(body_part);

                    functions[func_name] = body_lines;

                    std::string remainder = trim_func(after_body.substr(end_delim + 1));

                    size_t start_pos = 0;
                    while (
                        start_pos < remainder.length() &&
                        (remainder[start_pos] == ';' ||
                         (std::isspace(static_cast<unsigned char>(remainder[start_pos])) != 0))) {
                        start_pos++;
                    }
                    remainder = remainder.substr(start_pos);
                    current_line = remainder;
                    found_function = true;
                    handled_single_line = true;
                } else if (!after_body.empty()) {
                    body_lines.push_back(after_body);
                }
            }

            if (!handled_single_line) {
                int depth = 1;
                std::string after_closing_delim;

                while (++line_index < lines.size() && depth > 0) {
                    const std::string& func_line_raw = lines[line_index];
                    std::string func_line = trim_func(strip_comment_func(func_line_raw));

                    for (char ch : func_line) {
                        if (ch == opening_delim) {
                            depth++;
                        } else if (ch == closing_delim) {
                            depth--;
                        }
                    }

                    if (depth <= 0) {
                        size_t pos = func_line.find(closing_delim);
                        if (pos != std::string::npos) {
                            std::string before = trim_func(func_line.substr(0, pos));
                            if (!before.empty())
                                body_lines.push_back(before);

                            if (pos + 1 < func_line.length()) {
                                after_closing_delim = trim_func(func_line.substr(pos + 1));
                            }
                        }
                        break;
                    }

                    if (!func_line.empty()) {
                        body_lines.push_back(func_line_raw);
                    }
                }

                functions[func_name] = body_lines;

                if (after_closing_delim.empty()) {
                    current_line.clear();
                } else {
                    size_t start_pos = 0;
                    while (start_pos < after_closing_delim.length() &&
                           (after_closing_delim[start_pos] == ';' ||
                            (std::isspace(static_cast<unsigned char>(
                                 after_closing_delim[start_pos])) != 0))) {
                        start_pos++;
                    }
                    current_line = after_closing_delim.substr(start_pos);
                }

                break;
            }

            result.found = true;
        }
    }

    if (!current_line.empty()) {
        result.remaining_line = current_line;
    }

    return result;
}

bool has_function(const FunctionMap& functions, const std::string& name) {
    return functions.find(name) != functions.end();
}

std::vector<std::string> get_function_names(const FunctionMap& functions) {
    std::vector<std::string> names;
    names.reserve(functions.size());
    for (const auto& pair : functions) {
        names.push_back(pair.first);
    }
    return names;
}

void push_function_scope(LocalVariableStack& stack) {
    stack.emplace_back();
}

void pop_function_scope(LocalVariableStack& stack) {
    if (stack.empty()) {
        return;
    }
    stack.pop_back();
}

void set_local_variable(
    LocalVariableStack& stack, const std::string& name, const std::string& value,
    const std::function<void(const std::string&, const std::string&)>& set_global_var) {
    if (stack.empty()) {
        set_global_var(name, value);
        return;
    }
    stack.back()[name] = value;
}

bool is_local_variable(const LocalVariableStack& stack, const std::string& name) {
    if (stack.empty()) {
        return false;
    }
    const auto& current_scope = stack.back();
    return current_scope.find(name) != current_scope.end();
}

}  // namespace function_evaluator

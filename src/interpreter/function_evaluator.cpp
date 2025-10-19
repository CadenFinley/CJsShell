#include "function_evaluator.h"

#include <algorithm>
#include <cctype>

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
        size_t brace_pos = current_line.find('{');

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

                
                size_t pos = name_end;
                while (pos < trimmed_line.length() &&
                       std::isspace(static_cast<unsigned char>(trimmed_line[pos]))) {
                    pos++;
                }
                if (pos + 1 < trimmed_line.length() && trimmed_line[pos] == '(' &&
                    trimmed_line[pos + 1] == ')') {
                    pos += 2;
                }

                
                brace_pos = current_line.find('{');
            }
        }

        
        size_t name_end = current_line.find("()");

        if (!has_function_keyword && name_end != std::string::npos &&
            brace_pos != std::string::npos && name_end < brace_pos) {
            func_name = trim_func(current_line.substr(0, name_end));
        }

        
        if (!func_name.empty() && func_name.find(' ') == std::string::npos &&
            brace_pos != std::string::npos) {
            std::vector<std::string> body_lines;
            bool handled_single_line = false;
            std::string after_brace = trim_func(current_line.substr(brace_pos + 1));

            if (!after_brace.empty()) {
                size_t end_brace = after_brace.find('}');
                if (end_brace != std::string::npos) {
                    std::string body_part = trim_func(after_brace.substr(0, end_brace));
                    if (!body_part.empty())
                        body_lines.push_back(body_part);

                    functions[func_name] = body_lines;

                    std::string remainder = trim_func(after_brace.substr(end_brace + 1));

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
                } else if (!after_brace.empty()) {
                    body_lines.push_back(after_brace);
                }
            }

            if (!handled_single_line) {
                int depth = 1;
                std::string after_closing_brace;

                while (++line_index < lines.size() && depth > 0) {
                    const std::string& func_line_raw = lines[line_index];
                    std::string func_line = trim_func(strip_comment_func(func_line_raw));

                    for (char ch : func_line) {
                        if (ch == '{') {
                            depth++;
                        } else if (ch == '}') {
                            depth--;
                        }
                    }

                    if (depth <= 0) {
                        size_t pos = func_line.find('}');
                        if (pos != std::string::npos) {
                            std::string before = trim_func(func_line.substr(0, pos));
                            if (!before.empty())
                                body_lines.push_back(before);

                            if (pos + 1 < func_line.length()) {
                                after_closing_brace = trim_func(func_line.substr(pos + 1));
                            }
                        }
                        break;
                    }

                    if (!func_line.empty()) {
                        body_lines.push_back(func_line_raw);
                    }
                }

                functions[func_name] = body_lines;

                if (after_closing_brace.empty()) {
                    current_line.clear();
                } else {
                    size_t start_pos = 0;
                    while (start_pos < after_closing_brace.length() &&
                           (after_closing_brace[start_pos] == ';' ||
                            (std::isspace(static_cast<unsigned char>(
                                 after_closing_brace[start_pos])) != 0))) {
                        start_pos++;
                    }
                    current_line = after_closing_brace.substr(start_pos);
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

}  

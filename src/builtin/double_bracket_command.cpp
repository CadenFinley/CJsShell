/*
  double_bracket_command.cpp

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

#include "double_bracket_command.h"

#include "builtin_help.h"
#include "error_out.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <regex>

namespace {

bool pattern_match(const std::string& text, const std::string& pattern) {
    std::string regex_pattern;
    for (size_t i = 0; i < pattern.size(); ++i) {
        char c = pattern[i];
        switch (c) {
            case '*':
                regex_pattern += ".*";
                break;
            case '?':
                regex_pattern += ".";
                break;
            case '[': {
                regex_pattern += "[";
                size_t j = i + 1;
                while (j < pattern.size() && pattern[j] != ']') {
                    regex_pattern += pattern[j];
                    j++;
                }
                if (j < pattern.size()) {
                    regex_pattern += "]";
                    i = j;
                } else {
                    regex_pattern += "\\[";
                }
                break;
            }
            case '\\':
                if (i + 1 < pattern.size()) {
                    regex_pattern += "\\\\";

                    char next_char = pattern[i + 1];
                    if (next_char == '.' || next_char == '^' || next_char == '$' ||
                        next_char == '*' || next_char == '+' || next_char == '?' ||
                        next_char == '(' || next_char == ')' || next_char == '[' ||
                        next_char == ']' || next_char == '{' || next_char == '}' ||
                        next_char == '|' || next_char == '\\') {
                        regex_pattern += "\\";
                    }
                    regex_pattern += next_char;
                    i++;
                } else {
                    regex_pattern += "\\\\";
                }
                break;
            case '.':
            case '^':
            case '$':
            case '+':
            case '(':
            case ')':
            case '{':
            case '}':
            case '|':
                regex_pattern += "\\";
                regex_pattern += c;
                break;
            default:
                regex_pattern += c;
                break;
        }
    }

    try {
        std::regex re(regex_pattern);
        return std::regex_match(text, re);
    } catch (const std::regex_error&) {
        return false;
    }
}

int evaluate_expression(const std::vector<std::string>& tokens) {
    if (tokens.empty()) {
        return 1;
    }

    if (tokens.size() == 1) {
        return tokens[0].empty() ? 1 : 0;
    }

    if (tokens.size() == 2) {
        const std::string& op = tokens[0];
        const std::string& arg = tokens[1];

        if (op == "-z") {
            return arg.empty() ? 0 : 1;
        } else if (op == "-n") {
            return arg.empty() ? 1 : 0;
        } else if (op == "-f") {
            struct stat st;
            return (stat(arg.c_str(), &st) == 0 && S_ISREG(st.st_mode)) ? 0 : 1;
        } else if (op == "-d") {
            struct stat st;
            return (stat(arg.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) ? 0 : 1;
        } else if (op == "-e") {
            return access(arg.c_str(), F_OK) == 0 ? 0 : 1;
        } else if (op == "-r") {
            return access(arg.c_str(), R_OK) == 0 ? 0 : 1;
        } else if (op == "-w") {
            return access(arg.c_str(), W_OK) == 0 ? 0 : 1;
        } else if (op == "-x") {
            return access(arg.c_str(), X_OK) == 0 ? 0 : 1;
        } else if (op == "-s") {
            struct stat st;
            return (stat(arg.c_str(), &st) == 0 && st.st_size > 0) ? 0 : 1;
        } else if (op == "!") {
            return arg.empty() ? 0 : 1;
        }
    }

    if (tokens.size() == 3) {
        if (tokens[0] == "!") {
            std::vector<std::string> sub_tokens(tokens.begin() + 1, tokens.end());
            return evaluate_expression(sub_tokens) == 0 ? 1 : 0;
        }

        const std::string& arg1 = tokens[0];
        const std::string& op = tokens[1];
        const std::string& arg2 = tokens[2];

        if (op == "=" || op == "==") {
            return pattern_match(arg1, arg2) ? 0 : 1;
        } else if (op == "!=") {
            return pattern_match(arg1, arg2) ? 1 : 0;
        } else if (op == "=~") {
            try {
                std::regex re(arg2);
                return std::regex_search(arg1, re) ? 0 : 1;
            } catch (const std::regex_error&) {
                return 1;
            }
        } else if (op == "-eq") {
            try {
                int n1 = std::stoi(arg1);
                int n2 = std::stoi(arg2);
                return n1 == n2 ? 0 : 1;
            } catch (...) {
                return 1;
            }
        } else if (op == "-ne") {
            try {
                int n1 = std::stoi(arg1);
                int n2 = std::stoi(arg2);
                return n1 != n2 ? 0 : 1;
            } catch (...) {
                return 1;
            }
        } else if (op == "-lt") {
            try {
                int n1 = std::stoi(arg1);
                int n2 = std::stoi(arg2);
                return n1 < n2 ? 0 : 1;
            } catch (...) {
                return 1;
            }
        } else if (op == "-le") {
            try {
                int n1 = std::stoi(arg1);
                int n2 = std::stoi(arg2);
                return n1 <= n2 ? 0 : 1;
            } catch (...) {
                return 1;
            }
        } else if (op == "-gt") {
            try {
                int n1 = std::stoi(arg1);
                int n2 = std::stoi(arg2);
                return n1 > n2 ? 0 : 1;
            } catch (...) {
                return 1;
            }
        } else if (op == "-ge") {
            try {
                int n1 = std::stoi(arg1);
                int n2 = std::stoi(arg2);
                return n1 >= n2 ? 0 : 1;
            } catch (...) {
                return 1;
            }
        }
    }

    if (tokens.size() == 4 && tokens[0] == "!") {
        std::vector<std::string> sub_tokens(tokens.begin() + 1, tokens.end());
        return evaluate_expression(sub_tokens) == 0 ? 1 : 0;
    }

    return 1;
}

bool is_binary_op(const std::string& op) {
    return op == "=" || op == "==" || op == "!=" || op == "=~" || op == "-eq" || op == "-ne" ||
           op == "-lt" || op == "-le" || op == "-gt" || op == "-ge" || op == "-ef" || op == "-nt" ||
           op == "-ot";
}

}  // namespace

int double_bracket_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(
            args,
            {"Usage: [[ EXPRESSION ]]",
             "Evaluate a conditional expression with pattern matching and logical operators."})) {
        return 0;
    }
    if (args.empty()) {
        return 1;
    }

    if (args[0] == "[[" && (args.size() == 1 || args.back() != "]]")) {
        print_error({ErrorType::SYNTAX_ERROR, "[[", "missing closing ']]'", {}});
        return 2;
    }

    std::vector<std::string> expression_args = args;

    if (args[0] == "[[" && args.size() > 1 && args.back() == "]]") {
        expression_args.pop_back();
        expression_args.erase(expression_args.begin());
    } else if (args[0] == "[[") {
        expression_args.erase(expression_args.begin());
    }

    if (expression_args.empty()) {
        return 1;
    }

    if ((expression_args.size() == 1 && is_binary_op(expression_args[0])) ||
        (expression_args.size() == 2 && is_binary_op(expression_args[1]))) {
        print_error({ErrorType::SYNTAX_ERROR, "[[", "syntax error: missing operand", {}});
        return 2;
    }

    std::vector<std::vector<std::string>> expressions;
    std::vector<std::string> operators;
    std::vector<std::string> current_expr;

    for (const auto& token : expression_args) {
        if (token == "&&" || token == "||") {
            if (!current_expr.empty()) {
                expressions.push_back(current_expr);
                current_expr.clear();
            }
            operators.push_back(token);
        } else {
            current_expr.push_back(token);
        }
    }

    if (!current_expr.empty()) {
        expressions.push_back(current_expr);
    }

    if (expressions.empty()) {
        return 1;
    }

    int result = evaluate_expression(expressions[0]);

    for (size_t i = 0; i < operators.size() && i + 1 < expressions.size(); ++i) {
        if (operators[i] == "&&") {
            if (result == 0) {
                result = evaluate_expression(expressions[i + 1]);
            }

        } else if (operators[i] == "||") {
            if (result != 0) {
                result = evaluate_expression(expressions[i + 1]);
            }
        }
    }

    return result;
}

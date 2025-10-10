#include "arithmetic_evaluator.h"

#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <vector>

ArithmeticEvaluator::ArithmeticEvaluator(VariableReader var_reader, VariableWriter var_writer)
    : read_variable(std::move(var_reader)), write_variable(std::move(var_writer)) {
}

long long ArithmeticEvaluator::evaluate(const std::string& expr) {
    auto tokens = tokenize(expr);
    handle_assignment_operators(tokens);
    handle_increment_operators(tokens);
    auto postfix = infix_to_postfix(tokens);
    return evaluate_postfix(postfix);
}

bool ArithmeticEvaluator::is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

int ArithmeticEvaluator::get_precedence(const std::string& op) {
    if (op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=")
        return 1;
    if (op == "?:" || op == "?")
        return 2;
    if (op == "||")
        return 3;
    if (op == "&&")
        return 4;
    if (op == "|")
        return 5;
    if (op == "^")
        return 6;
    if (op == "&")
        return 7;
    if (op == "==" || op == "!=")
        return 8;
    if (op == "<" || op == ">" || op == "<=" || op == ">=")
        return 9;
    if (op == "<<" || op == ">>")
        return 10;
    if (op == "+" || op == "-")
        return 11;
    if (op == "*" || op == "/" || op == "%")
        return 12;
    if (op == "**")
        return 13;
    if (op == "!" || op == "~" || op == "unary+" || op == "unary-")
        return 14;
    if (op == "++" || op == "--")
        return 15;
    return 0;
}

bool ArithmeticEvaluator::is_right_associative(const std::string& op) {
    return op == "**" || op == "?" || op == "=" || op == "+=" || op == "-=" || op == "*=" ||
           op == "/=" || op == "%=";
}

std::vector<ArithmeticEvaluator::Token> ArithmeticEvaluator::tokenize(const std::string& expr) {
    std::vector<Token> tokens;
    bool expect_number = true;

    for (size_t i = 0; i < expr.size();) {
        if (is_space(expr[i])) {
            ++i;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(expr[i])) != 0 ||
            (expr[i] == '-' && expect_number && i + 1 < expr.size() &&
             (std::isdigit(static_cast<unsigned char>(expr[i + 1])) != 0 || expr[i + 1] == '0'))) {
            size_t j = i;
            if (expr[j] == '-')
                ++j;

            if (j < expr.size() && expr[j] == '0' && j + 1 < expr.size() &&
                (expr[j + 1] == 'x' || expr[j + 1] == 'X')) {
                j += 2;
                while (j < expr.size() && std::isxdigit(static_cast<unsigned char>(expr[j])) != 0) {
                    ++j;
                }
            } else {
                while (j < expr.size() && std::isdigit(static_cast<unsigned char>(expr[j])) != 0) {
                    ++j;
                }
            }

            std::string num_str = expr.substr(i, j - i);
            char* endptr = nullptr;
            long long val = std::strtoll(num_str.c_str(), &endptr, 0);
            if (endptr == num_str.c_str()) {
                val = 0;
            }

            tokens.push_back({TokenType::NUMBER, val, "", ""});
            i = j;
            expect_number = false;
            continue;
        }

        if (std::isalpha(static_cast<unsigned char>(expr[i])) != 0 || expr[i] == '_') {
            size_t j = i;
            while (j < expr.size() &&
                   (std::isalnum(static_cast<unsigned char>(expr[j])) != 0 || expr[j] == '_')) {
                ++j;
            }
            std::string name = expr.substr(i, j - i);

            if (j + 1 < expr.size() && ((expr.substr(j, 2) == "++" || expr.substr(j, 2) == "--"))) {
                std::string op = expr.substr(j, 2);
                long long old_val = read_variable(name);
                long long new_val = old_val + (op == "++" ? 1 : -1);
                write_variable(name, new_val);
                tokens.push_back({TokenType::NUMBER, old_val, "", ""});
                i = j + 2;
                expect_number = false;
                continue;
            }

            tokens.push_back({TokenType::VARIABLE, read_variable(name), name, ""});
            i = j;
            expect_number = false;
            continue;
        }

        if (expr[i] == '(') {
            tokens.push_back({TokenType::LPAREN, 0, "", ""});
            ++i;
            expect_number = true;
            continue;
        }
        if (expr[i] == ')') {
            tokens.push_back({TokenType::RPAREN, 0, "", ""});
            ++i;
            expect_number = false;
            continue;
        }

        if (expr[i] == '?') {
            tokens.push_back({TokenType::TERNARY_Q, 0, "", "?"});
            ++i;
            expect_number = true;
            continue;
        }
        if (expr[i] == ':') {
            tokens.push_back({TokenType::TERNARY_COLON, 0, "", ":"});
            ++i;
            expect_number = true;
            continue;
        }

        if (i + 1 < expr.size()) {
            std::string two_char = expr.substr(i, 2);
            if (two_char == "==" || two_char == "!=" || two_char == "<=" || two_char == ">=" ||
                two_char == "&&" || two_char == "||" || two_char == "<<" || two_char == ">>" ||
                two_char == "++" || two_char == "--" || two_char == "**" || two_char == "+=" ||
                two_char == "-=" || two_char == "*=" || two_char == "/=" || two_char == "%=") {
                if ((two_char == "++" || two_char == "--") && expect_number) {
                    tokens.push_back({TokenType::OPERATOR, 0, "", "pre" + two_char});
                    i += 2;
                    expect_number = true;
                    continue;
                }

                tokens.push_back({TokenType::OPERATOR, 0, "", two_char});
                i += 2;
                expect_number = (two_char != "++" && two_char != "--");
                continue;
            }
        }

        char op_char = expr[i];
        if (op_char == '+' || op_char == '-' || op_char == '*' || op_char == '/' ||
            op_char == '%' || op_char == '<' || op_char == '>' || op_char == '&' ||
            op_char == '|' || op_char == '^' || op_char == '!' || op_char == '~' ||
            op_char == '=') {
            std::string op_str(1, op_char);

            if (expect_number &&
                (op_char == '+' || op_char == '-' || op_char == '!' || op_char == '~')) {
                if (op_char == '+') {
                    op_str = "unary+";
                } else if (op_char == '-') {
                    op_str = "unary-";
                }
                tokens.push_back({TokenType::OPERATOR, 0, "", op_str});
                ++i;
                expect_number = true;
                continue;
            }

            tokens.push_back({TokenType::OPERATOR, 0, "", op_str});
            ++i;
            expect_number = true;
            continue;
        }

        ++i;
    }

    return tokens;
}

void ArithmeticEvaluator::handle_assignment_operators(std::vector<Token>& tokens) {
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].type == TokenType::OPERATOR &&
            (tokens[i].op == "+=" || tokens[i].op == "-=" || tokens[i].op == "*=" ||
             tokens[i].op == "/=" || tokens[i].op == "%=" || tokens[i].op == "=")) {
            if (i > 0 && tokens[i - 1].type == TokenType::VARIABLE) {
                std::string var_name = tokens[i - 1].str_value;

                if (i + 1 < tokens.size()) {
                    long long current_val = read_variable(var_name);
                    long long assign_val = 0;

                    if (tokens[i + 1].type == TokenType::NUMBER) {
                        assign_val = tokens[i + 1].value;
                    } else if (tokens[i + 1].type == TokenType::VARIABLE) {
                        assign_val = read_variable(tokens[i + 1].str_value);
                    }

                    long long result = assign_val;
                    if (tokens[i].op == "+=") {
                        result = current_val + assign_val;
                    } else if (tokens[i].op == "-=") {
                        result = current_val - assign_val;
                    } else if (tokens[i].op == "*=") {
                        result = current_val * assign_val;
                    } else if (tokens[i].op == "/=") {
                        if (assign_val == 0)
                            throw std::runtime_error("Division by zero");
                        result = current_val / assign_val;
                    } else if (tokens[i].op == "%=") {
                        if (assign_val == 0)
                            throw std::runtime_error("Division by zero");
                        result = current_val % assign_val;
                    }

                    write_variable(var_name, result);

                    tokens[i - 1] = {TokenType::NUMBER, result, "", ""};
                    tokens.erase(tokens.begin() + i, tokens.begin() + i + 2);
                    i = i - 1;
                }
            }
        }
    }
}

void ArithmeticEvaluator::handle_increment_operators(std::vector<Token>& tokens) {
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].type == TokenType::OPERATOR &&
            (tokens[i].op == "pre++" || tokens[i].op == "pre--")) {
            if (i + 1 < tokens.size() && tokens[i + 1].type == TokenType::VARIABLE) {
                std::string var_name = tokens[i + 1].str_value;
                long long current_val = read_variable(var_name);
                long long new_val = current_val + (tokens[i].op == "pre++" ? 1 : -1);
                write_variable(var_name, new_val);

                tokens[i] = {TokenType::NUMBER, new_val, "", ""};
                tokens.erase(tokens.begin() + i + 1);
            }
        }
    }
}

std::vector<ArithmeticEvaluator::Token> ArithmeticEvaluator::infix_to_postfix(
    const std::vector<Token>& tokens) {
    std::vector<Token> output;
    std::vector<Token> operator_stack;

    for (const auto& token : tokens) {
        if (token.type == TokenType::NUMBER || token.type == TokenType::VARIABLE) {
            output.push_back(token);
        } else if (token.type == TokenType::OPERATOR) {
            while (!operator_stack.empty() && operator_stack.back().type == TokenType::OPERATOR &&
                   operator_stack.back().op != "(" &&
                   ((get_precedence(operator_stack.back().op) > get_precedence(token.op)) ||
                    (get_precedence(operator_stack.back().op) == get_precedence(token.op) &&
                     !is_right_associative(token.op)))) {
                output.push_back(operator_stack.back());
                operator_stack.pop_back();
            }
            operator_stack.push_back(token);
        } else if (token.type == TokenType::LPAREN) {
            operator_stack.push_back(token);
        } else if (token.type == TokenType::RPAREN) {
            while (!operator_stack.empty() && operator_stack.back().type != TokenType::LPAREN) {
                output.push_back(operator_stack.back());
                operator_stack.pop_back();
            }
            if (!operator_stack.empty()) {
                operator_stack.pop_back();
            }
        } else if (token.type == TokenType::TERNARY_Q) {
            while (!operator_stack.empty() && operator_stack.back().type == TokenType::OPERATOR &&
                   get_precedence(operator_stack.back().op) > get_precedence("?")) {
                output.push_back(operator_stack.back());
                operator_stack.pop_back();
            }
            operator_stack.push_back(token);
        } else if (token.type == TokenType::TERNARY_COLON) {
            while (!operator_stack.empty() && operator_stack.back().type != TokenType::TERNARY_Q) {
                output.push_back(operator_stack.back());
                operator_stack.pop_back();
            }
            if (!operator_stack.empty()) {
                operator_stack.pop_back();
                Token ternary_op;
                ternary_op.type = TokenType::OPERATOR;
                ternary_op.op = "?:";
                operator_stack.push_back(ternary_op);
            }
        }
    }

    while (!operator_stack.empty()) {
        if (operator_stack.back().type == TokenType::OPERATOR ||
            operator_stack.back().type == TokenType::TERNARY_Q) {
            output.push_back(operator_stack.back());
        }
        operator_stack.pop_back();
    }

    return output;
}

long long ArithmeticEvaluator::evaluate_postfix(const std::vector<Token>& postfix) {
    std::vector<long long> eval_stack;

    for (const auto& token : postfix) {
        if (token.type == TokenType::NUMBER) {
            eval_stack.push_back(token.value);
        } else if (token.type == TokenType::VARIABLE) {
            eval_stack.push_back(read_variable(token.str_value));
        } else if (token.type == TokenType::OPERATOR) {
            if (token.op == "unary+" || token.op == "unary-" || token.op == "!" ||
                token.op == "~") {
                if (!eval_stack.empty()) {
                    long long a = eval_stack.back();
                    eval_stack.pop_back();
                    eval_stack.push_back(apply_unary_operator(a, token.op));
                }
            } else if (token.op == "?:") {
                if (eval_stack.size() >= 3) {
                    long long false_val = eval_stack.back();
                    eval_stack.pop_back();
                    long long true_val = eval_stack.back();
                    eval_stack.pop_back();
                    long long condition = eval_stack.back();
                    eval_stack.pop_back();
                    eval_stack.push_back(condition ? true_val : false_val);
                }
            } else {
                if (eval_stack.size() >= 2) {
                    long long b = eval_stack.back();
                    eval_stack.pop_back();
                    long long a = eval_stack.back();
                    eval_stack.pop_back();
                    eval_stack.push_back(apply_binary_operator(a, b, token.op));
                }
            }
        } else if (token.type == TokenType::TERNARY_Q && token.op == "?:") {
            if (eval_stack.size() >= 3) {
                long long false_val = eval_stack.back();
                eval_stack.pop_back();
                long long true_val = eval_stack.back();
                eval_stack.pop_back();
                long long condition = eval_stack.back();
                eval_stack.pop_back();
                eval_stack.push_back(condition ? true_val : false_val);
            }
        }
    }

    return eval_stack.empty() ? 0 : eval_stack.back();
}

long long ArithmeticEvaluator::apply_binary_operator(long long a, long long b,
                                                     const std::string& op) {
    if (op == "+")
        return a + b;
    if (op == "-")
        return a - b;
    if (op == "*")
        return a * b;
    if (op == "/") {
        if (b == 0) {
            throw std::runtime_error("Division by zero");
        }
        return a / b;
    }
    if (op == "%") {
        if (b == 0) {
            throw std::runtime_error("Division by zero");
        }
        return a % b;
    }
    if (op == "==")
        return (a == b) ? 1 : 0;
    if (op == "!=")
        return (a != b) ? 1 : 0;
    if (op == "<")
        return (a < b) ? 1 : 0;
    if (op == ">")
        return (a > b) ? 1 : 0;
    if (op == "<=")
        return (a <= b) ? 1 : 0;
    if (op == ">=")
        return (a >= b) ? 1 : 0;
    if (op == "&&")
        return (a && b) ? 1 : 0;
    if (op == "||")
        return (a || b) ? 1 : 0;
    if (op == "&")
        return a & b;
    if (op == "|")
        return a | b;
    if (op == "^")
        return a ^ b;
    if (op == "<<")
        return a << b;
    if (op == ">>")
        return a >> b;
    if (op == "**") {
        long long result = 1;
        for (long long i = 0; i < b; ++i) {
            result *= a;
        }
        return result;
    }
    return 0;
}

long long ArithmeticEvaluator::apply_unary_operator(long long a, const std::string& op) {
    if (op == "unary+")
        return a;
    if (op == "unary-")
        return -a;
    if (op == "!")
        return !a ? 1 : 0;
    if (op == "~")
        return ~a;
    return a;
}

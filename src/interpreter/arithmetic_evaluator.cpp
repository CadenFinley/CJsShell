/*
  arithmetic_evaluator.cpp

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

#include "arithmetic_evaluator.h"

#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

inline std::uint64_t to_uint64(long long value) {
    std::uint64_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

inline long long from_uint64(std::uint64_t value) {
    long long result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

inline long long wrap_add(long long a, long long b) {
    return from_uint64(to_uint64(a) + to_uint64(b));
}

inline long long wrap_sub(long long a, long long b) {
    return from_uint64(to_uint64(a) - to_uint64(b));
}

inline long long wrap_mul(long long a, long long b) {
    return from_uint64(to_uint64(a) * to_uint64(b));
}

inline long long wrap_bitand(long long a, long long b) {
    return from_uint64(to_uint64(a) & to_uint64(b));
}

inline long long wrap_bitor(long long a, long long b) {
    return from_uint64(to_uint64(a) | to_uint64(b));
}

inline long long wrap_bitxor(long long a, long long b) {
    return from_uint64(to_uint64(a) ^ to_uint64(b));
}

inline long long wrap_bitnot(long long a) {
    return from_uint64(~to_uint64(a));
}

long long parse_wrapping_literal(const std::string& num_str) {
    if (num_str.empty()) {
        return 0;
    }

    const char* ptr = num_str.c_str();
    bool negative = false;
    if (*ptr == '+') {
        ++ptr;
    } else if (*ptr == '-') {
        negative = true;
        ++ptr;
    }

    int base = 10;
    if (*ptr == '0') {
        if (ptr[1] == 'x' || ptr[1] == 'X') {
            base = 16;
            ptr += 2;
        } else {
            base = 8;
            ++ptr;
        }
    }

    bool any_digit = false;
    std::uint64_t accum = 0;
    while (*ptr != '\0') {
        int digit = -1;
        unsigned char ch = static_cast<unsigned char>(*ptr);
        if (std::isdigit(ch) != 0) {
            digit = ch - '0';
        } else if (std::isalpha(ch) != 0) {
            digit = std::tolower(ch) - 'a' + 10;
        }

        if (digit < 0 || digit >= base) {
            break;
        }

        any_digit = true;
        accum = accum * static_cast<std::uint64_t>(base) + static_cast<std::uint64_t>(digit);
        ++ptr;
    }

    if (!any_digit) {
        return 0;
    }

    long long wrapped = from_uint64(accum);
    if (negative) {
        return wrap_sub(0, wrapped);
    }
    return wrapped;
}

inline bool valid_shift_amount(long long amount) {
    return amount >= 0 && amount < 64;
}

}  // namespace

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

long long ArithmeticEvaluator::fast_pow(long long base, long long exp) {
    if (exp < 0)
        return 0;
    if (exp == 0)
        return 1;

    long long result = 1;
    long long current_base = base;

    while (exp > 0) {
        if (exp & 1) {
            result = wrap_mul(result, current_base);
        }
        current_base = wrap_mul(current_base, current_base);
        exp >>= 1;
    }

    return result;
}

ArithmeticEvaluator::OperatorType ArithmeticEvaluator::string_to_operator_type(
    const std::string& op) {
    if (op.empty())
        return OperatorType::UNKNOWN;

    switch (op[0]) {
        case '+':
            if (op.size() == 1)
                return OperatorType::ADD;
            if (op == "+=")
                return OperatorType::ADD_ASSIGN;
            break;
        case '-':
            if (op.size() == 1)
                return OperatorType::SUB;
            if (op == "-=")
                return OperatorType::SUB_ASSIGN;
            break;
        case '*':
            if (op.size() == 1)
                return OperatorType::MUL;
            if (op == "*=")
                return OperatorType::MUL_ASSIGN;
            if (op == "**")
                return OperatorType::POW;
            break;
        case '/':
            if (op.size() == 1)
                return OperatorType::DIV;
            if (op == "/=")
                return OperatorType::DIV_ASSIGN;
            break;
        case '%':
            if (op.size() == 1)
                return OperatorType::MOD;
            if (op == "%=")
                return OperatorType::MOD_ASSIGN;
            break;
        case '=':
            if (op.size() == 1)
                return OperatorType::ASSIGN;
            if (op == "==")
                return OperatorType::EQ;
            break;
        case '!':
            if (op.size() == 1)
                return OperatorType::NOT;
            if (op == "!=")
                return OperatorType::NE;
            break;
        case '<':
            if (op.size() == 1)
                return OperatorType::LT;
            if (op == "<=")
                return OperatorType::LE;
            if (op == "<<")
                return OperatorType::LSHIFT;
            break;
        case '>':
            if (op.size() == 1)
                return OperatorType::GT;
            if (op == ">=")
                return OperatorType::GE;
            if (op == ">>")
                return OperatorType::RSHIFT;
            break;
        case '&':
            if (op.size() == 1)
                return OperatorType::BITAND;
            if (op == "&&")
                return OperatorType::AND;
            break;
        case '|':
            if (op.size() == 1)
                return OperatorType::BITOR;
            if (op == "||")
                return OperatorType::OR;
            break;
        case '^':
            if (op.size() == 1)
                return OperatorType::BITXOR;
            break;
        case '~':
            if (op.size() == 1)
                return OperatorType::BITNOT;
            break;
        case 'u':
            if (op == "unary+")
                return OperatorType::UNARY_PLUS;
            if (op == "unary-")
                return OperatorType::UNARY_MINUS;
            break;
        case '?':
            if (op == "?:")
                return OperatorType::TERNARY;
            break;
    }

    return OperatorType::UNKNOWN;
}

int ArithmeticEvaluator::get_precedence(OperatorType op_type) {
    static constexpr int precedence_table[] = {11, 11, 12, 12, 12, 8,  8,  9,  9,  9,  9,
                                               4,  3,  7,  5,  6,  10, 10, 13, 14, 14, 14,
                                               14, 1,  1,  1,  1,  1,  1,  2,  0};

    return precedence_table[static_cast<int>(op_type)];
}

bool ArithmeticEvaluator::is_right_associative(OperatorType op_type) {
    switch (op_type) {
        case OperatorType::POW:
        case OperatorType::ASSIGN:
        case OperatorType::ADD_ASSIGN:
        case OperatorType::SUB_ASSIGN:
        case OperatorType::MUL_ASSIGN:
        case OperatorType::DIV_ASSIGN:
        case OperatorType::MOD_ASSIGN:
        case OperatorType::TERNARY:
            return true;
        default:
            return false;
    }
}

bool ArithmeticEvaluator::is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

std::vector<ArithmeticEvaluator::Token> ArithmeticEvaluator::tokenize(const std::string& expr) {
    std::vector<Token> tokens;
    tokens.reserve(expr.size() / 3 + 8);

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

            std::string num_str;
            num_str.assign(expr, i, j - i);
            long long val = parse_wrapping_literal(num_str);

            tokens.push_back({TokenType::NUMBER, val, "", "", OperatorType::UNKNOWN});
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
            std::string name;
            name.assign(expr, i, j - i);

            if (j + 1 < expr.size() && ((expr[j] == '+' && expr[j + 1] == '+') ||
                                        (expr[j] == '-' && expr[j + 1] == '-'))) {
                char op_char = expr[j];
                long long old_val = read_variable(name);
                long long new_val = (op_char == '+') ? wrap_add(old_val, 1) : wrap_sub(old_val, 1);
                write_variable(name, new_val);
                tokens.push_back({TokenType::NUMBER, old_val, "", "", OperatorType::UNKNOWN});
                i = j + 2;
                expect_number = false;
                continue;
            }

            tokens.push_back(
                {TokenType::VARIABLE, read_variable(name), name, "", OperatorType::UNKNOWN});
            i = j;
            expect_number = false;
            continue;
        }

        if (expr[i] == '(') {
            tokens.push_back({TokenType::LPAREN, 0, "", "", OperatorType::UNKNOWN});
            ++i;
            expect_number = true;
            continue;
        }
        if (expr[i] == ')') {
            tokens.push_back({TokenType::RPAREN, 0, "", "", OperatorType::UNKNOWN});
            ++i;
            expect_number = false;
            continue;
        }

        if (expr[i] == '?') {
            tokens.push_back({TokenType::TERNARY_Q, 0, "", "?", OperatorType::TERNARY});
            ++i;
            expect_number = true;
            continue;
        }
        if (expr[i] == ':') {
            tokens.push_back({TokenType::TERNARY_COLON, 0, "", ":", OperatorType::TERNARY});
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
                    std::string pre_op = "pre" + two_char;
                    tokens.push_back(
                        {TokenType::OPERATOR, 0, "", pre_op, string_to_operator_type(pre_op)});
                    i += 2;
                    expect_number = true;
                    continue;
                }

                tokens.push_back(
                    {TokenType::OPERATOR, 0, "", two_char, string_to_operator_type(two_char)});
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
                tokens.push_back(
                    {TokenType::OPERATOR, 0, "", op_str, string_to_operator_type(op_str)});
                ++i;
                expect_number = true;
                continue;
            }

            tokens.push_back({TokenType::OPERATOR, 0, "", op_str, string_to_operator_type(op_str)});
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
                        result = wrap_add(current_val, assign_val);
                    } else if (tokens[i].op == "-=") {
                        result = wrap_sub(current_val, assign_val);
                    } else if (tokens[i].op == "*=") {
                        result = wrap_mul(current_val, assign_val);
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
                long long new_val =
                    (tokens[i].op == "pre++") ? wrap_add(current_val, 1) : wrap_sub(current_val, 1);
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

    output.reserve(tokens.size());
    operator_stack.reserve(tokens.size() / 4 + 1);

    for (const auto& token : tokens) {
        if (token.type == TokenType::NUMBER || token.type == TokenType::VARIABLE) {
            output.push_back(token);
        } else if (token.type == TokenType::OPERATOR) {
            while (
                !operator_stack.empty() && operator_stack.back().type == TokenType::OPERATOR &&
                operator_stack.back().op != "(" &&
                ((get_precedence(operator_stack.back().op_type) > get_precedence(token.op_type)) ||
                 (get_precedence(operator_stack.back().op_type) == get_precedence(token.op_type) &&
                  !is_right_associative(token.op_type)))) {
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
                   get_precedence(operator_stack.back().op_type) >
                       get_precedence(OperatorType::TERNARY)) {
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
                ternary_op.op_type = OperatorType::TERNARY;
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
    eval_stack.reserve(postfix.size() / 2 + 1);

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

                    eval_stack.push_back(apply_unary_operator(a, token.op_type));
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

                    eval_stack.push_back(apply_binary_operator(a, b, token.op_type));
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
                                                     OperatorType op_type) {
    switch (op_type) {
        case OperatorType::ADD:
            return wrap_add(a, b);
        case OperatorType::SUB:
            return wrap_sub(a, b);
        case OperatorType::MUL:
            return wrap_mul(a, b);
        case OperatorType::DIV:
            if (b == 0)
                throw std::runtime_error("Division by zero");
            if (b == -1 && a == std::numeric_limits<long long>::min())
                return a;
            return a / b;
        case OperatorType::MOD:
            if (b == 0)
                throw std::runtime_error("Division by zero");
            if (b == -1 && a == std::numeric_limits<long long>::min())
                return 0;
            return a % b;
        case OperatorType::EQ:
            return (a == b) ? 1 : 0;
        case OperatorType::NE:
            return (a != b) ? 1 : 0;
        case OperatorType::LT:
            return (a < b) ? 1 : 0;
        case OperatorType::GT:
            return (a > b) ? 1 : 0;
        case OperatorType::LE:
            return (a <= b) ? 1 : 0;
        case OperatorType::GE:
            return (a >= b) ? 1 : 0;
        case OperatorType::AND:
            return (a && b) ? 1 : 0;
        case OperatorType::OR:
            return (a || b) ? 1 : 0;
        case OperatorType::BITAND:
            return wrap_bitand(a, b);
        case OperatorType::BITOR:
            return wrap_bitor(a, b);
        case OperatorType::BITXOR:
            return wrap_bitxor(a, b);
        case OperatorType::LSHIFT: {
            if (!valid_shift_amount(b))
                return 0;
            std::uint64_t shifted = to_uint64(a) << static_cast<unsigned>(b);
            return from_uint64(shifted);
        }
        case OperatorType::RSHIFT: {
            if (!valid_shift_amount(b))
                return a < 0 ? -1 : 0;
            return a >> static_cast<unsigned>(b);
        }
        case OperatorType::POW:

            return fast_pow(a, b);
        default:
            return 0;
    }
}

long long ArithmeticEvaluator::apply_unary_operator(long long a, OperatorType op_type) {
    switch (op_type) {
        case OperatorType::UNARY_PLUS:
            return a;
        case OperatorType::UNARY_MINUS:
            return wrap_sub(0, a);
        case OperatorType::NOT:
            return !a ? 1 : 0;
        case OperatorType::BITNOT:
            return wrap_bitnot(a);
        default:
            return a;
    }
}

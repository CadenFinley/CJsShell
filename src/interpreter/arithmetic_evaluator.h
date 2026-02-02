/*
  arithmetic_evaluator.h

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

#pragma once

#include <cstdint>
#include <functional>
#include <string>

class ArithmeticEvaluator {
   public:
    using VariableReader = std::function<long long(const std::string&)>;
    using VariableWriter = std::function<void(const std::string&, long long)>;

    ArithmeticEvaluator(VariableReader var_reader, VariableWriter var_writer);
    long long evaluate(const std::string& expr);

   private:
    enum class TokenType : std::uint8_t {
        NUMBER,
        OPERATOR,
        VARIABLE,
        LPAREN,
        RPAREN,
        TERNARY_Q,
        TERNARY_COLON
    };

    enum class OperatorType : std::uint8_t {
        ADD,
        SUB,
        MUL,
        DIV,
        MOD,
        EQ,
        NE,
        LT,
        GT,
        LE,
        GE,
        AND,
        OR,
        BITAND,
        BITOR,
        BITXOR,
        LSHIFT,
        RSHIFT,
        POW,
        NOT,
        BITNOT,
        UNARY_PLUS,
        UNARY_MINUS,
        ASSIGN,
        ADD_ASSIGN,
        SUB_ASSIGN,
        MUL_ASSIGN,
        DIV_ASSIGN,
        MOD_ASSIGN,
        TERNARY,
        UNKNOWN
    };

    struct Token {
        TokenType type{};
        long long value{};
        std::string str_value;
        std::string op;
        OperatorType op_type{OperatorType::UNKNOWN};
    };

    VariableReader read_variable;
    VariableWriter write_variable;

    std::vector<Token> tokenize(const std::string& expr);

    static int get_precedence(OperatorType op_type);
    static bool is_right_associative(OperatorType op_type);
    static OperatorType string_to_operator_type(const std::string& op);

    std::vector<Token> infix_to_postfix(const std::vector<Token>& tokens);
    long long evaluate_postfix(const std::vector<Token>& postfix);

    static long long apply_binary_operator(long long a, long long b, OperatorType op_type);
    static long long apply_unary_operator(long long a, OperatorType op_type);
    static long long fast_pow(long long base, long long exp);

    void handle_assignment_operators(std::vector<Token>& tokens);
    void handle_increment_operators(std::vector<Token>& tokens);

    static bool is_space(char c);
};

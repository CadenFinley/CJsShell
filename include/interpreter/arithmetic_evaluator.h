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

    struct Token {
        TokenType type{};
        long long value{};
        std::string str_value;
        std::string op;
    };

    VariableReader read_variable;
    VariableWriter write_variable;

    std::vector<Token> tokenize(const std::string& expr);

    static int get_precedence(const std::string& op);
    static bool is_right_associative(const std::string& op);

    std::vector<Token> infix_to_postfix(const std::vector<Token>& tokens);
    long long evaluate_postfix(const std::vector<Token>& postfix);

    static long long apply_binary_operator(long long a, long long b, const std::string& op);
    static long long apply_unary_operator(long long a, const std::string& op);

    void handle_assignment_operators(std::vector<Token>& tokens);
    void handle_increment_operators(std::vector<Token>& tokens);

    static bool is_space(char c);
};

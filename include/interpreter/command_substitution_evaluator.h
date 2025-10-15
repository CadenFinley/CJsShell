#pragma once

#include <functional>
#include <string>
#include <vector>
class CommandSubstitutionEvaluator {
   public:
    struct ExpansionResult {
        std::string text;
        std::vector<std::string> outputs;
    };
    using CommandExecutor = std::function<std::string(const std::string&)>;
    explicit CommandSubstitutionEvaluator(CommandExecutor executor);
    ExpansionResult expand_substitutions(const std::string& input);
    std::string capture_command_output(const std::string& command);

    static std::optional<size_t> find_matching_paren(const std::string& text, size_t start_index);

    static CommandExecutor create_command_executor(
        const std::function<int(const std::string&)>& executor);

   private:
    bool find_matching_delimiter(const std::string& text, size_t start, char open_char,
                                 char close_char, size_t& end_out);

    std::string escape_for_double_quotes(const std::string& content);

    bool try_handle_arithmetic_expansion(const std::string& input, size_t& i,
                                         std::string& output_text);

    bool try_handle_command_substitution(const std::string& input, size_t& i,
                                         ExpansionResult& result, bool in_double_quotes);

    bool try_handle_backtick_substitution(const std::string& input, size_t& i,
                                          ExpansionResult& result, bool in_double_quotes);

    bool try_handle_parameter_expansion(const std::string& input, size_t& i,
                                        std::string& output_text);

    size_t find_closing_backtick(const std::string& input, size_t start);

    void append_substitution_result(const std::string& content, bool in_double_quotes,
                                    std::string& output);

    bool handle_escape_sequence(char c, bool& escaped, std::string& output);

    bool handle_quote_toggle(char c, bool in_single_quotes, bool& in_quotes, char& quote_char,
                             std::string& output);

    CommandExecutor command_executor_;
    static constexpr const char* SUBST_LITERAL_START = "\x1E__SUBST_LITERAL_START__\x1E";
    static constexpr const char* SUBST_LITERAL_END = "\x1E__SUBST_LITERAL_END__\x1E";
    static constexpr const char* NOENV_START = "\x1E__NOENV_START__\x1E";
    static constexpr const char* NOENV_END = "\x1E__NOENV_END__\x1E";
};

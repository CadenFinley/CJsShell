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

   private:
    bool find_matching_delimiter(const std::string& text, size_t start, char open_char,
                                 char close_char, size_t& end_out);

    std::string escape_for_double_quotes(const std::string& content);

    CommandExecutor command_executor_;
    static constexpr const char* SUBST_LITERAL_START = "\x1E__SUBST_LITERAL_START__\x1E";
    static constexpr const char* SUBST_LITERAL_END = "\x1E__SUBST_LITERAL_END__\x1E";
    static constexpr const char* NOENV_START = "\x1E__NOENV_START__\x1E";
    static constexpr const char* NOENV_END = "\x1E__NOENV_END__\x1E";
};

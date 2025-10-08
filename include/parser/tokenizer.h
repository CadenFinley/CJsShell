#pragma once

#include <string>
#include <vector>

class Shell;

class Tokenizer {
   public:
    static std::vector<std::string> tokenize_command(const std::string& cmdline);
    static std::vector<std::string> merge_redirection_tokens(
        const std::vector<std::string>& tokens);

    std::vector<std::string> split_by_ifs(const std::string& input, Shell* shell);

   private:
    static bool looks_like_assignment(const std::string& input);
};

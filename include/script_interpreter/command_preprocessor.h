#pragma once

#include <cstdint>
#include <map>
#include <string>

class CommandPreprocessor {
   public:
    struct PreprocessedCommand {
        std::string processed_text;
        std::map<std::string, std::string> here_documents;
        bool has_subshells = false;
        bool needs_special_handling = false;
    };

    static PreprocessedCommand preprocess(const std::string& command);

   private:
    static std::string process_here_documents(const std::string& command,
                                              std::map<std::string, std::string>& here_docs);

    static std::string process_subshells(const std::string& command);

    static std::string generate_placeholder();

    static size_t find_matching_paren(const std::string& text, size_t start_pos);

    static size_t find_matching_brace(const std::string& text, size_t start_pos);

    static bool is_inside_quotes(const std::string& text, size_t pos);

    static std::uint32_t next_placeholder_id();
};

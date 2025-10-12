#pragma once

#include <string>
#include <vector>

class HistoryExpansion {
   public:
    struct ExpansionResult {
        std::string expanded_command;
        bool was_expanded = false;
        bool should_echo = true;
        std::string error_message;
        bool has_error = false;
    };

    static ExpansionResult expand(const std::string& command,
                                  const std::vector<std::string>& history_entries);
    static bool contains_history_expansion(const std::string& command);
    static std::string get_history_file_path();
    static std::vector<std::string> read_history_entries();

   private:
    static bool expand_double_bang(const std::string& command, size_t& pos,
                                   const std::vector<std::string>& history, std::string& result,
                                   std::string& error);

    static bool expand_history_number(const std::string& command, size_t& pos,
                                      const std::vector<std::string>& history, std::string& result,
                                      std::string& error);

    static bool expand_history_search(const std::string& command, size_t& pos,
                                      const std::vector<std::string>& history, std::string& result,
                                      std::string& error);

    static bool expand_quick_substitution(const std::string& command,
                                          const std::vector<std::string>& history,
                                          std::string& result, std::string& error);

    static bool expand_word_designator(const std::string& command, size_t& pos,
                                       const std::string& referenced_command, std::string& result,
                                       std::string& error);

    static std::vector<std::string> split_into_words(const std::string& command);
    static bool is_word_char(char c);
    static bool is_inside_quotes(const std::string& text, size_t pos);
    static std::string get_word_from_command(const std::string& command, int word_index);
    static std::string get_words_range(const std::string& command, int start, int end);
};

#pragma once

#include <shared_mutex>
#include <string>
#include <unordered_set>

#include "isocline/isocline.h"

class SyntaxHighlighter {
   public:
    static void initialize();
    static void refresh_executables_cache();

    static void highlight(ic_highlight_env_t* henv, const char* input, void* arg);
    static void initialize_syntax_highlighting();

   private:
    static const std::unordered_set<std::string> basic_unix_commands_;  // NOLINT
    static std::unordered_set<std::string> external_executables_;       // NOLINT
    static std::shared_mutex external_cache_mutex_;                     // NOLINT
    static const std::unordered_set<std::string> command_operators_;    // NOLINT
    static const std::unordered_set<std::string> shell_keywords_;       // NOLINT
    static const std::unordered_set<std::string> shell_built_ins_;      // NOLINT

    static bool is_external_command(const std::string& token);
    static bool is_shell_keyword(const std::string& token);
    static bool is_shell_builtin(const std::string& token);
    static bool is_variable_reference(const std::string& token);
    static bool is_quoted_string(const std::string& token, char& quote_type);
    static bool is_redirection_operator(const std::string& token);
    static bool is_glob_pattern(const std::string& token);
    static bool is_option(const std::string& token);
    static bool is_numeric_literal(const std::string& token);
    static bool is_function_definition(const std::string& input, size_t& func_name_start,
                                       size_t& func_name_end);
    static void highlight_quotes_and_variables(ic_highlight_env_t* henv, const char* input,
                                               size_t start, size_t length);
    static void highlight_variable_assignment(ic_highlight_env_t* henv, const char* input,
                                              size_t absolute_start, const std::string& token);
    static void highlight_assignment_value(ic_highlight_env_t* henv, const char* input,
                                           size_t absolute_start, const std::string& value);
};

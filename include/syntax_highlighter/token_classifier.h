#pragma once

#include <shared_mutex>
#include <string>
#include <unordered_set>

namespace token_classifier {

extern std::unordered_set<std::string> external_executables_;
extern std::shared_mutex external_cache_mutex_;

void initialize_external_cache();
void refresh_executables_cache();

bool is_external_command(const std::string& token);
bool is_shell_keyword(const std::string& token);
bool is_shell_builtin(const std::string& token);
bool is_variable_reference(const std::string& token);
bool is_quoted_string(const std::string& token, char& quote_type);
bool is_redirection_operator(const std::string& token);
bool is_glob_pattern(const std::string& token);
bool is_option(const std::string& token);
bool is_numeric_literal(const std::string& token);
bool is_function_definition(const std::string& input, size_t& func_name_start,
                            size_t& func_name_end);

}  // namespace token_classifier

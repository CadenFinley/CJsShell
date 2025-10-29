#pragma once

#include <string>

namespace token_classifier {

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

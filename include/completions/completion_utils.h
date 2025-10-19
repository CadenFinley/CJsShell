#pragma once

#include <string>
#include <vector>

extern bool g_completion_case_sensitive;

namespace completion_utils {

std::string quote_path_if_needed(const std::string& path);
std::string unquote_path(const std::string& path);
std::vector<std::string> tokenize_command_line(const std::string& line);
size_t find_last_unquoted_space(const std::string& str);
std::string normalize_for_comparison(const std::string& value);

bool starts_with_case_insensitive(const std::string& str, const std::string& prefix);
bool starts_with_case_sensitive(const std::string& str, const std::string& prefix);
bool matches_completion_prefix(const std::string& str, const std::string& prefix);
bool equals_completion_token(const std::string& value, const std::string& target);
bool starts_with_token(const std::string& value, const std::string& target_prefix);

}  

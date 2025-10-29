#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace token_constants {

extern const std::unordered_set<std::string> comparison_operators;
extern const std::unordered_set<std::string> command_operators;
extern const std::unordered_set<std::string> shell_keywords;
extern const std::unordered_set<std::string> redirection_operators;
extern const std::unordered_map<std::string, std::string> default_styles;
}  // namespace token_constants

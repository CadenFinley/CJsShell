#pragma once

#include <string>
#include <unordered_set>
#include <unordered_map>

namespace token_constants {

extern const std::unordered_set<std::string> comparison_operators;
extern const std::unordered_set<std::string> basic_unix_commands;
extern const std::unordered_set<std::string> command_operators;
extern const std::unordered_set<std::string> shell_keywords;
extern const std::unordered_set<std::string> shell_built_ins;
extern const std::unordered_map<std::string, std::string> default_styles;
}  // namespace token_constants

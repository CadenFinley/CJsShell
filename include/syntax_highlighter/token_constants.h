#pragma once

#include <string>
#include <unordered_set>

namespace token_constants {

extern const std::unordered_set<std::string> comparison_operators;
extern const std::unordered_set<std::string> basic_unix_commands;
extern const std::unordered_set<std::string> command_operators;
extern const std::unordered_set<std::string> shell_keywords;
extern const std::unordered_set<std::string> shell_built_ins;

}  // namespace token_constants

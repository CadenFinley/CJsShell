#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace token_constants {

const std::unordered_set<std::string>& comparison_operators();
const std::unordered_set<std::string>& shell_keywords();
const std::unordered_set<std::string>& redirection_operators();
const std::unordered_map<std::string, std::string>& default_styles();
}  // namespace token_constants

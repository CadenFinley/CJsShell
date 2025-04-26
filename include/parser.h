#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

/**
 * @brief Parses a command string into its constituent arguments.
 *
 * Splits the input command into a vector of arguments, applying environment variable substitutions and resolving aliases as needed.
 *
 * @param command The input command string to parse.
 * @return std::vector<std::string> The parsed arguments.
 */

class Parser {
public:
  std::vector<std::string> parse_command(const std::string& command);
};
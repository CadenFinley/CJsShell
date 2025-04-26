#include "parser.h"

/**
 * @brief Splits a command string into whitespace-separated tokens.
 *
 * Parses the input command string and returns a vector containing each argument as a separate string, using whitespace as the delimiter.
 *
 * @param command The command line string to parse.
 * @return std::vector<std::string> List of parsed arguments.
 */
std::vector<std::string> Parser::parse_command(const std::string& command) {
  std::vector<std::string> args;
  std::istringstream iss(command);
  std::string arg;
  
  while (iss >> arg) {
    args.push_back(arg);
  }
  
  return args;
}
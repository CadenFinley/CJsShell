#include "parser.h"

std::vector<std::string> Parser::parse_command(const std::string& command) {
  std::vector<std::string> args;
  std::istringstream iss(command);
  std::string arg;
  
  while (iss >> arg) {
    args.push_back(arg);
  }
  
  return args;
}
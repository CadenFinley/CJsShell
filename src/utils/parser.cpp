#include "parser.h"

std::vector<std::string> Parser::parse_command(const std::string& command) {
  std::vector<std::string> args;
  std::string current_arg;
  bool in_quotes = false;
  char quote_char = '\0';
  
  for (size_t i = 0; i < command.length(); i++) {
    char c = command[i];
    
    // Handle quotes
    if ((c == '"' || c == '\'') && (i == 0 || command[i-1] != '\\')) {
      if (!in_quotes) {
        in_quotes = true;
        quote_char = c;
      } else if (c == quote_char) {
        in_quotes = false;
        quote_char = '\0';
      } else {
        current_arg += c;
      }
      continue;
    }
    
    // Handle spaces
    if (c == ' ' && !in_quotes && current_arg.length() > 0) {
      args.push_back(current_arg);
      current_arg.clear();
      // Skip consecutive spaces
      while (i+1 < command.length() && command[i+1] == ' ') i++;
      continue;
    }
    
    // Handle escaped characters
    if (c == '\\' && i+1 < command.length()) {
      current_arg += command[i+1];
      i++;
      continue;
    }
    
    // Normal character
    if (c != ' ' || in_quotes) {
      current_arg += c;
    }
  }
  
  // Add the last argument if it exists
  if (current_arg.length() > 0) {
    args.push_back(current_arg);
  }
  
  return args;
}
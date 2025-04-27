#include "parser.h"
#include <regex>
#include <cstdlib>

std::vector<std::string> Parser::parse_command(const std::string& command) {
  std::string expanded_command = command;
  
  std::string first_word;
  size_t space_pos = command.find(' ');
  if (space_pos != std::string::npos) {
    first_word = command.substr(0, space_pos);
  } else {
    first_word = command;
  }
  
  auto alias_it = aliases.find(first_word);
  if (alias_it != aliases.end()) {
    if (space_pos != std::string::npos) {
      expanded_command = alias_it->second + command.substr(space_pos);
    } else {
      expanded_command = alias_it->second;
    }
  }
  
  std::vector<std::string> args;
  std::string current_arg;
  bool in_quotes = false;
  char quote_char = '\0';
  
  for (size_t i = 0; i < expanded_command.length(); i++) {
    char c = expanded_command[i];
    
    if ((c == '"' || c == '\'') && (i == 0 || expanded_command[i-1] != '\\')) {
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
    
    if (c == ' ' && !in_quotes && current_arg.length() > 0) {
      args.push_back(current_arg);
      current_arg.clear();
      while (i+1 < expanded_command.length() && expanded_command[i+1] == ' ') i++;
      continue;
    }
    
    if (c == '\\' && i+1 < expanded_command.length()) {
      current_arg += expanded_command[i+1];
      i++;
      continue;
    }
    
    if (c != ' ' || in_quotes) {
      current_arg += c;
    }
  }
  
  if (current_arg.length() > 0) {
    args.push_back(current_arg);
  }
  
  for (auto& arg : args) {
    expand_env_vars(arg);
  }
  
  return args;
}

void Parser::expand_env_vars(std::string& arg) {
  size_t pos = 0;
  while ((pos = arg.find('$', pos)) != std::string::npos) {
    // Skip if escaped
    if (pos > 0 && arg[pos-1] == '\\') {
      pos++;
      continue;
    }
    
    std::string var_name;
    size_t var_end;
    
    if (pos + 1 < arg.length() && arg[pos + 1] == '{') {
      var_end = arg.find('}', pos + 2);
      if (var_end == std::string::npos) {
        pos++;
        continue;
      }
      var_name = arg.substr(pos + 2, var_end - (pos + 2));
      var_end++;
    } else {
      var_end = pos + 1;
      while (var_end < arg.length() && (isalnum(arg[var_end]) || arg[var_end] == '_')) {
        var_end++;
      }
      var_name = arg.substr(pos + 1, var_end - (pos + 1));
    }
    
    if (var_name.empty()) {
      pos++;
      continue;
    }
    

    auto it = env_vars.find(var_name);
    if (it != env_vars.end()) {
      arg.replace(pos, var_end - pos, it->second);
      pos += it->second.length();
    } else {
      const char* env_value = std::getenv(var_name.c_str());
      if (env_value != nullptr) {
        arg.replace(pos, var_end - pos, env_value);
        pos += strlen(env_value);
      } else {
        arg.erase(pos, var_end - pos);
      }
    }
  }
}
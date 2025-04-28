#include "parser.h"
#include <regex>
#include <cstdlib>
#include <glob.h>
#include <memory>
#include <array>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

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
        pos = var_end;
      }
    }
  }
}

std::vector<Command> Parser::parse_pipeline(const std::string& command) {
  std::vector<Command> pipeline;

  std::vector<std::string> commands;
  std::string current;
  bool in_quotes = false;
  char quote_char = '\0';
  
  for (size_t i = 0; i < command.length(); i++) {
    char c = command[i];
    
    if ((c == '"' || c == '\'') && (i == 0 || command[i-1] != '\\')) {
      if (!in_quotes) {
        in_quotes = true;
        quote_char = c;
      } else if (c == quote_char) {
        in_quotes = false;
        quote_char = '\0';
      }
    }
    
    if (c == '|' && !in_quotes) {
      commands.push_back(current);
      current.clear();
    } else {
      current += c;
    }
  }
  
  if (!current.empty()) {
    commands.push_back(current);
  }

  for (const auto& cmd : commands) {
    Command parsed_cmd;
    std::string processed_cmd = cmd;

    if (!processed_cmd.empty() && processed_cmd.back() == '&') {
      parsed_cmd.background = true;
      processed_cmd.pop_back();
      while (!processed_cmd.empty() && std::isspace(processed_cmd.back())) {
        processed_cmd.pop_back();
      }
    }

    std::string remaining = processed_cmd;
    std::string token;
    std::vector<std::string> tokens;
    
    in_quotes = false;
    quote_char = '\0';
    current.clear();
    
    for (size_t i = 0; i < remaining.length(); i++) {
      char c = remaining[i];
      
      if ((c == '"' || c == '\'') && (i == 0 || remaining[i-1] != '\\')) {
        if (!in_quotes) {
          in_quotes = true;
          quote_char = c;
        } else if (c == quote_char) {
          in_quotes = false;
          quote_char = '\0';
        }
        current += c;
        continue;
      }
      
      if (!in_quotes && (c == '<' || c == '>')) {
        if (!current.empty()) {
          tokens.push_back(current);
          current.clear();
        }
        
        if (c == '<') {
          tokens.push_back("<");
        } else if (c == '>' && i + 1 < remaining.length() && remaining[i + 1] == '>') {
          tokens.push_back(">>");
          i++;
        } else {
          tokens.push_back(">");
        }
      } else if (!in_quotes && std::isspace(c)) {
        if (!current.empty()) {
          tokens.push_back(current);
          current.clear();
        }
      } else {
        current += c;
      }
    }
    
    if (!current.empty()) {
      tokens.push_back(current);
    }

    for (size_t i = 0; i < tokens.size(); i++) {
      if (tokens[i] == "<") {
        if (i + 1 < tokens.size()) {
          parsed_cmd.input_file = tokens[i + 1];
          i++;
        }
      } else if (tokens[i] == ">") {
        if (i + 1 < tokens.size()) {
          parsed_cmd.output_file = tokens[i + 1];
          i++;
        }
      } else if (tokens[i] == ">>") {
        if (i + 1 < tokens.size()) {
          parsed_cmd.append_file = tokens[i + 1];
          i++;
        }
      } else {
        if (tokens[i].find_first_of("*?") != std::string::npos) {
          auto expanded = expand_wildcards(tokens[i]);
          if (!expanded.empty()) {
            parsed_cmd.args.insert(parsed_cmd.args.end(), expanded.begin(), expanded.end());
          } else {
            parsed_cmd.args.push_back(tokens[i]);
          }
        } else {
          std::string arg = tokens[i];
          expand_env_vars(arg);
          parsed_cmd.args.push_back(arg);
        }
      }
    }
    
    pipeline.push_back(parsed_cmd);
  }
  
  return pipeline;
}

std::vector<std::string> Parser::expand_wildcards(const std::string& pattern) {
  glob_t globbuf;
  std::vector<std::string> result;
  
  int ret = glob(pattern.c_str(), GLOB_TILDE | GLOB_BRACE, nullptr, &globbuf);
  if (ret == 0) {
    for (size_t i = 0; i < globbuf.gl_pathc; i++) {
      result.push_back(globbuf.gl_pathv[i]);
    }
    globfree(&globbuf);
  }
  
  return result;
}

std::vector<LogicalCommand> Parser::parse_logical_commands(const std::string& command) {
  std::vector<LogicalCommand> result;
  
  std::string current_cmd;
  bool in_quotes = false;
  char quote_char = '\0';
  
  for (size_t i = 0; i < command.length(); i++) {
    char c = command[i];

    if ((c == '"' || c == '\'') && (i == 0 || command[i-1] != '\\')) {
      if (!in_quotes) {
        in_quotes = true;
        quote_char = c;
      } else if (c == quote_char) {
        in_quotes = false;
        quote_char = '\0';
      }
      current_cmd += c;
      continue;
    }
    
    if (!in_quotes && i + 1 < command.length()) {
      if ((c == '&' && command[i + 1] == '&') || 
          (c == '|' && command[i + 1] == '|' && (i == 0 || command[i-1] != '|'))) {
        
        std::string trimmed_cmd = current_cmd;
        while (!trimmed_cmd.empty() && std::isspace(trimmed_cmd.back())) {
          trimmed_cmd.pop_back();
        }
        
        if (!trimmed_cmd.empty()) {
          LogicalCommand cmd_segment;
          cmd_segment.command = trimmed_cmd;
          cmd_segment.op = std::string(1, c) + c;
          result.push_back(cmd_segment);
        }
        
        i++;
        current_cmd.clear();
        continue;
      }
    }
    
    current_cmd += c;
  }
  
  if (!current_cmd.empty()) {
    std::string trimmed_cmd = current_cmd;
    while (!trimmed_cmd.empty() && std::isspace(trimmed_cmd.back())) {
      trimmed_cmd.pop_back();
    }
    
    if (!trimmed_cmd.empty()) {
      LogicalCommand cmd_segment;
      cmd_segment.command = trimmed_cmd;
      cmd_segment.op = "";
      result.push_back(cmd_segment);
    }
  }
  
  return result;
}
#include "parser.h"
#include "main.h"
#include <regex>
#include <cstdlib>
#include <glob.h>
#include <memory>
#include <array>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>

std::vector<std::string> Parser::parse_command(const std::string& cmdline) {
  std::vector<std::string> args;
  GError* error = nullptr;
  gchar** argv = nullptr;
  int argc = 0;
  if (!g_shell_parse_argv(cmdline.c_str(), &argc, &argv, &error)) {
    if (g_debug_mode && error) {
      std::cerr << "DEBUG: g_shell_parse_argv failed: "
                << error->message << std::endl;
      g_error_free(error);
    }
    return args;
  }
  for (int i = 0; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }
  g_strfreev(argv);
  if (!args.empty()) {
    auto alias_it = aliases.find(args[0]);
    if (alias_it != aliases.end()) {
      std::vector<std::string> alias_args;
      GError* alias_error = nullptr;
      gchar** alias_argv = nullptr;
      int alias_argc = 0;
      
      if (g_shell_parse_argv(alias_it->second.c_str(), &alias_argc, &alias_argv, &alias_error)) {
        for (int i = 0; i < alias_argc; ++i) {
          alias_args.emplace_back(alias_argv[i]);
        }
        g_strfreev(alias_argv);
        if (!alias_args.empty()) {
          std::vector<std::string> new_args;
          new_args.insert(new_args.end(), alias_args.begin(), alias_args.end());
          if (args.size() > 1) {
            new_args.insert(new_args.end(), args.begin() + 1, args.end());
          }
          
          args = new_args;
        }
      } else if (alias_error) {
        g_error_free(alias_error);
      }
    }
  }
  for (auto& arg : args) {
    expand_env_vars(arg);
  }

  std::vector<std::string> expanded_args;
  for (auto& arg : args) {
    bool has_tilde = false;
    if (!arg.empty()) {
      if (arg[0] == '~') {
        has_tilde = true;
      } else {
        for (size_t i = 1; i < arg.length(); ++i) {
          if (arg[i] == '~' && (arg[i-1] == '/' || arg[i-1] == ':')) {
            has_tilde = true;
            break;
          }
        }
      }
    }
    bool has_braces = (arg.find('{') != std::string::npos && arg.find('}') != std::string::npos);
    bool has_glob = (arg.find_first_of("*?") != std::string::npos);
    if (has_glob || has_tilde || has_braces) {
      auto ex = expand_wildcards(arg);
      if (!ex.empty()) {
        expanded_args.insert(expanded_args.end(), ex.begin(), ex.end());
      } else {
        expanded_args.push_back(arg);
      }
    } else {
      expanded_args.push_back(arg);
    }
  }
  return expanded_args;
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
      current += c;
      continue;
    }
    
    if (c == '|' && !in_quotes) {
      std::string trimmed = current;
      while (!trimmed.empty() && std::isspace(trimmed.back())) {
        trimmed.pop_back();
      }
      size_t start = 0;
      while (start < trimmed.length() && std::isspace(trimmed[start])) {
        start++;
      }
      if (start < trimmed.length()) {
        trimmed = trimmed.substr(start);
      }
      
      commands.push_back(trimmed);
      current.clear();
    } else {
      current += c;
    }
  }
  
  if (!current.empty()) {
    std::string trimmed = current;
    while (!trimmed.empty() && std::isspace(trimmed.back())) {
      trimmed.pop_back();
    }
    size_t start = 0;
    while (start < trimmed.length() && std::isspace(trimmed[start])) {
      start++;
    }
    if (start < trimmed.length()) {
      trimmed = trimmed.substr(start);
    }
    
    commands.push_back(trimmed);
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
      
      if (c == '\\' && i + 1 < remaining.length()) {
        if (in_quotes && quote_char == '\'') {
          current += c;
        } else {
          current += remaining[i+1];
          i++;
        }
        continue;
      }
      
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
        std::string arg = tokens[i];
        if (arg.length() >= 2 && 
            ((arg.front() == '"' && arg.back() == '"') || 
             (arg.front() == '\'' && arg.back() == '\''))) {
          if (arg.front() == '\'') {
            arg = arg.substr(1, arg.length() - 2);
          }
          else if (arg.front() == '"') {
            std::string unquoted = arg.substr(1, arg.length() - 2);
            arg = "";
            for (size_t j = 0; j < unquoted.length(); j++) {
              if (unquoted[j] == '\\' && j + 1 < unquoted.length()) {
                arg += unquoted[j+1];
                j++;
              } else {
                arg += unquoted[j];
              }
            }
          }
        }
        expand_env_vars(arg);
        bool has_tilde = false;
        if (!arg.empty()) {
          if (arg[0] == '~') {
            has_tilde = true;
          } else {
            for (size_t k = 1; k < arg.size(); ++k) {
              if (arg[k] == '~' && (arg[k-1] == '/' || arg[k-1] == ':')) {
                has_tilde = true;
                break;
              }
            }
          }
        }
        bool has_glob = (arg.find_first_of("*?") != std::string::npos);
        bool has_braces = (arg.find('{') != std::string::npos && arg.find('}') != std::string::npos);
        if (has_glob || has_tilde || has_braces) {
          auto ex = expand_wildcards(arg);
          if (!ex.empty()) {
            for (const auto& e : ex) {
              parsed_cmd.args.push_back(e);
            }
          } else {
            parsed_cmd.args.push_back(arg);
          }
        } else {
          parsed_cmd.args.push_back(arg);
        }
      }
    }
    
    pipeline.push_back(parsed_cmd);
  }
  
  return pipeline;
}

std::vector<std::string> Parser::expand_braces(const std::string& pattern) {
  std::vector<std::string> result;
  
  if (pattern.find('{') == std::string::npos || pattern.find('}') == std::string::npos) {
    result.push_back(pattern);
    return result;
  }
  
  size_t start = std::string::npos;
  for (size_t i = 0; i < pattern.size(); i++) {
    if (pattern[i] == '{' && (i == 0 || pattern[i-1] != '\\')) {
      start = i;
      break;
    }
  }
  
  if (start == std::string::npos) {
    result.push_back(pattern);
    return result;
  }
  
  int brace_level = 1;
  size_t end = start + 1;
  
  while (end < pattern.size() && brace_level > 0) {
    if (pattern[end] == '{' && (end == 0 || pattern[end-1] != '\\')) {
      brace_level++;
    } else if (pattern[end] == '}' && (end == 0 || pattern[end-1] != '\\')) {
      brace_level--;
    }
    end++;
    
    if (brace_level == 0) break;
  }
  
  if (brace_level != 0) {
    result.push_back(pattern);
    return result;
  }
  
  end--;
  
  std::string prefix = pattern.substr(0, start);
  std::string brace_content = pattern.substr(start + 1, end - start - 1);
  std::string suffix = pattern.substr(end + 1);
  
  std::vector<std::string> alternatives;
  brace_level = 0;
  std::string current;
  
  for (size_t i = 0; i < brace_content.size(); i++) {
    char c = brace_content[i];
    
    if (c == '{' && (i == 0 || brace_content[i-1] != '\\')) {
      brace_level++;
      current += c;
    } else if (c == '}' && (i == 0 || brace_content[i-1] != '\\')) {
      brace_level--;
      current += c;
    } else if (c == ',' && brace_level == 0 && (i == 0 || brace_content[i-1] != '\\')) {
      alternatives.push_back(current);
      current.clear();
    } else {
      current += c;
    }
  }
  
  if (!current.empty() || alternatives.empty()) {
    alternatives.push_back(current);
  }
  
  for (const auto& alt : alternatives) {
    std::string expanded = prefix + alt + suffix;
    
    std::vector<std::string> nested_expansions = expand_braces(expanded);
    result.insert(result.end(), nested_expansions.begin(), nested_expansions.end());
  }
  
  return result;
}

std::vector<std::string> Parser::expand_wildcards(const std::string& pattern) {
  bool has_braces = (pattern.find('{') != std::string::npos && pattern.find('}') != std::string::npos);
  
  std::vector<std::string> result;
  std::vector<std::string> patterns_to_expand;
  
  if (has_braces) {
    patterns_to_expand = expand_braces(pattern);
  } else {
    patterns_to_expand.push_back(pattern);
  }
  
  for (const auto& expanded_pattern : patterns_to_expand) {
    glob_t globbuf;
    int ret = glob(expanded_pattern.c_str(), GLOB_TILDE | GLOB_BRACE, nullptr, &globbuf);
    
    if (ret == 0) {
      for (size_t i = 0; i < globbuf.gl_pathc; i++) {
        result.push_back(globbuf.gl_pathv[i]);
      }
      globfree(&globbuf);
    } else if (ret == GLOB_NOMATCH) {
      if (expanded_pattern.find('~') != std::string::npos) {
        const char* home = getenv("HOME");
        if (home != nullptr) {
          std::string home_expanded = expanded_pattern;
          size_t pos = 0;
          while ((pos = home_expanded.find('~', pos)) != std::string::npos) {
            if (pos == 0 || home_expanded[pos-1] == ':' || home_expanded[pos-1] == '/') {
              if (pos + 1 == home_expanded.length() || home_expanded[pos+1] == '/' || home_expanded[pos+1] == ':') {
                home_expanded.replace(pos, 1, home);
                pos += strlen(home);
              } else {
                pos++;
              }
            } else {
              pos++;
            }
          }
          
          ret = glob(home_expanded.c_str(), GLOB_BRACE, nullptr, &globbuf);
          if (ret == 0) {
            for (size_t i = 0; i < globbuf.gl_pathc; i++) {
              result.push_back(globbuf.gl_pathv[i]);
            }
            globfree(&globbuf);
            continue;
          }
          
          result.push_back(home_expanded);
          continue;
        }
      }
      
      result.push_back(expanded_pattern);
    }
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

std::vector<std::string> Parser::parse_semicolon_commands(const std::string& command) {
  std::vector<std::string> result;
  
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
    
    if (c == ';' && !in_quotes) {
      if (!current_cmd.empty()) {
        while (!current_cmd.empty() && std::isspace(current_cmd.back())) {
          current_cmd.pop_back();
        }
        
        size_t startPos = 0;
        while (startPos < current_cmd.length() && std::isspace(current_cmd[startPos])) {
          startPos++;
        }
        
        if (startPos < current_cmd.length()) {
          result.push_back(current_cmd.substr(startPos));
        }
        current_cmd.clear();
      }
    } else {
      current_cmd += c;
    }
  }
  
  if (!current_cmd.empty()) {
    while (!current_cmd.empty() && std::isspace(current_cmd.back())) {
      current_cmd.pop_back();
    }
    
    size_t startPos = 0;
    while (startPos < current_cmd.length() && std::isspace(current_cmd[startPos])) {
      startPos++;
    }
    
    if (startPos < current_cmd.length()) {
      result.push_back(current_cmd.substr(startPos));
    }
  }
  
  return result;
}

bool Parser::is_env_assignment(const std::string& command, std::string& var_name, std::string& var_value) {
  size_t pos = command.find('=');
  if (pos == std::string::npos) {
    return false;
  }
  
  std::string name = command.substr(0, pos);
  if (name.empty() || !(std::isalpha(name[0]) || name[0] == '_')) {
    return false;
  }
  
  for (char c : name) {
    if (!(std::isalnum(c) || c == '_')) {
      return false;
    }
  }
  
  var_name = name;
  var_value = command.substr(pos + 1);
  return true;
}
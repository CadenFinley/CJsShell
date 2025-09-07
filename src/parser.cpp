#include "parser.h"

#include <fcntl.h>
#include <glob.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <regex>
#include <sstream>

#include "cjsh.h"


std::vector<std::string> Parser::parse_into_lines(const std::string& script) {
  // Split script content on unquoted newlines into logical lines.
  // Semicolons, background '&', and logical ops are handled later.
  std::vector<std::string> lines;
  size_t start = 0;
  bool in_quotes = false;
  char quote_char = '\0';

  for (size_t i = 0; i < script.size(); ++i) {
    char c = script[i];
    if (!in_quotes && (c == '"' || c == '\'')) {
      in_quotes = true;
      quote_char = c;
    } else if (in_quotes && c == quote_char) {
      in_quotes = false;
    } else if (!in_quotes && c == '\n') {
      // Extract line [start, i)
      std::string segment = script.substr(start, i - start);
      // Trim trailing CR for CRLF
      if (!segment.empty() && segment.back() == '\r') {
        segment.pop_back();
      }
      lines.push_back(segment);
      start = i + 1;
    }
  }

  if (start <= script.size()) {
    std::string tail = script.substr(start);
    if (!tail.empty() && !in_quotes) {
      if (!tail.empty() && tail.back() == '\r') {
        tail.pop_back();
      }
      lines.push_back(tail);
    } else if (!tail.empty()) {
      // Even if still in quotes (unterminated), push remainder as a line
      if (!tail.empty() && tail.back() == '\r') {
        tail.pop_back();
      }
      lines.push_back(tail);
    }
  }

  return lines;
}

std::vector<std::string> tokenize_command(const std::string& cmdline) {
  std::vector<std::string> tokens;
  std::string current_token;
  bool in_quotes = false;
  char quote_char = '\0';
  bool escaped = false;

  for (size_t i = 0; i < cmdline.length(); ++i) {
    char c = cmdline[i];

    if (escaped) {
      current_token += c;
      escaped = false;
    } else if (c == '\\' && (!in_quotes || quote_char != '\'')) {
      // Backslash escapes next character, except within single quotes
      escaped = true;
    } else if ((c == '"' || c == '\'') && !in_quotes) {
      in_quotes = true;
      quote_char = c;
    } else if (c == quote_char && in_quotes) {
      in_quotes = false;
      quote_char = '\0';
    } else if ((c == '(' || c == ')') && !in_quotes) {
      if (!current_token.empty()) {
        tokens.push_back(current_token);
        current_token.clear();
      }
      tokens.push_back(std::string(1, c));
    } else if (std::isspace(c) && !in_quotes) {
      if (!current_token.empty()) {
        tokens.push_back(current_token);
        current_token.clear();
      }
    } else {
      current_token += c;
    }
  }

  if (!current_token.empty()) {
    tokens.push_back(current_token);
  }

  return tokens;
}

std::vector<std::string> Parser::parse_command(const std::string& cmdline) {
  std::vector<std::string> args;

  try {
    args = tokenize_command(cmdline);
  } catch (const std::exception& e) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: tokenize_command failed: " << e.what() << std::endl;
    }
    return args;
  }

  if (!args.empty()) {
    auto alias_it = aliases.find(args[0]);
    if (alias_it != aliases.end()) {
      std::vector<std::string> alias_args;

      try {
        alias_args = tokenize_command(alias_it->second);

        if (!alias_args.empty()) {
          std::vector<std::string> new_args;
          new_args.insert(new_args.end(), alias_args.begin(), alias_args.end());
          if (args.size() > 1) {
            new_args.insert(new_args.end(), args.begin() + 1, args.end());
          }

          args = new_args;
        }
      } catch (const std::exception& e) {
        if (g_debug_mode) {
          std::cerr << "DEBUG: tokenize_command for alias failed: " << e.what()
                    << std::endl;
        }
      }
    }
  }

  std::vector<std::string> expanded_args;
  for (const auto& arg : args) {
    if (arg.find('{') != std::string::npos &&
        arg.find('}') != std::string::npos) {
      std::vector<std::string> brace_expansions = expand_braces(arg);
      expanded_args.insert(expanded_args.end(), brace_expansions.begin(),
                           brace_expansions.end());
    } else {
      expanded_args.push_back(arg);
    }
  }
  args = expanded_args;
  for (auto& arg : args) {
    expand_env_vars(arg);
  }
  std::vector<std::string> tilde_expanded_args;
  for (auto& arg : args) {
    bool has_tilde = false;
    if (!arg.empty()) {
      if (arg[0] == '~') {
        has_tilde = true;
      } else {
        for (size_t i = 1; i < arg.length(); ++i) {
          if (arg[i] == '~') {
            has_tilde = true;
            break;
          }
        }
      }
    }

    if (has_tilde) {
      char* home_dir = std::getenv("HOME");
      if (home_dir) {
        std::string home_str(home_dir);
        std::string expanded_arg =
            std::regex_replace(arg, std::regex("^~"), home_str);
        tilde_expanded_args.push_back(expanded_arg);
      } else {
        tilde_expanded_args.push_back(arg);
      }
    } else {
      tilde_expanded_args.push_back(arg);
    }
  }

  std::vector<std::string> final_args;
  for (const auto& arg : tilde_expanded_args) {
    auto gw = expand_wildcards(arg);
    final_args.insert(final_args.end(), gw.begin(), gw.end());
  }
  return final_args;
}
std::vector<std::string> Parser::expand_braces(const std::string& pattern) {
  std::vector<std::string> result;

  size_t open_pos = pattern.find('{');
  if (open_pos == std::string::npos) {
    result.push_back(pattern);
    return result;
  }

  int depth = 1;
  size_t close_pos = open_pos + 1;

  while (close_pos < pattern.size() && depth > 0) {
    if (pattern[close_pos] == '{') {
      depth++;
    } else if (pattern[close_pos] == '}') {
      depth--;
    }

    if (depth > 0) {
      close_pos++;
    }
  }

  if (depth != 0) {
    result.push_back(pattern);
    return result;
  }

  std::string prefix = pattern.substr(0, open_pos);
  std::string content = pattern.substr(open_pos + 1, close_pos - open_pos - 1);
  std::string suffix = pattern.substr(close_pos + 1);

  std::vector<std::string> options;
  size_t start = 0;
  depth = 0;

  for (size_t i = 0; i <= content.size(); ++i) {
    if (i == content.size() || (content[i] == ',' && depth == 0)) {
      options.push_back(content.substr(start, i - start));
      start = i + 1;
    } else if (content[i] == '{') {
      depth++;
    } else if (content[i] == '}') {
      depth--;
    }
  }

  for (const auto& option : options) {
    std::vector<std::string> expanded_results =
        expand_braces(prefix + option + suffix);
    result.insert(result.end(), expanded_results.begin(),
                  expanded_results.end());
  }

  return result;
}

void Parser::expand_env_vars(std::string& arg) {
  std::string result;
  bool in_var = false;
  std::string var_name;

  for (size_t i = 0; i < arg.length(); ++i) {
    // Handle escaped dollar (e.g., \$var): treat $ literally and remove escape
    if (!in_var && arg[i] == '$' && i > 0 && arg[i - 1] == '\\') {
      if (!result.empty() && result.back() == '\\') {
        result.pop_back();
      }
      result += '$';
      continue;
    }
    if (arg[i] == '$' && (i + 1 < arg.length()) &&
        (isalpha(arg[i + 1]) || arg[i + 1] == '_' || isdigit(arg[i + 1]))) {
      in_var = true;
      var_name.clear();
      continue;
    } else if (in_var) {
      if (isalnum(arg[i]) || arg[i] == '_' ||
          (var_name.empty() && isdigit(arg[i]))) {
        var_name += arg[i];
      } else {
        in_var = false;
        auto it = env_vars.find(var_name);
        if (it != env_vars.end()) {
          result += it->second;
        } else {
          const char* env_val = getenv(var_name.c_str());
          if (env_val) {
            result += env_val;
          }
        }
        result += arg[i];
      }
    } else {
      result += arg[i];
    }
  }

  if (in_var) {
    auto it = env_vars.find(var_name);
    if (it != env_vars.end()) {
      result += it->second;
    } else {
      const char* env_val = getenv(var_name.c_str());
      if (env_val) {
        result += env_val;
      }
    }
  }

  arg = result;
}

std::vector<Command> Parser::parse_pipeline(const std::string& command) {
  std::vector<Command> commands;
  std::vector<std::string> command_parts;

  std::string current;
  bool in_quotes = false;
  char quote_char = '\0';

  for (size_t i = 0; i < command.length(); ++i) {
    if (command[i] == '"' || command[i] == '\'') {
      if (!in_quotes) {
        in_quotes = true;
        quote_char = command[i];
      } else if (quote_char == command[i]) {
        in_quotes = false;
      }
      current += command[i];
    } else if (command[i] == '|' && !in_quotes) {
      if (!current.empty()) {
        command_parts.push_back(current);
        current.clear();
      }
    } else {
      current += command[i];
    }
  }

  if (!current.empty()) {
    command_parts.push_back(current);
  }

  for (const auto& cmd_str : command_parts) {
    Command cmd;
    std::string cmd_part = cmd_str;

    if (!cmd_part.empty() && cmd_part.back() == '&') {
      cmd.background = true;
      cmd_part.pop_back();
      cmd_part.erase(cmd_part.find_last_not_of(" \t\n\r") + 1);
    }

    std::vector<std::string> tokens = tokenize_command(cmd_part);
    std::vector<std::string> filtered_args;

    for (size_t i = 0; i < tokens.size(); ++i) {
      if (tokens[i] == "<" && i + 1 < tokens.size()) {
        cmd.input_file = tokens[++i];
      } else if (tokens[i] == ">" && i + 1 < tokens.size()) {
        cmd.output_file = tokens[++i];
      } else if (tokens[i] == ">>" && i + 1 < tokens.size()) {
        cmd.append_file = tokens[++i];
      } else {
        filtered_args.push_back(tokens[i]);
      }
    }

    cmd.args = filtered_args;

    const char* home = getenv("HOME");
    if (home) {
      std::string h(home);
      if (!cmd.input_file.empty() && cmd.input_file[0] == '~') {
        cmd.input_file = h + cmd.input_file.substr(1);
      }
      if (!cmd.output_file.empty() && cmd.output_file[0] == '~') {
        cmd.output_file = h + cmd.output_file.substr(1);
      }
      if (!cmd.append_file.empty() && cmd.append_file[0] == '~') {
        cmd.append_file = h + cmd.append_file.substr(1);
      }
    }

    commands.push_back(cmd);
  }

  return commands;
}

bool Parser::is_env_assignment(const std::string& command,
                               std::string& var_name, std::string& var_value) {
  std::regex env_regex("^\\s*([A-Za-z_][A-Za-z0-9_]*)=(.*)$");
  std::smatch match;

  if (std::regex_match(command, match, env_regex)) {
    var_name = match[1].str();
    var_value = match[2].str();

    if (var_value.size() >= 2) {
      if ((var_value.front() == '"' && var_value.back() == '"') ||
          (var_value.front() == '\'' && var_value.back() == '\'')) {
        var_value = var_value.substr(1, var_value.length() - 2);
      }
    }
    return true;
  }
  return false;
}

std::vector<LogicalCommand> Parser::parse_logical_commands(
    const std::string& command) {
  std::vector<LogicalCommand> logical_commands;
  std::string current;
  bool in_quotes = false;
  char quote_char = '\0';

  for (size_t i = 0; i < command.length(); ++i) {
    if (command[i] == '"' || command[i] == '\'') {
      if (!in_quotes) {
        in_quotes = true;
        quote_char = command[i];
      } else if (quote_char == command[i]) {
        in_quotes = false;
      }
      current += command[i];
    } else if (!in_quotes && i < command.length() - 1) {
      if (command[i] == '&' && command[i + 1] == '&') {
        if (!current.empty()) {
          logical_commands.push_back({current, "&&"});
          current.clear();
        }
        i++;
      } else if (command[i] == '|' && command[i + 1] == '|') {
        if (!current.empty()) {
          logical_commands.push_back({current, "||"});
          current.clear();
        }
        i++;
      } else {
        current += command[i];
      }
    } else {
      current += command[i];
    }
  }

  if (!current.empty()) {
    logical_commands.push_back({current, ""});
  }

  return logical_commands;
}

std::vector<std::string> Parser::parse_semicolon_commands(
    const std::string& command) {
  std::vector<std::string> commands;
  std::string current;
  bool in_quotes = false;
  char quote_char = '\0';

  for (size_t i = 0; i < command.length(); ++i) {
    if (command[i] == '"' || command[i] == '\'') {
      if (!in_quotes) {
        in_quotes = true;
        quote_char = command[i];
      } else if (quote_char == command[i]) {
        in_quotes = false;
      }
      current += command[i];
    } else if (command[i] == ';' && !in_quotes) {
      if (!current.empty()) {
        current.erase(0, current.find_first_not_of(" \t\n\r"));
        current.erase(current.find_last_not_of(" \t\n\r") + 1);
        if (!current.empty()) {
          commands.push_back(current);
        }
        current.clear();
      }
    } else {
      current += command[i];
    }
  }

  if (!current.empty()) {
    current.erase(0, current.find_first_not_of(" \t\n\r"));
    current.erase(current.find_last_not_of(" \t\n\r") + 1);
    if (!current.empty()) {
      commands.push_back(current);
    }
  }

  return commands;
}

std::vector<std::string> Parser::expand_wildcards(const std::string& pattern) {
  std::vector<std::string> result;

  if (pattern.find_first_of("*?[]") == std::string::npos) {
    result.push_back(pattern);
    return result;
  }

  glob_t glob_result;
  memset(&glob_result, 0, sizeof(glob_result));

  int return_value =
      glob(pattern.c_str(), GLOB_TILDE | GLOB_MARK, NULL, &glob_result);
  if (return_value == 0) {
    for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
      result.push_back(std::string(glob_result.gl_pathv[i]));
    }
    globfree(&glob_result);
  } else if (return_value == GLOB_NOMATCH) {
    result.push_back(pattern);
  }

  return result;
}

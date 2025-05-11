#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

std::vector<std::string> tokenize_command(const std::string& cmdline);

struct Command {
  std::vector<std::string> args;
  std::string input_file;   // < redirection
  std::string output_file;  // > redirection
  std::string append_file;  // >> redirection
  bool background = false;  // & at the end
  bool stderr_to_stdout = false; // 2>&1 redirection
  std::string here_doc;     // << HERE document
};

struct LogicalCommand {
  std::string command;
  std::string op;
};

class Parser {
 public:
  std::vector<std::string> parse_command(const std::string& cmdline);
  std::vector<Command> parse_pipeline(const std::string& command);
  std::vector<std::string> expand_wildcards(const std::string& pattern);
  std::vector<LogicalCommand> parse_logical_commands(
      const std::string& command);
  std::vector<std::string> parse_semicolon_commands(const std::string& command);
  bool is_env_assignment(const std::string& command, std::string& var_name,
                         std::string& var_value);
  void expand_env_vars(std::string& arg);

  void set_aliases(
      const std::unordered_map<std::string, std::string>& new_aliases) {
    this->aliases = new_aliases;
  }

  void set_env_vars(
      const std::unordered_map<std::string, std::string>& new_env_vars) {
    this->env_vars = new_env_vars;
  }

 private:
  std::vector<std::string> expand_braces(const std::string& pattern);
  std::unordered_map<std::string, std::string> aliases;
  std::unordered_map<std::string, std::string> env_vars;
};
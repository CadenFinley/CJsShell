#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>

// Structure to represent a command with its redirections
struct Command {
  std::vector<std::string> args;
  std::string input_file;   // < redirection
  std::string output_file;  // > redirection
  std::string append_file;  // >> redirection
  bool background = false;  // & at the end
};

// Structure to represent a logical command segment
struct LogicalCommand {
  std::string command;   // The command to execute
  std::string op;        // The logical operator: "&&", "||", or empty for the last command
};

class Parser {
public:
  std::vector<std::string> parse_command(const std::string& command);
  
  // New method to parse pipeline commands
  std::vector<Command> parse_pipeline(const std::string& command);
  
  // New method to expand wildcards
  std::vector<std::string> expand_wildcards(const std::string& pattern);

  // New method to parse commands with logical operators
  std::vector<LogicalCommand> parse_logical_commands(const std::string& command);

  void set_aliases(const std::unordered_map<std::string, std::string>& new_aliases) {
    this->aliases = new_aliases;
  }

  void set_env_vars(const std::unordered_map<std::string, std::string>& new_env_vars) {
    this->env_vars = new_env_vars;
  }

private:
  void expand_env_vars(std::string& arg);
  std::unordered_map<std::string, std::string> aliases;
  std::unordered_map<std::string, std::string> env_vars;
};
#pragma once

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "command_preprocessor.h"

class Shell;

std::vector<std::string> tokenize_command(const std::string& cmdline);

struct Command {
  std::vector<std::string> args;
  std::string input_file;         // < redirection
  std::string output_file;        // > redirection
  std::string append_file;        // >> redirection
  bool background = false;        // & at the end
  bool stderr_to_stdout = false;  // 2>&1 redirection
  bool stdout_to_stderr = false;  // >&2 redirection
  std::string stderr_file;        // 2> redirection (stderr to file)
  bool stderr_append = false;     // 2>> append redirection
  std::string here_doc;           // << HERE document
  std::string here_string;        // <<< here string
  bool both_output = false;  // &> redirection (stdout and stderr to same file)
  std::string both_output_file;  // file for &> redirection
  bool force_overwrite = false;  // >| force overwrite (noclobber bypass)

  // File descriptor redirections (fd_num -> target)
  std::map<int, std::string> fd_redirections;      // e.g., 3< file.txt
  std::map<int, int> fd_duplications;              // e.g., 2>&1, 3>&2
  std::vector<std::string> process_substitutions;  // <(cmd) or >(cmd)

  Command() {
    args.reserve(8);  // Reserve space for typical command + arguments
    process_substitutions.reserve(
        2);  // Reserve space for typical process substitutions
  }
};

struct LogicalCommand {
  std::string command;
  std::string op;
};

class Parser {
 public:
  std::vector<std::string> parse_into_lines(const std::string& scripts);

  std::vector<std::string> parse_command(const std::string& cmdline);
  std::vector<Command> parse_pipeline(const std::string& command);
  std::vector<std::string> expand_wildcards(const std::string& pattern);
  std::vector<LogicalCommand> parse_logical_commands(
      const std::string& command);
  std::vector<std::string> parse_semicolon_commands(const std::string& command);
  bool is_env_assignment(const std::string& command, std::string& var_name,
                         std::string& var_value);
  void expand_env_vars(std::string& arg);
  void expand_env_vars_selective(std::string& arg);
  std::vector<std::string> split_by_ifs(const std::string& input);

  std::vector<Command> parse_pipeline_with_preprocessing(
      const std::string& command);

  void set_aliases(
      const std::unordered_map<std::string, std::string>& new_aliases) {
    this->aliases = new_aliases;
  }

  void set_env_vars(
      const std::unordered_map<std::string, std::string>& new_env_vars) {
    this->env_vars = new_env_vars;
  }

  void set_shell(Shell* shell) {
    this->shell = shell;
  }

 private:
  std::string get_variable_value(const std::string& var_name);
  std::string resolve_parameter_value(const std::string& var_name);
  std::vector<std::string> expand_braces(const std::string& pattern);
  std::unordered_map<std::string, std::string> aliases;
  std::unordered_map<std::string, std::string> env_vars;
  Shell* shell = nullptr;

  std::map<std::string, std::string> current_here_docs;
};
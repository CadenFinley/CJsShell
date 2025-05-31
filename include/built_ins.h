#pragma once

#include <limits.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "shell.h"

class Shell;

class Built_ins {
 public:
  Built_ins();
  ~Built_ins() = default;

  void set_shell(Shell* shell_ptr) { shell = shell_ptr; }
  std::string get_current_directory() const { return current_directory; }
  std::string get_previous_directory() const { return previous_directory; }
  void set_current_directory() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
      current_directory = cwd;
    } else {
      current_directory = "/";
    }
  }

  int builtin_command(const std::vector<std::string>& args);
  int is_builtin_command(const std::string& cmd) const;
  int ai_commands(const std::vector<std::string>& args);
  int do_ai_request(const std::string& prompt);

  std::vector<std::string> get_builtin_commands() const {
    std::vector<std::string> names;
    for (auto& kv : builtins) names.push_back(kv.first);
    return names;
  }

  std::string get_last_error() const { return last_terminal_output_error; }

 private:
  std::string current_directory;
  std::string previous_directory;
  std::unordered_map<std::string,
                     std::function<int(const std::vector<std::string>&)>>
      builtins;
  Shell* shell;
  std::unordered_map<std::string, std::string> aliases;
  std::unordered_map<std::string, std::string> env_vars;
  std::string last_terminal_output_error;

  int ai_chat_commands(const std::vector<std::string>& args, int command_index);
  int handle_ai_file_commands(const std::vector<std::string>& args,
                              int command_index);
  int plugin_commands(const std::vector<std::string>& args);
  int theme_commands(const std::vector<std::string>& args);
  int update_theme_in_rc_file(const std::string& themeName);
  int uninstall_command();
  int restart_command();
  int user_commands(const std::vector<std::string>& args);
  int help_command();
  int aihelp_command(const std::vector<std::string>& args);
  int alias_command(const std::vector<std::string>& args);
  int export_command(const std::vector<std::string>& args);
  int unalias_command(const std::vector<std::string>& args);
  int unset_command(const std::vector<std::string>& args);
  int eval_command(const std::vector<std::string>& args);
  int history_command(const std::vector<std::string>& args);
  int clear_command(const std::vector<std::string>& args);
  int save_alias_to_file(const std::string& name, const std::string& value);
  int save_env_var_to_file(const std::string& name, const std::string& value);
  int remove_alias_from_file(const std::string& name);
  int remove_env_var_from_file(const std::string& name);
  int parse_assignment(const std::string& arg, std::string& name,
                       std::string& value);
};
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

  void set_shell(Shell* shell_ptr) {
    shell = shell_ptr;
  }
  std::string get_current_directory() const {
    return current_directory;
  }
  std::string get_previous_directory() const {
    return previous_directory;
  }
  void set_current_directory() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
      current_directory = cwd;
    } else {
      current_directory = "/";
    }
  }

  Shell* get_shell() {
    return shell;
  }

  int builtin_command(const std::vector<std::string>& args);
  int is_builtin_command(const std::string& cmd) const;

  std::vector<std::string> get_builtin_commands() const {
    std::vector<std::string> names;
    for (auto& kv : builtins)
      names.push_back(kv.first);
    return names;
  }

  std::string get_last_error() const {
    return last_terminal_output_error;
  }
  int do_ai_request(const std::string& prompt);

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
};
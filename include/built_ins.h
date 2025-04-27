#pragma once

#include <iostream>
#include "hash.h"
#include <filesystem>
#include <unistd.h>
#include <unordered_map>
#include <fstream>
#include <vector>
#include <string>

class Shell; // Forward declaration

class Built_ins {

public:
Built_ins(): builtins({
      {"cd", [this](const std::vector<std::string>& args) { std::string result;return change_directory(args.size() > 1 ? args[1] : current_directory, result);}},
      {"alias", [this](const std::vector<std::string>& args) { return alias_command(args); }},
      {"export", [this](const std::vector<std::string>& args) { return export_command(args); }},
      {"unset", [this](const std::vector<std::string>& args) { return unset_command(args); }},
      {"source", [this](const std::vector<std::string>& args) { return source_command(args); }},
      {"unalias", [this](const std::vector<std::string>& args) { return unalias_command(args); }},
      {"ai", [this](const std::vector<std::string>& args) { ai_commands(args); return true; }},
      {"user", [this](const std::vector<std::string>& args) { user_commands(args); return true; }},
      {"theme", [this](const std::vector<std::string>& args) { theme_commands(args); return true; }},
      {"plugin", [this](const std::vector<std::string>& args) { plugin_commands(args); return true; }},
      {"help", [this](const std::vector<std::string>&) { help_command(); return true; }},
      {"approot", [this](const std::vector<std::string>&) { approot_command(); return true; }},
      {"aihelp", [this](const std::vector<std::string>& args) { aihelp_command(args); return true; }},
      {"version", [this](const std::vector<std::string>&) { version_command(); return true; }},
      {"uninstall", [this](const std::vector<std::string>&) { uninstall_command(); return true; }},
  }), shell(nullptr) {}
  ~Built_ins() = default;

  void set_shell(Shell* shell_ptr) {
    shell = shell_ptr;
  }

  bool builtin_command(const std::vector<std::string>& args);
  bool change_directory(const std::string& dir, std::string& result);

  std::string get_current_directory() const {
    return current_directory;
  }

  void set_current_directory() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
      current_directory = cwd;
    } else {
      current_directory = "/";
    }
  }

  void ai_commands(const std::vector<std::string>& args);
  void ai_chat_commands(const std::vector<std::string>& args, int command_index);
  void handle_ai_file_commands(const std::vector<std::string>& args, int command_index);
  void do_ai_request(const std::string& prompt);
  
  // New command handlers
  void plugin_commands(const std::vector<std::string>& args);
  void theme_commands(const std::vector<std::string>& args);
  void approot_command();
  void version_command();
  void uninstall_command();
  void user_commands(const std::vector<std::string>& args);
  void help_command();
  void aihelp_command(const std::vector<std::string>& args);

  // Alias and environment variable command handlers
  bool alias_command(const std::vector<std::string>& args);
  bool unalias_command(const std::vector<std::string>& args);
  bool export_command(const std::vector<std::string>& args);
  bool unset_command(const std::vector<std::string>& args);
  bool source_command(const std::vector<std::string>& args);

  // Save settings to files
  void save_aliases_to_file();
  void save_env_vars_to_file();

private:
  std::string current_directory;
  std::unordered_map<std::string, std::function<bool(const std::vector<std::string>&)>> builtins;
  Shell* shell;
  std::unordered_map<std::string, std::string> aliases;
  std::unordered_map<std::string, std::string> env_vars;
  
  // Helper functions
  bool parse_assignment(const std::string& arg, std::string& name, std::string& value);
};
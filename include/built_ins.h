#pragma once

#include <iostream>
#include <filesystem>
#include <unistd.h>
#include <unordered_map>
#include <fstream>
#include <vector>
#include <string>
#include <functional>
#include <limits.h>

class Shell;

class Built_ins {

public:
Built_ins(): builtins({
      {"cd", [this](const std::vector<std::string>& args) { std::string result; return change_directory(args.size() > 1 ? args[1] : current_directory, result);}},
      {"alias", [this](const std::vector<std::string>& args) { return alias_command(args); }},
      {"export", [this](const std::vector<std::string>& args) { return export_command(args); }},
      {"unalias", [this](const std::vector<std::string>& args) { return unalias_command(args); }},
      {"unset", [this](const std::vector<std::string>& args) { return unset_command(args); }},
      {"ai", [this](const std::vector<std::string>& args) { return ai_commands(args); }},
      {"user", [this](const std::vector<std::string>& args) { return user_commands(args); }},
      {"theme", [this](const std::vector<std::string>& args) { return theme_commands(args); }},
      {"plugin", [this](const std::vector<std::string>& args) { return plugin_commands(args); }},
      {"help", [this](const std::vector<std::string>&) { return help_command(); }},
      {"approot", [this](const std::vector<std::string>&) { return approot_command(); }},
      {"aihelp", [this](const std::vector<std::string>& args) { return aihelp_command(args); }},
      {"version", [this](const std::vector<std::string>&) { return version_command(); }},
      {"uninstall", [this](const std::vector<std::string>&) { return uninstall_command(); }},
  }), shell(nullptr) {}
  ~Built_ins() = default;

  void set_shell(Shell* shell_ptr) {
    shell = shell_ptr;
  }
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

  bool builtin_command(const std::vector<std::string>& args);
  bool is_builtin_command(const std::string& cmd) const;
  bool change_directory(const std::string& dir, std::string& result);
  bool ai_commands(const std::vector<std::string>& args);
  void do_ai_request(const std::string& prompt);

private:
  std::string current_directory;
  std::unordered_map<std::string, std::function<bool(const std::vector<std::string>&)>> builtins;
  Shell* shell;
  std::unordered_map<std::string, std::string> aliases;
  std::unordered_map<std::string, std::string> env_vars;

  bool ai_chat_commands(const std::vector<std::string>& args, int command_index);
  bool handle_ai_file_commands(const std::vector<std::string>& args, int command_index);
  bool plugin_commands(const std::vector<std::string>& args);
  bool theme_commands(const std::vector<std::string>& args);
  void update_theme_in_rc_file(const std::string& themeName);
  bool approot_command();
  bool version_command();
  bool uninstall_command();
  bool user_commands(const std::vector<std::string>& args);
  bool help_command();
  bool aihelp_command(const std::vector<std::string>& args);
  bool alias_command(const std::vector<std::string>& args);
  bool export_command(const std::vector<std::string>& args);
  bool unalias_command(const std::vector<std::string>& args);
  bool unset_command(const std::vector<std::string>& args);
  void save_alias_to_file(const std::string& name, const std::string& value);
  void save_env_var_to_file(const std::string& name, const std::string& value);
  void remove_alias_from_file(const std::string& name);
  void remove_env_var_from_file(const std::string& name);
  bool parse_assignment(const std::string& arg, std::string& name, std::string& value);
};
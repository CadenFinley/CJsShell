#pragma once

#include <iostream>
#include "hash.h"
#include <filesystem>
#include <unistd.h>
#include <unordered_map>
#include <fstream>
#include <vector>
#include <string>

class Built_ins {

public:
Built_ins(): builtins({
      {"cd", [this](const std::vector<std::string>& args) { std::string result;return change_directory(args.size() > 1 ? args[1] : current_directory, result);}},
      {"alias", [](const std::vector<std::string>&) { /* Handle alias command */ return true; }},
      {"export", [](const std::vector<std::string>&) { /* Handle export command */ return true; }},
      {"unset", [](const std::vector<std::string>&) { /* Handle unset command */ return true; }},
      {"source", [](const std::vector<std::string>&) { /* Handle source command */ return true; }},
      {"unalias", [](const std::vector<std::string>&) { /* Handle unalias command */ return true; }},
      {"ai", [this](const std::vector<std::string>& args) { ai_commands(args); return true; }},
      {"user", [this](const std::vector<std::string>& args) { user_commands(args); return true; }},
      {"theme", [this](const std::vector<std::string>& args) { theme_commands(args); return true; }},
      {"plugin", [this](const std::vector<std::string>& args) { plugin_commands(args); return true; }},
      {"help", [this](const std::vector<std::string>&) { help_command(); return true; }},
      {"approot", [this](const std::vector<std::string>&) { approot_command(); return true; }},
      {"aihelp", [this](const std::vector<std::string>& args) { aihelp_command(args); return true; }},
      {"version", [this](const std::vector<std::string>&) { version_command(); return true; }},
      {"uninstall", [this](const std::vector<std::string>&) { uninstall_command(); return true; }},
  }) {}
  ~Built_ins() = default;


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

private:
  std::string current_directory;
  std::unordered_map<std::string, std::function<bool(const std::vector<std::string>&)>> builtins;
};
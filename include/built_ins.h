#pragma once

#include <iostream>
#include "hash.h"
#include <filesystem>
#include <unistd.h>
#include <unordered_map>

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
      {"user", [](const std::vector<std::string>&) { /* Handle user command */ return true; }},
      {"theme", [](const std::vector<std::string>&) { /* Handle theme command */ return true; }},
      {"plugin", [](const std::vector<std::string>&) { /* Handle plugin command */ return true; }},
     {"help", [](const std::vector<std::string>&) { /* Handle help command */ return true; }},


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
  void do_ai_request(const std::string& prompt);

private:
  std::string current_directory;
  Built_ins* built_ins;
  std::unordered_map<std::string, std::function<bool(const std::vector<std::string>&)>> builtins;
};
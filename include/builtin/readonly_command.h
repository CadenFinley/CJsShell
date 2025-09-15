#pragma once

#include <string>
#include <unordered_set>
#include <vector>

class Shell;

class ReadonlyManager {
 public:
  static ReadonlyManager& instance();

  void set_readonly(const std::string& name);

  bool is_readonly(const std::string& name) const;

  void remove_readonly(const std::string& name);

  std::vector<std::string> get_readonly_variables() const;

  void clear_all();

 private:
  ReadonlyManager() = default;
  std::unordered_set<std::string> readonly_vars;
};

int readonly_command(const std::vector<std::string>& args, Shell* shell);

#pragma once

#include <string>
#include <vector>
#include <unordered_set>

class Shell;

// Readonly variable manager
class ReadonlyManager {
 public:
  static ReadonlyManager& instance();
  
  // Mark a variable as readonly
  void set_readonly(const std::string& name);
  
  // Check if a variable is readonly
  bool is_readonly(const std::string& name) const;
  
  // Remove readonly status (used internally for unset)
  void remove_readonly(const std::string& name);
  
  // Get all readonly variables
  std::vector<std::string> get_readonly_variables() const;
  
  // Clear all readonly variables
  void clear_all();

 private:
  ReadonlyManager() = default;
  std::unordered_set<std::string> readonly_vars;
};

// Readonly builtin command
int readonly_command(const std::vector<std::string>& args, Shell* shell);

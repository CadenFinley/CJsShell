#include "readonly_command.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include "shell.h"

ReadonlyManager& ReadonlyManager::instance() {
  static ReadonlyManager instance;
  return instance;
}

void ReadonlyManager::set_readonly(const std::string& name) {
  readonly_vars.insert(name);
}

bool ReadonlyManager::is_readonly(const std::string& name) const {
  return readonly_vars.find(name) != readonly_vars.end();
}

void ReadonlyManager::remove_readonly(const std::string& name) {
  readonly_vars.erase(name);
}

std::vector<std::string> ReadonlyManager::get_readonly_variables() const {
  std::vector<std::string> result(readonly_vars.begin(), readonly_vars.end());
  std::sort(result.begin(), result.end());
  return result;
}

void ReadonlyManager::clear_all() {
  readonly_vars.clear();
}

int readonly_command(const std::vector<std::string>& args, Shell* shell) {
  (void)shell;  // Suppress unused parameter warning
  auto& readonly_manager = ReadonlyManager::instance();

  if (args.size() == 1) {
    // List all readonly variables
    auto readonly_vars = readonly_manager.get_readonly_variables();

    for (const std::string& var : readonly_vars) {
      const char* value = getenv(var.c_str());
      if (value) {
        std::cout << "readonly " << var << "=" << value << std::endl;
      } else {
        std::cout << "readonly " << var << std::endl;
      }
    }
    return 0;
  }

  bool print_mode = false;
  bool function_mode = false;
  size_t start_index = 1;

  // Parse options
  for (size_t i = 1; i < args.size(); ++i) {
    if (args[i] == "-p") {
      print_mode = true;
      start_index = i + 1;
    } else if (args[i] == "-f") {
      function_mode = true;
      start_index = i + 1;
    } else if (args[i].substr(0, 1) == "-") {
      std::cerr << "readonly: " << args[i] << ": invalid option" << std::endl;
      return 2;
    } else {
      break;
    }
  }

  if (print_mode) {
    // Print in a format suitable for input
    auto readonly_vars = readonly_manager.get_readonly_variables();

    for (const std::string& var : readonly_vars) {
      const char* value = getenv(var.c_str());
      if (value) {
        std::cout << "readonly " << var << "='" << value << "'" << std::endl;
      } else {
        std::cout << "readonly " << var << std::endl;
      }
    }
    return 0;
  }

  if (function_mode) {
    // Function readonly is not implemented yet
    std::cerr << "readonly: -f option not implemented" << std::endl;
    return 1;
  }

  // Process variable assignments
  for (size_t i = start_index; i < args.size(); ++i) {
    std::string arg = args[i];

    size_t eq_pos = arg.find('=');
    if (eq_pos != std::string::npos) {
      // Variable assignment
      std::string name = arg.substr(0, eq_pos);
      std::string value = arg.substr(eq_pos + 1);

      // Check if already readonly
      if (readonly_manager.is_readonly(name)) {
        std::cerr << "readonly: " << name << ": readonly variable" << std::endl;
        return 1;
      }

      // Set the variable value
      if (setenv(name.c_str(), value.c_str(), 1) != 0) {
        perror("readonly: setenv");
        return 1;
      }

      // Mark as readonly
      readonly_manager.set_readonly(name);
    } else {
      // Variable name only - mark existing variable as readonly
      const char* value = getenv(arg.c_str());
      if (value == nullptr) {
        // Variable doesn't exist, create it with empty value
        if (setenv(arg.c_str(), "", 1) != 0) {
          perror("readonly: setenv");
          return 1;
        }
      }

      readonly_manager.set_readonly(arg);
    }
  }

  return 0;
}

// Helper function to check if a variable assignment should be blocked
bool check_readonly_assignment(const std::string& name) {
  return ReadonlyManager::instance().is_readonly(name);
}

#include "set_command.h"

#include <iostream>
#include <string>
#include <vector>

#include "cjsh.h"

int set_command(const std::vector<std::string>& args, Shell* shell) {
  if (!shell) {
    std::cerr << "set: shell not available" << std::endl;
    return 1;
  }

  // Handle set with no arguments - display all variables
  if (args.size() == 1) {
    // For now, just display environment variables
    extern char** environ;
    for (char** env = environ; *env; ++env) {
      std::cout << *env << std::endl;
    }
    return 0;
  }

  // Handle set -- args (set positional parameters)
  if (args.size() >= 2 && args[1] == "--") {
    std::vector<std::string> positional_params;
    for (size_t i = 2; i < args.size(); ++i) {
      positional_params.push_back(args[i]);
    }

    shell->set_positional_parameters(positional_params);

    if (g_debug_mode) {
      std::cerr << "DEBUG: Set " << positional_params.size()
                << " positional parameters" << std::endl;
      for (size_t i = 0; i < positional_params.size(); ++i) {
        std::cerr << "DEBUG: $" << (i + 1) << "=" << positional_params[i]
                  << std::endl;
      }
    }

    return 0;
  }

  // Handle set options (future implementation)
  // For now, just ignore options and continue
  std::cerr << "set: options not yet implemented" << std::endl;
  return 1;
}

int shift_command(const std::vector<std::string>& args, Shell* shell) {
  if (!shell) {
    std::cerr << "shift: shell not available" << std::endl;
    return 1;
  }

  int shift_count = 1;

  // Parse shift count if provided
  if (args.size() > 1) {
    try {
      shift_count = std::stoi(args[1]);
      if (shift_count < 0) {
        std::cerr << "shift: negative shift count" << std::endl;
        return 1;
      }
    } catch (const std::exception&) {
      std::cerr << "shift: invalid shift count: " << args[1] << std::endl;
      return 1;
    }
  }

  return shell->shift_positional_parameters(shift_count);
}

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

  if (args.size() == 1) {
    extern char** environ;
    for (char** env = environ; *env; ++env) {
      std::cout << *env << std::endl;
    }
    return 0;
  }

  for (size_t i = 1; i < args.size(); ++i) {
    const std::string& arg = args[i];

    if (arg == "-e" ||
        (arg == "-o" && i + 1 < args.size() && args[i + 1] == "errexit")) {
      shell->set_shell_option("errexit", true);
      if (arg == "-o") {
        ++i;
      }
      if (g_debug_mode) {
        std::cerr << "DEBUG: Enabled errexit option" << std::endl;
      }
    } else if (arg == "+e" || (arg == "+o" && i + 1 < args.size() &&
                               args[i + 1] == "errexit")) {
      shell->set_shell_option("errexit", false);
      if (arg == "+o") {
        ++i;
      }
      if (g_debug_mode) {
        std::cerr << "DEBUG: Disabled errexit option" << std::endl;
      }
    } else if (arg.substr(0, 2) == "--") {
      std::vector<std::string> positional_params;
      for (size_t j = i + 1; j < args.size(); ++j) {
        positional_params.push_back(args[j]);
      }

      shell->set_positional_parameters(positional_params);

      if (g_debug_mode) {
        std::cerr << "DEBUG: Set " << positional_params.size()
                  << " positional parameters" << std::endl;
        for (size_t k = 0; k < positional_params.size(); ++k) {
          std::cerr << "DEBUG: $" << (k + 1) << "=" << positional_params[k]
                    << std::endl;
        }
      }

      return 0;
    } else {
      std::cerr << "set: option '" << arg << "' not supported yet" << std::endl;
      return 1;
    }
  }

  return 0;
}

int shift_command(const std::vector<std::string>& args, Shell* shell) {
  if (!shell) {
    std::cerr << "shift: shell not available" << std::endl;
    return 1;
  }

  int shift_count = 1;

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

#include "getopts_command.h"
#include <cstdlib>
#include <iostream>
#include "cjsh.h"
#include "shell.h"

int getopts_command(const std::vector<std::string>& args, Shell* shell) {
  if (!shell) {
    std::cerr << "getopts: shell not available" << std::endl;
    return 1;
  }

  if (args.size() < 2) {
    std::cerr << "getopts: usage: getopts optstring name [args...]"
              << std::endl;
    return 1;
  }

  std::string optstring = args[1];
  std::string name = args[2];

  // Get positional parameters or use provided args
  std::vector<std::string> argv_list;
  if (args.size() > 3) {
    // Use provided arguments
    for (size_t i = 3; i < args.size(); ++i) {
      argv_list.push_back(args[i]);
    }
  } else {
    // Use shell's positional parameters
    argv_list = shell->get_positional_parameters();
  }

  // Get current option index from OPTIND environment variable
  int optind = 1;
  const char* optind_env = getenv("OPTIND");
  if (optind_env) {
    try {
      optind = std::stoi(optind_env);
    } catch (...) {
      optind = 1;
    }
  }

  // Check if we've processed all arguments
  if (optind > static_cast<int>(argv_list.size())) {
    // Reset OPTIND and return 1 (false) to indicate end of options
    setenv("OPTIND", "1", 1);
    return 1;
  }

  // Get current argument to process
  if (optind <= 0 || optind > static_cast<int>(argv_list.size())) {
    setenv("OPTIND", "1", 1);
    return 1;
  }

  std::string current_arg = argv_list[optind - 1];

  // Check if argument starts with '-' and has more characters
  if (current_arg.size() < 2 || current_arg[0] != '-') {
    // Not an option, reset and return false
    setenv("OPTIND", "1", 1);
    return 1;
  }

  // Handle "--" (end of options)
  if (current_arg == "--") {
    setenv("OPTIND", std::to_string(optind + 1).c_str(), 1);
    return 1;
  }

  // Get current character position within the argument
  static int char_index = 1;
  const char* optarg_env = getenv("GETOPTS_POS");
  if (optarg_env) {
    try {
      char_index = std::stoi(optarg_env);
    } catch (...) {
      char_index = 1;
    }
  }

  // Check if we're at the start of a new argument
  if (char_index == 1) {
    // Skip the '-' character
    char_index = 1;
  }

  // Get the current option character
  if (char_index >= static_cast<int>(current_arg.size())) {
    // Move to next argument
    optind++;
    char_index = 1;
    setenv("OPTIND", std::to_string(optind).c_str(), 1);
    setenv("GETOPTS_POS", "1", 1);

    // Recursively call to process next argument
    return getopts_command(args, shell);
  }

  char opt = current_arg[char_index];
  char_index++;

  // Look for the option in optstring
  size_t opt_pos = optstring.find(opt);
  if (opt_pos == std::string::npos) {
    // Invalid option
    setenv(name.c_str(), "?", 1);

    // Set OPTARG to the invalid option character
    std::string optarg_val(1, opt);
    setenv("OPTARG", optarg_val.c_str(), 1);

    // Update position
    if (char_index >= static_cast<int>(current_arg.size())) {
      optind++;
      char_index = 1;
    }
    setenv("OPTIND", std::to_string(optind).c_str(), 1);
    setenv("GETOPTS_POS", std::to_string(char_index).c_str(), 1);

    // Check if optstring starts with ':' (silent error reporting)
    if (!optstring.empty() && optstring[0] == ':') {
      setenv(name.c_str(), "?", 1);
      return 0;  // Return success for silent mode
    }

    std::cerr << "getopts: illegal option -- " << opt << std::endl;
    return 0;  // getopts returns 0 when an option is found (even invalid)
  }

  // Valid option found
  std::string opt_val(1, opt);
  setenv(name.c_str(), opt_val.c_str(), 1);

  // Check if option requires an argument
  if (opt_pos + 1 < optstring.size() && optstring[opt_pos + 1] == ':') {
    // Option requires an argument
    std::string optarg;

    if (char_index < static_cast<int>(current_arg.size())) {
      // Argument is remainder of current argument
      optarg = current_arg.substr(char_index);
      optind++;
      char_index = 1;
    } else {
      // Argument should be next argument
      optind++;
      if (optind <= static_cast<int>(argv_list.size())) {
        optarg = argv_list[optind - 1];
        optind++;
      } else {
        // Missing argument
        if (!optstring.empty() && optstring[0] == ':') {
          // Silent error reporting
          setenv(name.c_str(), ":", 1);
          setenv("OPTARG", opt_val.c_str(), 1);
        } else {
          std::cerr << "getopts: option requires an argument -- " << opt
                    << std::endl;
          setenv(name.c_str(), "?", 1);
          setenv("OPTARG", opt_val.c_str(), 1);
        }
        setenv("OPTIND", std::to_string(optind).c_str(), 1);
        setenv("GETOPTS_POS", "1", 1);
        return 0;
      }
      char_index = 1;
    }

    setenv("OPTARG", optarg.c_str(), 1);
  } else {
    // Option doesn't require an argument
    unsetenv("OPTARG");

    // Update position within current argument
    if (char_index >= static_cast<int>(current_arg.size())) {
      optind++;
      char_index = 1;
    }
  }

  setenv("OPTIND", std::to_string(optind).c_str(), 1);
  setenv("GETOPTS_POS", std::to_string(char_index).c_str(), 1);

  return 0;  // Success - option found
}

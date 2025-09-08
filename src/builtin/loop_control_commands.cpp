#include "loop_control_commands.h"

#include <iostream>
#include <string>
#include <vector>

// These commands need special handling by the script interpreter
// For now, we'll implement basic versions that set environment variables
// which the script interpreter can check

int break_command(const std::vector<std::string>& args) {
  // Parse optional level argument
  int level = 1;
  if (args.size() > 1) {
    try {
      level = std::stoi(args[1]);
      if (level < 1) {
        std::cerr << "break: invalid level: " << args[1] << std::endl;
        return 1;
      }
    } catch (const std::exception&) {
      std::cerr << "break: invalid level: " << args[1] << std::endl;
      return 1;
    }
  }

  // Set environment variable to signal break to script interpreter
  setenv("CJSH_BREAK_LEVEL", std::to_string(level).c_str(), 1);

  // Return special exit code to indicate break
  return 255;  // Special exit code for break
}

int continue_command(const std::vector<std::string>& args) {
  // Parse optional level argument
  int level = 1;
  if (args.size() > 1) {
    try {
      level = std::stoi(args[1]);
      if (level < 1) {
        std::cerr << "continue: invalid level: " << args[1] << std::endl;
        return 1;
      }
    } catch (const std::exception&) {
      std::cerr << "continue: invalid level: " << args[1] << std::endl;
      return 1;
    }
  }

  // Set environment variable to signal continue to script interpreter
  setenv("CJSH_CONTINUE_LEVEL", std::to_string(level).c_str(), 1);

  // Return special exit code to indicate continue
  return 254;  // Special exit code for continue
}

int return_command(const std::vector<std::string>& args) {
  // Parse optional exit code argument
  int exit_code = 0;
  if (args.size() > 1) {
    try {
      exit_code = std::stoi(args[1]);
      // Ensure exit code is in valid range (0-255)
      if (exit_code < 0 || exit_code > 255) {
        std::cerr << "return: invalid exit code: " << args[1] << std::endl;
        return 1;
      }
    } catch (const std::exception&) {
      std::cerr << "return: invalid exit code: " << args[1] << std::endl;
      return 1;
    }
  }

  // Set environment variable to signal return to script interpreter
  setenv("CJSH_RETURN_CODE", std::to_string(exit_code).c_str(), 1);

  // Return special exit code to indicate return
  return 253;  // Special exit code for return
}

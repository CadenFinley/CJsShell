#include "loop_control_commands.h"

#include <iostream>
#include <string>
#include <vector>

int break_command(const std::vector<std::string>& args) {
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

  setenv("CJSH_BREAK_LEVEL", std::to_string(level).c_str(), 1);

  return 255;
}

int continue_command(const std::vector<std::string>& args) {
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

  setenv("CJSH_CONTINUE_LEVEL", std::to_string(level).c_str(), 1);

  return 254;
}

int return_command(const std::vector<std::string>& args) {
  int exit_code = 0;
  if (args.size() > 1) {
    try {
      exit_code = std::stoi(args[1]);

      if (exit_code < 0 || exit_code > 255) {
        std::cerr << "return: invalid exit code: " << args[1] << std::endl;
        return 1;
      }
    } catch (const std::exception&) {
      std::cerr << "return: invalid exit code: " << args[1] << std::endl;
      return 1;
    }
  }

  setenv("CJSH_RETURN_CODE", std::to_string(exit_code).c_str(), 1);

  return 253;
}

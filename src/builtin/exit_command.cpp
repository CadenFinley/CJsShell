#include "exit_command.h"

#include "cjsh.h"
#include "trap_command.h"

int exit_command(const std::vector<std::string>& args) {
  int exit_code = 0;
  bool force_exit = false;
  
  // Check for force flags
  force_exit = std::find(args.begin(), args.end(), "-f") != args.end() ||
               std::find(args.begin(), args.end(), "--force") != args.end();
  
  // Parse numeric exit code from any argument position
  for (size_t i = 1; i < args.size(); i++) {
    const std::string& val = args[i];
    if (val != "-f" && val != "--force") {
      char* endptr = nullptr;
      long code = std::strtol(val.c_str(), &endptr, 10);
      if (endptr && *endptr == '\0') {
        // Normalize exit code to 0-255
        exit_code = static_cast<int>(code) & 0xFF;
        break;
      }
    }
  }
  
  // Forced exit - clean up and exit immediately
  if (force_exit) {
    cleanup_resources();
    std::exit(exit_code);
  }

  // For normal exit, set the flag and store the exit code
  // EXIT trap will be executed in cleanup_resources()
  // The exit code will be used when the main loop exits
  g_exit_flag = true;
  setenv("EXIT_CODE", std::to_string(exit_code).c_str(), 1);
  return 0;
}
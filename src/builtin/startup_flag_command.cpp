#include "startup_flag_command.h"

#include <iostream>

#include "cjsh.h"

// External declarations from cjsh.cpp
extern bool g_debug_mode;
extern std::vector<std::string> g_startup_args;

int startup_flag_command(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    std::cerr << "startup-flag: missing flag argument" << std::endl;
    std::cerr << "Usage: startup-flag [--flag-name]" << std::endl;
    std::cerr << "Available flags:" << std::endl;
    std::cerr << "  --login          Set login mode" << std::endl;
    std::cerr << "  --interactive    Force interactive mode" << std::endl;
    std::cerr << "  --debug          Enable debug mode" << std::endl;
    std::cerr << "  --no-plugins     Disable plugins" << std::endl;
    std::cerr << "  --no-themes      Disable themes" << std::endl;
    std::cerr << "  --no-ai          Disable AI features" << std::endl;
    std::cerr << "  --no-colors      Disable colors" << std::endl;
    std::cerr << "  --no-titleline   Disable title line" << std::endl;
    std::cerr << "  --no-source      Disable source file processing" << std::endl;
    std::cerr << "  --startup-test   Enable startup test mode" << std::endl;
    return 1;
  }

  const std::string& flag = args[1];

  if (g_debug_mode) {
    std::cerr << "DEBUG: Processing startup flag: " << flag << std::endl;
  }

  // Validate the flag
  if (flag == "--login" || flag == "--interactive" || flag == "--debug" ||
      flag == "--no-plugins" || flag == "--no-themes" || flag == "--no-ai" ||
      flag == "--no-colors" || flag == "--no-titleline" || flag == "--no-source" ||
      flag == "--startup-test") {
    
    // Check if the flag is already in g_startup_args
    bool flag_exists = false;
    for (const auto& existing_flag : g_startup_args) {
      if (existing_flag == flag) {
        flag_exists = true;
        break;
      }
    }
    
    // Only add the flag if it's not already present
    if (!flag_exists) {
      g_startup_args.push_back(flag);
      if (g_debug_mode) {
        std::cerr << "DEBUG: Added '" << flag << "' to startup args" << std::endl;
      }
    } else if (g_debug_mode) {
      std::cerr << "DEBUG: Flag '" << flag << "' already exists in startup args" << std::endl;
    }
  } else {
    std::cerr << "startup-flag: unknown flag '" << flag << "'" << std::endl;
    return 1;
  }

  return 0;
}

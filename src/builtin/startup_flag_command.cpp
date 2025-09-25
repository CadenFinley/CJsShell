#include "startup_flag_command.h"

#include <iostream>

#include "cjsh.h"
#include "error_out.h"

extern bool g_debug_mode;
extern std::vector<std::string> g_profile_startup_args;

int startup_flag_command(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    std::cerr << "cjsh: startup-flag: missing flag argument" << std::endl;
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
    std::cerr << "  --show-startup-time Display shell startup time"
              << std::endl;
    std::cerr << "  --no-source      Don't source the .cjshrc file"
              << std::endl;
    std::cerr << "  --no-completions Disable tab completions" << std::endl;
    std::cerr << "  --no-syntax-highlighting Disable syntax highlighting"
              << std::endl;
    std::cerr << "  --no-smart-cd    Disable smart cd functionality"
              << std::endl;
    std::cerr
        << "  --minimal        Disable all unique cjsh features (plugins, themes, AI, colors, completions, syntax highlighting, smart cd, sourcing, custom ls colors, startup time display)"
        << std::endl;
    std::cerr << "  --disable-ls-colors Disable custom ls output colors"
              << std::endl;
    std::cerr << "  --startup-test   Enable startup test mode" << std::endl;
    return 1;
  }

  const std::string& flag = args[1];

  if (g_debug_mode) {
    std::cerr << "DEBUG: Processing startup flag: " << flag << std::endl;
  }

  if (flag == "--login" || flag == "--interactive" || flag == "--debug" ||
      flag == "--no-plugins" || flag == "--no-themes" || flag == "--no-ai" ||
      flag == "--no-colors" || flag == "--no-titleline" ||
      flag == "--show-startup-time" || flag == "--no-source" ||
      flag == "--no-completions" || flag == "--no-syntax-highlighting" ||
      flag == "--no-smart-cd" || flag == "--minimal" ||
      flag == "--startup-test" || flag == "--disable-ls-colors") {
    bool flag_exists = false;
    for (const auto& existing_flag : g_profile_startup_args) {
      if (existing_flag == flag) {
        flag_exists = true;
        break;
      }
    }

    if (!flag_exists) {
      g_profile_startup_args.push_back(flag);
      if (g_debug_mode) {
        std::cerr << "DEBUG: Added '" << flag << "' to profile startup args"
                  << std::endl;
      }
    } else if (g_debug_mode) {
      std::cerr << "DEBUG: Flag '" << flag
                << "' already exists in profile startup args" << std::endl;
    }
  } else {
    print_error({ErrorType::INVALID_ARGUMENT,
                 "startup-flag",
                 "unknown flag '" + flag + "'",
                 {}});
    return 1;
  }

  return 0;
}

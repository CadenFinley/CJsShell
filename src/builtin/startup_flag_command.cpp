#include "startup_flag_command.h"

#include <iostream>

#include "cjsh.h"
#include "error_out.h"

extern bool g_debug_mode;
extern std::vector<std::string> g_profile_startup_args;

int startup_flag_command(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    print_error(
        {ErrorType::INVALID_ARGUMENT,
         "login-startup-arg",
         "Missing flag argument",
         {"Usage: login-startup-arg [--flag-name]",
          "Available flags:", "  --login              Set login mode",
          "  --interactive        Force interactive mode",
          "  --debug              Enable debug mode",
          "  --no-plugins         Disable plugins",
          "  --no-themes          Disable themes",
          "  --no-ai              Disable AI features",
          "  --no-colors          Disable colors",
          "  --no-titleline       Disable title line",
          "  --show-startup-time  Display shell startup time",
          "  --no-source          Don't source the .cjshrc file",
          "  --no-completions     Disable tab completions",
          "  --no-syntax-highlighting Disable syntax highlighting",
          "  --no-smart-cd        Disable smart cd functionality",
          "  --minimal            Disable all unique cjsh features (plugins, "
          "themes, AI, colors, completions, syntax highlighting, smart cd, "
          "sourcing, custom ls, startup time display)",
          "  --disable-custom-ls  Use system ls command instead of builtin ls",
          "  --startup-test       Enable startup test mode"}});
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
      flag == "--startup-test" || flag == "--disable-custom-ls") {
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
                 "login-startup-arg",
                 "unknown flag '" + flag + "'",
                 {}});
    return 1;
  }

  return 0;
}

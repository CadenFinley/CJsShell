#include "user_command.h"

#include <ctime>
#include <iostream>
#include <string>
#include <vector>

#include "main.h"

#define PRINT_ERROR(MSG)                             \
  do {                                               \
    std::cerr << (MSG) << '\n';                      \
  } while (0)

int user_command(const std::vector<std::string>& args) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: user_commands called with " << args.size()
              << " arguments" << std::endl;
    if (args.size() > 1)
      std::cerr << "DEBUG: user subcommand: " << args[1] << std::endl;
  }

  if (args.size() < 2) {
    PRINT_ERROR("Unknown command. No given ARGS. Try 'help'");
    return 1;
  }

  const std::string& cmd = args[1];

  if (cmd == "testing") {
    if (args.size() < 3) {
      std::cout << "Debug mode is currently "
                << (g_debug_mode ? "enabled." : "disabled.") << std::endl;
      return 0;
    }

    if (args[2] == "enable") {
      g_debug_mode = true;
      std::cout << "Debug mode enabled." << std::endl;
      return 0;
    }

    if (args[2] == "disable") {
      g_debug_mode = false;
      std::cout << "Debug mode disabled." << std::endl;
      return 0;
    }

    PRINT_ERROR("Unknown testing command. Use 'enable' or 'disable'.");
    return 1;
  }

  if (cmd == "checkforupdates") {
    if (g_debug_mode)
      std::cerr << "DEBUG: Processing checkforupdates command" << std::endl;

    if (args.size() < 3) {
      std::cout << "Check for updates is currently "
                << (g_check_updates ? "enabled." : "disabled.") << std::endl;
      return 0;
    }

    if (args[2] == "enable") {
      if (g_debug_mode)
        std::cerr << "DEBUG: Enabling check for updates" << std::endl;
      g_check_updates = true;
      std::cout << "Check for updates enabled." << std::endl;
      return 0;
    }

    if (args[2] == "disable") {
      if (g_debug_mode)
        std::cerr << "DEBUG: Disabling check for updates" << std::endl;
      g_check_updates = false;
      std::cout << "Check for updates disabled." << std::endl;
      return 0;
    }

    std::cerr << "Unknown command. Use 'enable' or 'disable'." << std::endl;
    return 1;
  }

  if (cmd == "silentupdatecheck") {
    if (args.size() < 3) {
      std::cout << "Silent update check is currently "
                << (g_silent_update_check ? "enabled." : "disabled.")
                << std::endl;
      return 0;
    }

    if (args[2] == "enable") {
      g_silent_update_check = true;
      std::cout << "Silent update check enabled." << std::endl;
      return 0;
    }

    if (args[2] == "disable") {
      g_silent_update_check = false;
      std::cout << "Silent update check disabled." << std::endl;
      return 0;
    }

    std::cerr << "Unknown command. Use 'enable' or 'disable'." << std::endl;
    return 1;
  }

  if (cmd == "titleline") {
    if (args.size() < 3) {
      std::cout << "Title line is currently "
                << (g_title_line ? "enabled." : "disabled.") << std::endl;
      return 0;
    }

    if (args[2] == "enable") {
      g_title_line = true;
      std::cout << "Title line enabled." << std::endl;
      return 0;
    }

    if (args[2] == "disable") {
      g_title_line = false;
      std::cout << "Title line disabled." << std::endl;
      return 0;
    }

    std::cerr << "Unknown command. Use 'enable' or 'disable'." << std::endl;
    return 1;
  }

  if (cmd == "update") {
    if (args.size() < 3) {
      std::cout << "Update settings:" << std::endl;
      std::cout << " Auto-check for updates: "
                << (g_check_updates ? "Enabled" : "Disabled") << std::endl;
      std::cout << " Silent update check: "
                << (g_silent_update_check ? "Enabled" : "Disabled")
                << std::endl;
      std::cout << " Update check interval: "
                << (g_update_check_interval / 3600) << " hours" << std::endl;
      std::cout << " Last update check: "
                << (g_last_update_check > 0
                        ? std::string(ctime(&g_last_update_check))
                        : "Never")
                << std::endl;
      if (g_cached_update) {
        std::cout << " Update available: " << g_cached_version << std::endl;
      }
      return 0;
    }

    if (args[2] == "check") {
      std::cout << "Checking for updates from GitHub..." << std::endl;
      bool updateAvailable = check_for_update();
      if (updateAvailable) {
        std::cout << "An update is available!" << std::endl;
        execute_update_if_available(updateAvailable);
      } else {
        std::cout << "You are up to date." << std::endl;
      }
      return 0;
    }

    if (args[2] == "interval" && args.size() > 3) {
      try {
        int hours = std::stoi(args[3]);
        if (hours < 1) {
          std::cerr << "Interval must be at least 1 hour" << std::endl;
          return 1;
        }
        g_update_check_interval = hours * 3600;
        std::cout << "Update check interval set to " << hours << " hours"
                  << std::endl;
        return 0;
      } catch (const std::exception& e) {
        std::cerr << "Invalid interval value. Please specify hours as a number"
                  << std::endl;
        return 1;
      }
    }

    if (args[2] == "help") {
      std::cout << "Update commands:" << std::endl;
      std::cout << " check: Manually check for updates now" << std::endl;
      std::cout << " interval [HOURS]: Set update check interval in hours"
                << std::endl;
      std::cout << " help: Show this help message" << std::endl;
      return 0;
    }

    std::cerr << "Unknown update command. Try 'help' for available commands."
              << std::endl;
    return 1;
  }

  if (cmd == "help") {
    std::cout << "User settings commands:" << std::endl;
    std::cout << " testing: Toggle debug mode (enable/disable)" << std::endl;
    std::cout << " checkforupdates: Control whether updates are checked"
              << std::endl;
    std::cout
        << " silentupdatecheck: Toggle silent update checking (enable/disable)"
        << std::endl;
    std::cout << " titleline: Toggle title line display (enable/disable)"
              << std::endl;
    std::cout
        << " update: Manage update settings and perform manual update checks"
        << std::endl;
    return 0;
  }

  std::cerr << "Unknown command. Try 'user help' for available commands."
            << std::endl;
  return 1;
}

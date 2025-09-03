#include "plugin_command.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "cjsh.h"

#define PRINT_ERROR(MSG)        \
  do {                          \
    std::cerr << (MSG) << '\n'; \
  } while (0)

int plugin_command(const std::vector<std::string>& args) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: plugin_commands called with " << args.size()
              << " arguments" << std::endl;
    if (args.size() > 1)
      std::cerr << "DEBUG: plugin command: " << args[1] << std::endl;
  }

  if (g_plugin == nullptr) {
    PRINT_ERROR("Plugin manager not initialized");
    return 1;
  }

  if (!g_plugin->get_enabled()) {
    PRINT_ERROR("Plugins are disabled");
    return 1;
  }

  if (args.size() < 2) {
    PRINT_ERROR("Unknown command. No given ARGS. Try 'help'");
    return 1;
  }

  const std::string& cmd = args[1];

  if (cmd == "help") {
    std::cout << "Plugin commands:" << std::endl;
    std::cout << " available: List available plugins" << std::endl;
    std::cout << " enabled: List enabled plugins" << std::endl;
    std::cout << " enable [NAME]: Enable a plugin" << std::endl;
    std::cout << " disable [NAME]: Disable a plugin" << std::endl;
    std::cout << " info [NAME]: Show plugin information" << std::endl;
    std::cout << " commands [NAME]: List commands for a plugin" << std::endl;
    std::cout
        << " settings [NAME] set [SETTING] [VALUE]: Modify a plugin setting"
        << std::endl;
    std::cout << " help: Show this help message" << std::endl;
    std::cout << " install [PATH]: Install a new plugin" << std::endl;
    std::cout << " uninstall [NAME]: Remove an installed plugin" << std::endl;
    return 0;
  }

  if (cmd == "available") {
    if (g_plugin) {
      auto plugins = g_plugin->get_available_plugins();
      std::cout << "Available plugins:" << std::endl;
      for (const auto& name : plugins) {
        std::cout << name << std::endl;
      }
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return 0;
  }

  if (cmd == "enabled") {
    if (g_plugin) {
      auto plugins = g_plugin->get_enabled_plugins();
      std::cout << "Enabled plugins:" << std::endl;
      for (const auto& name : plugins) {
        std::cout << name << std::endl;
      }
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return 0;
  }

  if (cmd == "enableall") {
    if (g_plugin) {
      for (const auto& plugin : g_plugin->get_available_plugins()) {
        g_plugin->enable_plugin(plugin);
      }
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return 0;
  }

  if (cmd == "disableall") {
    if (g_plugin) {
      for (const auto& plugin : g_plugin->get_enabled_plugins()) {
        g_plugin->disable_plugin(plugin);
      }
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return 0;
  }

  if (cmd == "install" && args.size() > 2) {
    if (g_plugin) {
      std::string pluginPath = args[2];
      g_plugin->install_plugin(pluginPath);
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return 0;
  }

  if (cmd == "uninstall" && args.size() > 2) {
    if (g_plugin) {
      std::string pluginName = args[2];
      g_plugin->uninstall_plugin(pluginName);
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return 0;
  }

  if (cmd == "info" && args.size() > 2) {
    if (g_plugin) {
      std::string pluginName = args[2];
      std::cout << g_plugin->get_plugin_info(pluginName) << std::endl;
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return 0;
  }

  if (cmd == "enable" && args.size() > 2) {
    if (g_plugin) {
      std::string pluginName = args[2];
      g_plugin->enable_plugin(pluginName);
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return 0;
  }

  if (cmd == "disable" && args.size() > 2) {
    if (g_plugin) {
      std::string pluginName = args[2];
      g_plugin->disable_plugin(pluginName);
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return 0;
  }

  if (cmd == "commands" && args.size() > 2) {
    if (g_plugin) {
      std::string pluginName = args[2];
      std::cout << "Commands for " << pluginName << ":" << std::endl;
      std::vector<std::string> listOfPluginCommands =
          g_plugin->get_plugin_commands(pluginName);
      for (const auto& cmd : listOfPluginCommands) {
        std::cout << "  " << cmd << std::endl;
      }
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return 0;
  }

  if (cmd == "settings") {
    if (g_plugin) {
      if (args.size() < 3) {
        std::cout << "Settings for plugins:" << std::endl;
        auto settings = g_plugin->get_all_plugin_settings();
        for (const auto& [plugin, setting_map] : settings) {
          std::cout << plugin << ":" << std::endl;
          for (const auto& [key, value] : setting_map) {
            std::cout << "  " << key << " = " << value << std::endl;
          }
        }
        return 0;
      }

      if (args.size() > 4 && args[3] == "set") {
        std::string pluginName = args[2];
        std::string settingName = args[4];
        std::string settingValue = args.size() > 5 ? args[5] : "";

        if (g_plugin->update_plugin_setting(pluginName, settingName,
                                            settingValue)) {
          std::cout << "Setting " << settingName << " set to " << settingValue
                    << " for plugin " << pluginName << std::endl;
        } else {
          std::cout << "Setting " << settingName << " not found for plugin "
                    << pluginName << std::endl;
        }
        return 0;
      }
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return 0;
  }

  if (g_plugin) {
    std::string pluginName = cmd;
    std::vector<std::string> enabledPlugins = g_plugin->get_enabled_plugins();
    if (std::find(enabledPlugins.begin(), enabledPlugins.end(), pluginName) !=
        enabledPlugins.end()) {
      if (args.size() > 2) {
        if (args[2] == "enable") {
          g_plugin->enable_plugin(pluginName);
          return 0;
        }
        if (args[2] == "disable") {
          g_plugin->disable_plugin(pluginName);
          return 0;
        }
        if (args[2] == "info") {
          std::cout << g_plugin->get_plugin_info(pluginName) << std::endl;
          return 0;
        }
        if (args[2] == "commands" || args[2] == "cmds" || args[2] == "help") {
          std::cout << "Commands for " << pluginName << ":" << std::endl;
          std::vector<std::string> listOfPluginCommands =
              g_plugin->get_plugin_commands(pluginName);
          for (const auto& cmd : listOfPluginCommands) {
            std::cout << "  " << cmd << std::endl;
          }
          return 0;
        }
      }
    } else {
      std::vector<std::string> availablePlugins =
          g_plugin->get_available_plugins();
      if (std::find(availablePlugins.begin(), availablePlugins.end(),
                    pluginName) != availablePlugins.end()) {
        if (args.size() > 2 && args[2] == "enable") {
          g_plugin->enable_plugin(pluginName);
          return 0;
        }
        std::cerr << "Plugin: " << pluginName << " is disabled." << std::endl;
        return 0;
      } else {
        std::cerr << "Plugin " << pluginName << " does not exist." << std::endl;
        return 1;
      }
    }
  }

  std::cerr << "Unknown command. Try 'help' for available commands."
            << std::endl;
  return 1;
}

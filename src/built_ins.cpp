#include "built_ins.h"

#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <iomanip>


#include "cjsh_filesystem.h"
#include "main.h"

#include "cd_command.h"
#include "approot_command.h"
#include "exit_command.h"
#include "version_command.h"
#include "restart_command.h"
#include "help_command.h"
#include "history_command.h"
#include "uninstall_command.h"
#include "eval_command.h"
#include "aihelp_command.h"
#include "alias_command.h"
#include "export_command.h"

#define PRINT_ERROR(MSG)                             \
  do {                                               \
    last_terminal_output_error = (MSG);              \
    std::cerr << last_terminal_output_error << '\n'; \
  } while (0)


Built_ins::Built_ins()
      : builtins{
            {"cd",
             [this](const std::vector<std::string>& args) {
              return ::change_directory(args.size() > 1 ? args[1] : current_directory, current_directory, previous_directory, last_terminal_output_error);
             }},
            {"alias",
             [this](const std::vector<std::string>& args) {
               return ::alias_command(args, shell);
             }},
            {"export",
             [this](const std::vector<std::string>& args) {
               return ::export_command(args, shell);
             }},
            {"unalias",
             [this](const std::vector<std::string>& args) {
               return ::unalias_command(args, shell);
             }},
            {"unset",
             [this](const std::vector<std::string>& args) {
               return ::unset_command(args, shell);
             }},
            {"ai",
             [this](const std::vector<std::string>& args) {
               return ai_commands(args);
             }},
            {"user",
             [this](const std::vector<std::string>& args) {
               return user_commands(args);
             }},
            {"theme",
             [this](const std::vector<std::string>& args) {
               return theme_commands(args);
             }},
            {"plugin",
             [this](const std::vector<std::string>& args) {
               return plugin_commands(args);
             }},
            {"help",
             [](const std::vector<std::string>&) {
               return ::help_command();
             }},
            {"approot",
             [this](const std::vector<std::string>&) {
               return ::change_to_approot(current_directory, previous_directory, last_terminal_output_error);
             }},
            {"aihelp",
             [](const std::vector<std::string>& args) {
               return ::aihelp_command(args);
             }},
            {"version",
             [](const std::vector<std::string>& args) {
               return ::version_command(args);
             }},
            {"uninstall",
             [](const std::vector<std::string>&) {
               return ::uninstall_command();
             }},
            {"restart",
             [](const std::vector<std::string>&) {
               return ::restart_command();
             }},
            {"eval",
             [this](const std::vector<std::string>& args) {
               return ::eval_command(args, shell);
             }},
            {"history",
             [](const std::vector<std::string>& args) {
               return ::history_command(args);
             }},
            {"exit",
             [](const std::vector<std::string>& args) {
               return ::exit_command(args);
             }},
            {"quit",
             [](const std::vector<std::string>& args) {
               return ::exit_command(args);
             }},
            {"terminal",
             [this](const std::vector<std::string>& args) {
              (void)args;
               shell->set_menu_active(true);
               return 0;
             }},
        },
        shell(nullptr) {}

int Built_ins::builtin_command(const std::vector<std::string>& args) {
  if (args.empty()) return 1;

  auto it = builtins.find(args[0]);
  if (it != builtins.end()) {
    if (args[0] == "cd" && args.size() == 1) {
      return ::change_directory("", current_directory, previous_directory, last_terminal_output_error);
    }
    int status = it->second(args);
    return status;
  }
  PRINT_ERROR("cjsh: command not found: " + args[0]);
  return 127;
}

int Built_ins::is_builtin_command(const std::string& cmd) const {
  return !cmd.empty() && builtins.find(cmd) != builtins.end();
}

int Built_ins::ai_commands(const std::vector<std::string>& args) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: ai_commands called with " << args.size()
              << " arguments" << std::endl;
    if (args.size() > 1)
      std::cerr << "DEBUG: ai subcommand: " << args[1] << std::endl;
  }

  if (g_ai == nullptr) {
    return 1;
  }
  unsigned int command_index = 1;

  if (args.size() <= command_index) {
    std::cerr << "To invoke regular commands prefix all commands with ':'"
              << std::endl;
    shell->set_menu_active(false);
    if (!g_ai->getChatCache().empty()) {
      std::cout << "Chat history:" << std::endl;
      for (const auto& message : g_ai->getChatCache()) {
        std::cout << message << std::endl;
      }
    }
    return 0;
  }

  const std::string& cmd = args[command_index];

  if (cmd == "log") {
    std::string lastChatSent = g_ai->getLastPromptUsed();
    std::string lastChatReceived = g_ai->getLastResponseReceived();
    std::string fileName =
        (cjsh_filesystem::g_cjsh_data_path /
         ("OpenAPI_Chat_" + std::to_string(time(nullptr)) + ".txt"))
            .string();
    std::ofstream file(fileName);
    if (!file.is_open()) {
      PRINT_ERROR(std::string("Error: Unable to create the chat log file at ") +
                  fileName);
    } else {
      file << "Chat Sent: " << lastChatSent << "\n";
      file << "Chat Received: " << lastChatReceived << "\n";
      file.close();
      std::cout << "Chat log saved to " << fileName << std::endl;
    }
    return 0;
  }

  if (cmd == "apikey") {
    std::cout << g_ai->getAPIKey() << std::endl;
    return 0;
  }

  if (cmd == "chat") {
    ai_chat_commands(args, command_index);
    return 0;
  }

  if (cmd == "get") {
    if (args.size() <= command_index + 1) {
      PRINT_ERROR(
          "Error: No arguments provided. Try 'help' for a list of commands.");
      return 1;
    }
    std::cout << g_ai->getResponseData(args[command_index + 1]) << std::endl;
    return 0;
  }

  if (cmd == "dump") {
    std::cout << g_ai->getResponseData("all") << std::endl;
    std::cout << g_ai->getLastPromptUsed() << std::endl;
    return 0;
  }

  if (cmd == "mode") {
    if (args.size() <= command_index + 1) {
      std::cout << "The current assistant mode is " << g_ai->getAssistantType()
                << std::endl;
      return 0;
    }
    g_ai->setAssistantType(args[command_index + 1]);
    std::cout << "Assistant mode set to " << args[command_index + 1]
              << std::endl;
    return 0;
  }

  if (cmd == "file") {
    handle_ai_file_commands(args, command_index);
    return 0;
  }

  if (cmd == "directory") {
    if (args.size() <= command_index + 1) {
      std::cout << "The current directory is " << g_ai->getSaveDirectory()
                << std::endl;
      return 0;
    }

    if (args[command_index + 1] == "set") {
      g_ai->setSaveDirectory(current_directory);
      std::cout << "Directory set to " << current_directory << std::endl;
      return 0;
    }

    if (args[command_index + 1] == "clear") {
      g_ai->setSaveDirectory(cjsh_filesystem::g_cjsh_data_path);
      std::cout << "Directory set to default." << std::endl;
      return 0;
    }
    return 1;
  }

  if (cmd == "model") {
    if (args.size() <= command_index + 1) {
      std::cout << "The current model is " << g_ai->getModel() << std::endl;
      return 0;
    }
    g_ai->setModel(args[command_index + 1]);
    std::cout << "Model set to " << args[command_index + 1] << std::endl;
    return 0;
  }

  if (cmd == "rejectchanges") {
    g_ai->rejectChanges();
    std::cout << "Changes rejected." << std::endl;
    return 0;
  }

  if (cmd == "timeoutflag") {
    if (args.size() <= command_index + 1) {
      std::cout << "The current timeout flag is "
                << g_ai->getTimeoutFlagSeconds() << std::endl;
      return 0;
    }

    try {
      int timeout = std::stoi(args[command_index + 1]);
      g_ai->setTimeoutFlagSeconds(timeout);
      std::cout << "Timeout flag set to " << timeout << " seconds."
                << std::endl;
    } catch (const std::exception& e) {
      PRINT_ERROR("Error: Invalid timeout value. Please provide a number.");
    }
    return 1;
  }

  if (cmd == "help") {
    std::cout << "AI settings commands:" << std::endl;
    std::cout << " log: Save recent chat exchange to a file" << std::endl;
    std::cout << " apikey" << std::endl;
    std::cout << " chat: Access AI chat commands" << std::endl;
    std::cout << " get [KEY]: Retrieve specific response data" << std::endl;
    std::cout << " dump: Display all response data and last prompt"
              << std::endl;
    std::cout << " mode [TYPE]: Set the assistant mode" << std::endl;
    std::cout << " file: Manage files for context (add, remove, active, "
                 "available, refresh, clear)"
              << std::endl;
    std::cout << " directory: Manage save directory (set, clear)" << std::endl;
    std::cout << " model [MODEL]: Set the AI model" << std::endl;
    std::cout << " rejectchanges: Reject AI suggested changes" << std::endl;
    std::cout << " timeoutflag [SECONDS]: Set the timeout duration"
              << std::endl;
    return 0;
  }

  std::string message = cmd;
  for (unsigned int i = command_index + 1; i < args.size(); i++) {
    message += " " + args[i];
  }
  do_ai_request(message);
  return 0;
}

int Built_ins::ai_chat_commands(const std::vector<std::string>& args,
                                int cmd_index) {
  if (args.size() <= static_cast<unsigned int>(cmd_index) + 1) {
    PRINT_ERROR(
        "Error: No arguments provided. Try 'help' for a list of commands.");
    return 1;
  }

  const std::string& subcmd = args[cmd_index + 1];

  if (subcmd == "history") {
    if (args.size() <= static_cast<unsigned int>(cmd_index) + 2) {
      if (!g_ai->getChatCache().empty()) {
        std::cout << "Chat history:" << std::endl;
        for (const auto& message : g_ai->getChatCache()) {
          std::cout << message << std::endl;
        }
      } else {
        std::cout << "No chat history available." << std::endl;
      }
      return 0;
    }

    if (args[cmd_index + 2] == "clear") {
      g_ai->clearChatCache();
      std::cout << "Chat history cleared." << std::endl;
      return 0;
    }
  }

  if (subcmd == "cache") {
    if (args.size() <= static_cast<unsigned int>(cmd_index) + 2) {
      std::cerr
          << "Error: No arguments provided. Try 'help' for a list of commands."
          << std::endl;
      return 1;
    }

    if (args[cmd_index + 2] == "enable") {
      g_ai->setCacheTokens(true);
      std::cout << "Cache tokens enabled." << std::endl;
      return 0;
    }

    if (args[cmd_index + 2] == "disable") {
      g_ai->setCacheTokens(false);
      std::cout << "Cache tokens disabled." << std::endl;
      return 0;
    }

    if (args[cmd_index + 2] == "clear") {
      g_ai->clearAllCachedTokens();
      std::cout << "Chat history cleared." << std::endl;
      return 0;
    }
  }

  if (subcmd == "help") {
    std::cout << "AI chat commands:" << std::endl;
    std::cout << " history: Show chat history" << std::endl;
    std::cout << " history clear: Clear chat history" << std::endl;
    std::cout << " cache enable: Enable token caching" << std::endl;
    std::cout << " cache disable: Disable token caching" << std::endl;
    std::cout << " cache clear: Clear all cached tokens" << std::endl;
    std::cout << " [MESSAGE]: Send a direct message to AI" << std::endl;
    return 0;
  }

  std::string message = subcmd;
  for (unsigned int i = cmd_index + 2; i < args.size(); i++) {
    message += " " + args[i];
  }

  std::cout << "Sent message to GPT: " << message << std::endl;
  do_ai_request(message);
  return 0;
}

int Built_ins::handle_ai_file_commands(const std::vector<std::string>& args,
                                       int cmd_index) {
  std::vector<std::string> filesAtPath;
  try {
    for (const auto& entry :
         std::filesystem::directory_iterator(current_directory)) {
      if (entry.is_regular_file()) {
        filesAtPath.push_back(entry.path().filename().string());
      }
    }
  } catch (const std::exception& e) {
    PRINT_ERROR(std::string("Error reading directory: ") + e.what());
  }

  if (args.size() <= static_cast<unsigned int>(cmd_index) + 1) {
    std::vector<std::string> activeFiles = g_ai->getFiles();
    std::cout << "Active Files: " << std::endl;
    for (const auto& file : activeFiles) {
      std::cout << file << std::endl;
    }
    std::cout << "Total characters processed: "
              << g_ai->getFileContents().length() << std::endl;
    std::cout << "Files at current path: " << std::endl;
    for (const auto& file : filesAtPath) {
      std::cout << file << std::endl;
    }
    return 0;
  }

  const std::string& subcmd = args[cmd_index + 1];

  if (subcmd == "add") {
    if (args.size() <= static_cast<unsigned int>(cmd_index) + 2) {
      PRINT_ERROR(
          "Error: No file specified. Try 'help' for a list of commands.");
      return 1;
    }

    if (args[cmd_index + 2] == "all") {
      int charsProcessed = g_ai->addFiles(filesAtPath);
      std::cout << "Processed " << charsProcessed << " characters from "
                << filesAtPath.size() << " files." << std::endl;
      return 0;
    }

    std::string filename = args[cmd_index + 2];
    std::string filePath = current_directory + "/" + filename;

    if (!std::filesystem::exists(filePath)) {
      PRINT_ERROR(std::string("Error: File not found: ") + filename);
      return 1;
    }

    int charsProcessed = g_ai->addFile(filePath);
    std::cout << "Processed " << charsProcessed
              << " characters from file: " << filename << std::endl;
    return 0;
  }

  if (subcmd == "remove") {
    if (args.size() <= static_cast<unsigned int>(cmd_index) + 2) {
      PRINT_ERROR(
          "Error: No file specified. Try 'help' for a list of commands.");
      return 1;
    }

    if (args[cmd_index + 2] == "all") {
      int fileCount = g_ai->getFiles().size();
      g_ai->clearFiles();
      std::cout << "Removed all " << fileCount << " files from context."
                << std::endl;
      return 0;
    }

    std::string filename = args[cmd_index + 2];
    std::string filePath = current_directory + "/" + filename;

    if (!std::filesystem::exists(filePath)) {
      PRINT_ERROR(std::string("Error: File not found: ") + filename);
      return 1;
    }

    g_ai->removeFile(filePath);
    std::cout << "Removed file: " << filename << " from context." << std::endl;
    return 0;
  }

  if (subcmd == "active") {
    std::vector<std::string> activeFiles = g_ai->getFiles();
    std::cout << "Active Files: " << std::endl;
    if (activeFiles.empty()) {
      std::cout << "  No active files." << std::endl;
    } else {
      for (const auto& file : activeFiles) {
        std::cout << "  " << file << std::endl;
      }
      std::cout << "Total characters processed: "
                << g_ai->getFileContents().length() << std::endl;
    }
    return 0;
  }

  if (subcmd == "available") {
    std::cout << "Files at current path: " << std::endl;
    for (const auto& file : filesAtPath) {
      std::cout << file << std::endl;
    }
    return 0;
  }

  if (subcmd == "refresh") {
    g_ai->refreshFiles();
    std::cout << "Files refreshed." << std::endl;
    return 0;
  }

  if (subcmd == "clear") {
    g_ai->clearFiles();
    std::cout << "Files cleared." << std::endl;
    return 0;
  }

  PRINT_ERROR("Error: Unknown command. Try 'help' for a list of commands.");
  return 1;
}

int Built_ins::plugin_commands(const std::vector<std::string>& args) {
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

int Built_ins::theme_commands(const std::vector<std::string>& args) {
  if (g_theme == nullptr) {
    PRINT_ERROR("Theme manager not initialized");
    return 1;
  }
  if (args.size() < 2) {
    if (g_theme) {
      std::cout << "Current theme: " << g_current_theme << std::endl;
      std::cout << "Available themes: " << std::endl;
      for (const auto& theme : g_theme->list_themes()) {
        std::cout << "  " << theme << std::endl;
      }
    } else {
      std::cerr << "Theme manager not initialized" << std::endl;
      return 1;
    }
    return 0;
  }

  if (args[1] == "load" && args.size() > 2) {
    if (g_theme) {
      std::string themeName = args[2];
      if (g_theme->load_theme(themeName)) {
        g_current_theme = themeName;
        update_theme_in_rc_file(themeName);
        return 0;
      } else {
        std::cerr << "Error: Theme '" << themeName
                  << "' not found or could not be loaded." << std::endl;
        std::cout << "Staying with current theme: '" << g_current_theme << "'"
                  << std::endl;
        return 0;
      }
    } else {
      std::cerr << "Theme manager not initialized" << std::endl;
      return 1;
    }
  }

  if (g_theme) {
    std::string themeName = args[1];
    if (g_theme->load_theme(themeName)) {
      g_current_theme = themeName;
      update_theme_in_rc_file(themeName);
      return 0;
    } else {
      std::cerr << "Error: Theme '" << themeName
                << "' not found or could not be loaded." << std::endl;
      std::cout << "Staying with current theme: '" << g_current_theme << "'"
                << std::endl;
      return 0;
    }
  } else {
    std::cerr << "Theme manager not initialized" << std::endl;
    return 1;
  }
}

int Built_ins::update_theme_in_rc_file(const std::string& themeName) {
  std::filesystem::path rc_path = cjsh_filesystem::g_cjsh_source_path;

  std::vector<std::string> lines;
  std::string line;
  std::ifstream read_file(rc_path);

  bool theme_line_found = false;
  size_t last_theme_line_idx = 0;

  if (read_file.is_open()) {
    while (std::getline(read_file, line)) {
      lines.push_back(line);
      if (line.find("theme ") == 0) {
        theme_line_found = true;
        last_theme_line_idx = lines.size() - 1;
      }
    }
    read_file.close();
  }

  std::string new_theme_line = "theme load " + themeName;

  if (theme_line_found) {
    lines[last_theme_line_idx] = new_theme_line;
  } else {
    lines.push_back(new_theme_line);
  }

  std::ofstream write_file(rc_path);
  if (write_file.is_open()) {
    for (const auto& l : lines) {
      write_file << l << std::endl;
    }
    write_file.close();

    if (g_debug_mode) {
      std::cout << "Theme setting updated in " << rc_path.string() << std::endl;
    }
  } else {
    PRINT_ERROR("Error: Unable to open .cjshrc file for writing at " +
                rc_path.string());
  }
  return 0;
}

int Built_ins::user_commands(const std::vector<std::string>& args) {
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

int Built_ins::do_ai_request(const std::string& prompt) {
  if (!g_ai) {
    PRINT_ERROR("AI system not initialized.");
    return 1;
  }

  if (g_ai->getAPIKey().empty()) {
    PRINT_ERROR(
        "Please set your OpenAI API key first using 'ai apikey set "
        "<YOUR_API_KEY>'.");
    return 1;
  }

  try {
    std::string response = g_ai->chatGPT(prompt, true);
    std::cout << g_ai->getModel() << ": " << response << std::endl;
    return 0;
  } catch (const std::exception& e) {
    PRINT_ERROR(std::string("Error communicating with AI: ") + e.what());
    return 1;
  }
}
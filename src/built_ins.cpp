#include "built_ins.h"
#include "main.h"
#include "cjsh_filesystem.h"
#include <unistd.h>
#include <sys/wait.h>

#define PRINT_ERROR(MSG)                              \
  do {                                                \
    last_terminal_output_error = (MSG);               \
    std::cerr << last_terminal_output_error << '\n';  \
  } while (0)

int Built_ins::builtin_command(const std::vector<std::string>& args) {
  if (args.empty()) return 1;

  auto it = builtins.find(args[0]);
  if (it != builtins.end()) {
    if (args[0] == "cd" && args.size() == 1) {
      return change_directory("");
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

int Built_ins::change_directory(const std::string& dir) {
  std::string target_dir = dir;
  
  if (target_dir.empty()) {
    const char* home_dir = getenv("HOME");
    if (!home_dir) {
      PRINT_ERROR("cjsh: HOME environment variable is not set");
      return 1;
    }
    target_dir = home_dir;
  }
  
  if (target_dir == "-") {
    if (previous_directory.empty()) {
      PRINT_ERROR("cjsh: No previous directory");
      return 1;
    }
    target_dir = previous_directory;
  }
  
  if (target_dir[0] == '~') {
    const char* home_dir = getenv("HOME");
    if (!home_dir) {
      PRINT_ERROR("cjsh: Cannot expand '~' - HOME environment variable is not set");
      return 1;
    }
    target_dir.replace(0, 1, home_dir);
  }
  
  std::filesystem::path dir_path;
  
  try {
    if (std::filesystem::path(target_dir).is_absolute()) {
      dir_path = target_dir;
    } else {
      dir_path = std::filesystem::path(current_directory) / target_dir;
    }

    if (!std::filesystem::exists(dir_path)) {
      PRINT_ERROR("cd: " + target_dir + ": No such file or directory");
      return 1;
    }
    
    if (!std::filesystem::is_directory(dir_path)) {
      PRINT_ERROR("cd: " + target_dir + ": Not a directory");
      return 1;
    }
    
    std::string old_directory = current_directory;
    
    std::filesystem::path canonical_path = std::filesystem::canonical(dir_path);
    current_directory = canonical_path.string();
    
    if (chdir(current_directory.c_str()) != 0) {
      PRINT_ERROR("cd: " + std::string(strerror(errno)));
      return 1;
    }
    
    setenv("PWD", current_directory.c_str(), 1);
    
    previous_directory = old_directory;
    
    return 0;
  }
  catch (const std::filesystem::filesystem_error& e) {
    PRINT_ERROR("cd: " + std::string(e.what()));
    return 1;
  }
  catch (const std::exception& e) {
    PRINT_ERROR("cd: Unexpected error: " + std::string(e.what()));
    return 1;
  }
}

int Built_ins::ai_commands(const std::vector<std::string>& args) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: ai_commands called with " << args.size() << " arguments" << std::endl;
    if (args.size() > 1) std::cerr << "DEBUG: ai subcommand: " << args[1] << std::endl;
  }

  if(g_ai == nullptr) {
    return 1;
  }
  unsigned int command_index = 1;
  
  if (args.size() <= command_index) {
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
    std::string fileName = (cjsh_filesystem::g_cjsh_data_path / ("OpenAPI_Chat_" + std::to_string(time(nullptr)) + ".txt")).string();
    std::ofstream file(fileName);
    if (!file.is_open()) {
      PRINT_ERROR(std::string("Error: Unable to create the chat log file at ") + fileName);
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
      PRINT_ERROR("Error: No arguments provided. Try 'help' for a list of commands.");
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
      std::cout << "The current assistant mode is " << g_ai->getAssistantType() << std::endl;
      return 0;
    }
    g_ai->setAssistantType(args[command_index + 1]);
    std::cout << "Assistant mode set to " << args[command_index + 1] << std::endl;
    return 0;
  }
  
  if (cmd == "file") {
    handle_ai_file_commands(args, command_index);
    return 0;
  }
  
  if (cmd == "directory") {
    if (args.size() <= command_index + 1) {
      std::cout << "The current directory is " << g_ai->getSaveDirectory() << std::endl;
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
      std::cout << "The current timeout flag is " << g_ai->getTimeoutFlagSeconds() << std::endl;
      return 0;
    }
    
    try {
      int timeout = std::stoi(args[command_index + 1]);
      g_ai->setTimeoutFlagSeconds(timeout);
      std::cout << "Timeout flag set to " << timeout << " seconds." << std::endl;
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
    std::cout << " dump: Display all response data and last prompt" << std::endl;
    std::cout << " mode [TYPE]: Set the assistant mode" << std::endl;
    std::cout << " file: Manage files for context (add, remove, active, available, refresh, clear)" << std::endl;
    std::cout << " directory: Manage save directory (set, clear)" << std::endl;
    std::cout << " model [MODEL]: Set the AI model" << std::endl;
    std::cout << " rejectchanges: Reject AI suggested changes" << std::endl;
    std::cout << " timeoutflag [SECONDS]: Set the timeout duration" << std::endl;
    return 0;
  }

  std::string message = cmd;
  for (unsigned int i = command_index + 1; i < args.size(); i++) {
    message += " " + args[i];
  }
  do_ai_request(message);
  return 0;
}

int Built_ins::ai_chat_commands(const std::vector<std::string>& args, int cmd_index) {
  if (args.size() <= static_cast<unsigned int>(cmd_index) + 1) {
    PRINT_ERROR("Error: No arguments provided. Try 'help' for a list of commands.");
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
      std::cerr << "Error: No arguments provided. Try 'help' for a list of commands." << std::endl;
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

int Built_ins::handle_ai_file_commands(const std::vector<std::string>& args, int cmd_index) {
  std::vector<std::string> filesAtPath;
  try {
    for (const auto& entry : std::filesystem::directory_iterator(current_directory)) {
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
    std::cout << "Total characters processed: " << g_ai->getFileContents().length() << std::endl;
    std::cout << "Files at current path: " << std::endl;
    for (const auto& file : filesAtPath) {
      std::cout << file << std::endl;
    }
    return 0;
  }
  
  const std::string& subcmd = args[cmd_index + 1];
  
  if (subcmd == "add") {
    if (args.size() <= static_cast<unsigned int>(cmd_index) + 2) {
      PRINT_ERROR("Error: No file specified. Try 'help' for a list of commands.");
      return 1;
    }
    
    if (args[cmd_index + 2] == "all") {
      int charsProcessed = g_ai->addFiles(filesAtPath);
      std::cout << "Processed " << charsProcessed << " characters from " << filesAtPath.size() << " files." << std::endl;
      return 0;
    }
    
    std::string filename = args[cmd_index + 2];
    std::string filePath = current_directory + "/" + filename;
    
    if (!std::filesystem::exists(filePath)) {
      PRINT_ERROR(std::string("Error: File not found: ") + filename);
      return 1;
    }
    
    int charsProcessed = g_ai->addFile(filePath);
    std::cout << "Processed " << charsProcessed << " characters from file: " << filename << std::endl;
    return 0;
  }
  
  if (subcmd == "remove") {
    if (args.size() <= static_cast<unsigned int>(cmd_index) + 2) {
      PRINT_ERROR("Error: No file specified. Try 'help' for a list of commands.");
      return 1;
    }
    
    if (args[cmd_index + 2] == "all") {
      int fileCount = g_ai->getFiles().size();
      g_ai->clearFiles();
      std::cout << "Removed all " << fileCount << " files from context." << std::endl;
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
      std::cout << "Total characters processed: " << g_ai->getFileContents().length() << std::endl;
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
    std::cerr << "DEBUG: plugin_commands called with " << args.size() << " arguments" << std::endl;
    if (args.size() > 1) std::cerr << "DEBUG: plugin command: " << args[1] << std::endl;
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
    std::cout << " settings [NAME] set [SETTING] [VALUE]: Modify a plugin setting" << std::endl;
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

  if(cmd == "enableall"){
    if (g_plugin) {
      for (const auto& plugin : g_plugin->get_available_plugins()) {
        g_plugin->enable_plugin(plugin);
      }
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return 0;
  }

  if(cmd == "disableall"){
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
      std::vector<std::string> listOfPluginCommands = g_plugin->get_plugin_commands(pluginName);
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
        
        if (g_plugin->update_plugin_setting(pluginName, settingName, settingValue)) {
          std::cout << "Setting " << settingName << " set to " << settingValue << " for plugin " << pluginName << std::endl;
        } else {
          std::cout << "Setting " << settingName << " not found for plugin " << pluginName << std::endl;
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
    if (std::find(enabledPlugins.begin(), enabledPlugins.end(), pluginName) != enabledPlugins.end()) {
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
          std::vector<std::string> listOfPluginCommands = g_plugin->get_plugin_commands(pluginName);
          for (const auto& cmd : listOfPluginCommands) {
            std::cout << "  " << cmd << std::endl;
          }
          return 0;
        }
      }
    } else {
      std::vector<std::string> availablePlugins = g_plugin->get_available_plugins();
      if (std::find(availablePlugins.begin(), availablePlugins.end(), pluginName) != availablePlugins.end()) {
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
  
  std::cerr << "Unknown command. Try 'help' for available commands." << std::endl;
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
        std::cerr << "Error: Theme '" << themeName << "' not found or could not be loaded." << std::endl;
        std::cout << "Staying with current theme: '" << g_current_theme << "'" << std::endl;
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
      std::cerr << "Error: Theme '" << themeName << "' not found or could not be loaded." << std::endl;
      std::cout << "Staying with current theme: '" << g_current_theme << "'" << std::endl;
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
    PRINT_ERROR("Error: Unable to open .cjshrc file for writing at " + rc_path.string());
  }
  return 0;
}

int Built_ins::approot_command() {
  std::string appRootPath = cjsh_filesystem::g_cjsh_data_path.string();
  return change_directory(appRootPath);
}

int Built_ins::version_command() {
  std::cout << "CJ's Shell v" << c_version << std::endl;
  return 0;
}

int Built_ins::uninstall_command() {
  std::cout << "To uninstall CJ's Shell run the following brew command:" << std::endl;
  std::cout << "brew uninstall cjsh" << std::endl;
  std::cout << "To remove the application data, run:" << std::endl;
  std::cout << "rm -rf " << cjsh_filesystem::g_cjsh_data_path.string() << std::endl;
  return 0;
}

int Built_ins::user_commands(const std::vector<std::string>& args) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: user_commands called with " << args.size() << " arguments" << std::endl;
    if (args.size() > 1) std::cerr << "DEBUG: user subcommand: " << args[1] << std::endl;
  }

  if (args.size() < 2) {
    PRINT_ERROR("Unknown command. No given ARGS. Try 'help'");
    return 1;
  }
  
  const std::string& cmd = args[1];
  
  if (cmd == "testing") {
    if (args.size() < 3) {
      std::cout << "Debug mode is currently " << (g_debug_mode ? "enabled." : "disabled.") << std::endl;
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
    if (g_debug_mode) std::cerr << "DEBUG: Processing checkforupdates command" << std::endl;
    
    if (args.size() < 3) {
      std::cout << "Check for updates is currently " << (g_check_updates ? "enabled." : "disabled.") << std::endl;
      return 0;
    }
    
    if (args[2] == "enable") {
      if (g_debug_mode) std::cerr << "DEBUG: Enabling check for updates" << std::endl;
      g_check_updates = true;
      std::cout << "Check for updates enabled." << std::endl;
      return 0;
    }
    
    if (args[2] == "disable") {
      if (g_debug_mode) std::cerr << "DEBUG: Disabling check for updates" << std::endl;
      g_check_updates = false;
      std::cout << "Check for updates disabled." << std::endl;
      return 0;
    }
    
    std::cerr << "Unknown command. Use 'enable' or 'disable'." << std::endl;
    return 1;
  }
  
  if (cmd == "silentupdatecheck") {
    if (args.size() < 3) {
      std::cout << "Silent update check is currently " << (g_silent_update_check ? "enabled." : "disabled.") << std::endl;
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
      std::cout << "Title line is currently " << (g_title_line ? "enabled." : "disabled.") << std::endl;
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
      std::cout << " Auto-check for updates: " << (g_check_updates ? "Enabled" : "Disabled") << std::endl;
      std::cout << " Silent update check: " << (g_silent_update_check ? "Enabled" : "Disabled") << std::endl;
      std::cout << " Update check interval: " << (g_update_check_interval / 3600) << " hours" << std::endl;
      std::cout << " Last update check: " << (g_last_update_check > 0 ? 
        std::string(ctime(&g_last_update_check)) : "Never") << std::endl;
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
        std::cout << "Update check interval set to " << hours << " hours" << std::endl;
        return 0;
      } catch (const std::exception& e) {
        std::cerr << "Invalid interval value. Please specify hours as a number" << std::endl;
        return 1;
      }
    }
    
    if (args[2] == "help") {
      std::cout << "Update commands:" << std::endl;
      std::cout << " check: Manually check for updates now" << std::endl;
      std::cout << " interval [HOURS]: Set update check interval in hours" << std::endl;
      std::cout << " help: Show this help message" << std::endl;
      return 0;
    }
    
    std::cerr << "Unknown update command. Try 'help' for available commands." << std::endl;
    return 1;
  }
  
  if (cmd == "help") {
    std::cout << "User settings commands:" << std::endl;
    std::cout << " testing: Toggle debug mode (enable/disable)" << std::endl;
    std::cout << " checkforupdates: Control whether updates are checked" << std::endl;
    std::cout << " silentupdatecheck: Toggle silent update checking (enable/disable)" << std::endl;
    std::cout << " titleline: Toggle title line display (enable/disable)" << std::endl;
    std::cout << " update: Manage update settings and perform manual update checks" << std::endl;
    return 0;
  }
  
  std::cerr << "Unknown command. Try 'user help' for available commands." << std::endl;
  return 1;
}

int Built_ins::help_command() {
  if (g_debug_mode) {
    std::cerr << "DEBUG: help_command called" << std::endl;
  }

  const std::string section_separator = "\n" + std::string(80, '-') + "\n";
  std::cout << "\nCJ'S SHELL COMMAND REFERENCE" << section_separator;
  
  // AI-related commands
  std::cout << "AI COMMANDS:\n\n";
  
  std::cout << "  ai                      Access AI assistant features and settings\n";
  std::cout << "    Usage: ai [subcommand] [options]\n";
  std::cout << "    Examples: 'ai' (enters AI chat mode), 'ai apikey set YOUR_KEY', 'ai chat history'\n";
  std::cout << "    Subcommands:\n";
  std::cout << "      log                 Save the recent chat exchange to a file\n";
  std::cout << "      apikey              View or set the OpenAI API key\n";
  std::cout << "      chat                Access AI chat commands (history, cache)\n";
  std::cout << "      get [KEY]           Retrieve specific response data\n";
  std::cout << "      dump                Display all response data and last prompt\n";
  std::cout << "      mode [TYPE]         Set or view the assistant mode\n";
  std::cout << "      file                Manage context files (add, remove, active, available)\n";
  std::cout << "      directory           Manage save directory for AI-generated files\n";
  std::cout << "      model [MODEL]       Set or view the AI model being used\n";
  std::cout << "      rejectchanges       Reject AI suggested code changes\n";
  std::cout << "      timeoutflag [SECS]  Set timeout duration for AI requests\n\n";
  
  std::cout << "  aihelp [QUERY]          Get troubleshooting help from AI\n";
  std::cout << "    Usage: aihelp [optional error description]\n";
  std::cout << "    Example: 'aihelp why is my command failing?'\n";
  std::cout << "    Note: Without arguments, will analyze the most recent error\n\n";
  
  // User settings
  std::cout << "USER SETTINGS:\n\n";
  
  std::cout << "  user                    Access and manage user settings\n";
  std::cout << "    Usage: user [subcommand] [options]\n";
  std::cout << "    Subcommands:\n";
  std::cout << "      testing             Toggle debug mode (enable/disable)\n";
  std::cout << "      checkforupdates     Control whether updates are checked\n";
  std::cout << "      silentupdatecheck   Toggle silent update checking\n";
  std::cout << "      titleline           Toggle title line display\n";
  std::cout << "      update              Manage update settings and perform manual update checks\n";
  std::cout << "    Example: 'user update check', 'user testing enable'\n\n";
  
  // Theme management
  std::cout << "THEME MANAGEMENT:\n\n";
  
  std::cout << "  theme [NAME]            View current theme or switch to a new theme\n";
  std::cout << "    Usage: theme [name] or theme load [name]\n";
  std::cout << "    Example: 'theme dark', 'theme load light'\n";
  std::cout << "    Note: Without arguments, displays the current theme and available themes\n\n";
  
  // Plugin management
  std::cout << "PLUGIN MANAGEMENT:\n\n";
  
  std::cout << "  plugin                  Manage shell plugins\n";
  std::cout << "    Usage: plugin [subcommand] [options]\n";
  std::cout << "    Subcommands:\n";
  std::cout << "      available           List all available plugins\n";
  std::cout << "      enabled             List currently enabled plugins\n";
  std::cout << "      enableall           Enable all available plugins\n";
  std::cout << "      disableall          Disable all enabled plugins\n";
  std::cout << "      enable [NAME]       Enable a specific plugin\n";
  std::cout << "      disable [NAME]      Disable a specific plugin\n";
  std::cout << "      info [NAME]         Show information about a plugin\n";
  std::cout << "      commands [NAME]     List commands provided by a plugin\n";
  std::cout << "      settings [NAME]     View or modify plugin settings\n";
  std::cout << "      install [PATH]      Install a new plugin from the given path\n";
  std::cout << "      uninstall [NAME]    Remove an installed plugin" << std::endl;
  std::cout << "    Example: 'plugin enable git_tools', 'plugin info markdown'\n\n";
  
  // Utility commands
  std::cout << "BUILT-IN SHELL COMMANDS:\n\n";
  
  std::cout << "  cd [DIR]                Change the current directory\n";
  std::cout << "    Usage: cd [directory]\n";
  std::cout << "    Examples: 'cd /path/to/dir', 'cd ~', 'cd ..' (parent directory), 'cd' (home directory)\n\n";
  
  std::cout << "  alias [NAME=VALUE]      Create or display command aliases\n";
  std::cout << "    Usage: alias [name=value]\n";
  std::cout << "    Examples: 'alias' (show all), 'alias ll=\"ls -la\"', 'alias gs=\"git status\"'\n\n";
  
  std::cout << "  unalias [NAME]          Remove a command alias\n";
  std::cout << "    Usage: unalias name\n";
  std::cout << "    Example: 'unalias ll'\n\n";
  
  std::cout << "  export [NAME=VALUE]     Set or display environment variables\n";
  std::cout << "    Usage: export [name=value]\n";
  std::cout << "    Examples: 'export' (show all), 'export PATH=\"$PATH:/new/path\"'\n\n";
  
  std::cout << "  unset [NAME]            Remove an environment variable\n";
  std::cout << "    Usage: unset name\n";
  std::cout << "    Example: 'unset TEMP_VAR'\n\n";
  
  std::cout << "  source [FILE]           Execute commands from a file\n";
  std::cout << "    Usage: source path/to/file\n";
  std::cout << "    Example: 'source ~/.cjshrc'\n\n";

  std::cout << "  eval [EXPRESSION]       Evaluate a shell expression\n";
  std::cout << "    Usage: eval expression\n";
  std::cout << "    Example: 'eval echo Hello, World!'\n\n";
  
  // Common system commands
  std::cout << "COMMON SYSTEM COMMANDS:\n\n";
  
  std::cout << "  clear                   Clear the terminal screen\n";
  std::cout << "    Usage: clear\n\n";
  
  std::cout << "  exit or quit            Exit the application\n";
  std::cout << "    Usage: exit or quit\n\n";
  
  std::cout << "  help                    Display this help message\n";
  std::cout << "    Usage: help\n";
  
  // Add new section for file system and configuration
  std::cout << section_separator;
  std::cout << "FILESYSTEM AND CONFIGURATION:\n\n";
  
  std::cout << "  Configuration Files:\n";
  std::cout << "    ~/.cjprofile          Environment variable, PATH setup, and optional statup args (login mode)\n";
  std::cout << "    ~/.cjshrc             Aliases, functions, themes, plugins (interactive mode)\n\n";
  
  std::cout << "  Primary Directories:\n";
  std::cout << "    ~/.cjsh               Main data directory for CJ's Shell\n";
  std::cout << "    ~/.cjsh/plugins       Where plugins are stored\n";
  std::cout << "    ~/.cjsh/themes        Where themes are stored\n";
  std::cout << "    ~/.cjsh/colors        Where color configurations are stored\n\n";
  
  std::cout << "  File Sourcing Order:\n";
  std::cout << "    1. ~/.profile         (if exists, login mode only)\n";
  std::cout << "    2. ~/.cjprofile       (login mode only)\n";
  std::cout << "    3. ~/.cjshrc          (interactive mode only, unless --no-source specified)\n\n";
  
  // Add new section for startup arguments
  std::cout << "STARTUP ARGUMENTS:\n\n";
  
  std::cout << "  Login and Execution:\n";
  std::cout << "    -l, --login           Start shell in login mode\n";
  std::cout << "    -c, --command CMD     Execute CMD and exit\n";
  std::cout << "    --set-as-shell        Show instructions to set as default shell\n\n";
  
  std::cout << "  Feature Toggles:\n";
  std::cout << "    --no-plugins          Disable plugins\n";
  std::cout << "    --no-themes           Disable themes\n";
  std::cout << "    --no-ai               Disable AI features\n";
  std::cout << "    --no-colors           Disable colors\n";
  std::cout << "    --no-titleline        Disable title line display\n";
  std::cout << "    --no-source           Don't source the ~/.cjshrc file\n\n";
  
  std::cout << "  Updates and Information:\n";
  std::cout << "    -v, --version         Display version and exit\n";
  std::cout << "    -h, --help            Display help and exit\n";
  std::cout << "    --update              Check for updates and install if available\n";
  std::cout << "    --check-update        Check for update\n";
  std::cout << "    --no-update           Do not check for update on launch\n";
  std::cout << "    --silent-updates      Enable silent update checks\n";
  std::cout << "    --splash              Display splash screen and exit\n";
  std::cout << "    -d, --debug           Enable debug mode\n";
  
  // Command-specific help reminder
  std::cout << section_separator;
  std::cout << "NOTE: Many commands have their own help. Try [command] help for details.\n";
  std::cout << "Examples: 'ai help', 'user help', 'plugin help', etc.\n";
  std::cout << section_separator;
  
  return 0;
}

int Built_ins::aihelp_command(const std::vector<std::string>& args) {
  if (!g_ai || g_ai->getAPIKey().empty()) {
    PRINT_ERROR("Please set your OpenAI API key first.");
    return 1;
  }
  const char* status_env = getenv("STATUS");
  if (!status_env) {
    std::cerr << "The last executed command status is unavailable" << std::endl;
    return 0;
  }
  int status = std::atoi(status_env);
  if (status == 0) {
    std::cerr << "The last executed command returned exitcode 0" << std::endl;
    return 0;
  }
  std::string message;
  if (args.size() > 1) {
    for (size_t i = 1; i < args.size(); ++i) {
      message += args[i] + " ";
    }
  } else {
    message = "I am encountering some issues with a cjsh command and would like some help. This is the most recent output: " + g_shell -> last_terminal_output_error + " Here is the command I used: " + g_shell ->last_command;
  }
  
  if (g_debug_mode) {
    std::cout << "Sending to AI: " << message << std::endl;
  }
  
  std::cout << g_ai->forceDirectChatGPT(message, false) << std::endl;
  return 0;
}

int Built_ins::alias_command(const std::vector<std::string>& args) {
  if (args.size() == 1) {
    auto& aliases = g_shell->get_aliases();
    if (aliases.empty()) {
      std::cout << "No aliases defined." << std::endl;
    } else {
      for (const auto& [name, value] : aliases) {
        std::cout << "alias " << name << "='" << value << "'" << std::endl;
      }
    }
    return 0;
  }

  bool all_successful = true;
  auto& aliases = g_shell->get_aliases();
  
  for (size_t i = 1; i < args.size(); ++i) {
    std::string name, value;
    if (parse_assignment(args[i], name, value)) {
      aliases[name] = value;
      save_alias_to_file(name, value);
      if (g_debug_mode) {
        std::cout << "Added alias: " << name << "='" << value << "'" << std::endl;
      }
    } else {
      PRINT_ERROR("alias: invalid assignment: " + args[i]);
      all_successful = false;
    }
  }

  if (g_shell) {
    g_shell->set_aliases(aliases);
  }
  
  return all_successful ? 0 : 1;
}

int Built_ins::export_command(const std::vector<std::string>& args) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: export_command called with " << args.size() << " arguments" << std::endl;
  }

  if (args.size() == 1) {
    if (g_debug_mode) std::cerr << "DEBUG: Listing all environment variables" << std::endl;
    extern char **environ;
    for (char **env = environ; *env; ++env) {
      std::cout << "export " << *env << std::endl;
    }
    return 0;
  }

  bool all_successful = true;
  for (size_t i = 1; i < args.size(); ++i) {
    std::string name, value;
    if (parse_assignment(args[i], name, value)) {
      env_vars[name] = value;
      
      setenv(name.c_str(), value.c_str(), 1);
      
      if (shell && shell->get_login_mode()) {
        save_env_var_to_file(name, value);
      } else if (g_debug_mode) {
        std::cout << "Note: Environment variable set for this session only (not in login mode)" << std::endl;
      }
      
      if (g_debug_mode) {
        std::cout << "Set environment variable: " << name << "='" << value << "'" << std::endl;
      }
    } else {
      const char* env_val = getenv(args[i].c_str());
      if (env_val) {
        std::cout << "export " << args[i] << "='" << env_val << "'" << std::endl;
      } else {
        PRINT_ERROR("export: " + args[i] + ": not found");
        all_successful = false;
      }
    }
  }

  if (g_shell) {
    g_shell->set_env_vars(env_vars);
  }
  
  return all_successful ? 0 : 1;
}
int Built_ins::unset_command(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    PRINT_ERROR("unset: not enough arguments");
    return 1;
  }

  bool success = true;
  
  for (size_t i = 1; i < args.size(); ++i) {
    const std::string& name = args[i];
    
    env_vars.erase(name);
    
    if (unsetenv(name.c_str()) != 0) {
      PRINT_ERROR(std::string("unset: error unsetting ") + name + ": " + strerror(errno));
      success = false;
    } else {
      if (shell && shell->get_login_mode()) {
        remove_env_var_from_file(name);
      }
      
      if (g_debug_mode) {
        std::cout << "Unset environment variable: " << name << std::endl;
      }
    }
  }

  if (g_shell) {
    g_shell->set_env_vars(env_vars);
  }
  
  return success ? 0 : 1;
}

int Built_ins::parse_assignment(const std::string& arg, std::string& name, std::string& value) {
  size_t equals_pos = arg.find('=');
  if (equals_pos == std::string::npos || equals_pos == 0) {
    return false;
  }
  
  name = arg.substr(0, equals_pos);
  value = arg.substr(equals_pos + 1);

  if (value.size() >= 2) {
    if ((value.front() == '"' && value.back() == '"') ||
        (value.front() == '\'' && value.back() == '\'')) {
      value = value.substr(1, value.size() - 2);
    }
  }
  
  return true;
}

int Built_ins::save_alias_to_file(const std::string& name, const std::string& value) {
  std::filesystem::path source_path = cjsh_filesystem::g_cjsh_source_path;
  
  std::vector<std::string> lines;
  std::string line;
  std::ifstream read_file(source_path);
  
  bool alias_found = false;
  
  if (read_file.is_open()) {
    while (std::getline(read_file, line)) {
      if (line.find("alias " + name + "=") == 0) {
        lines.push_back("alias " + name + "='" + value + "'");
        alias_found = true;
      } else {
        lines.push_back(line);
      }
    }
    read_file.close();
  }
  
  if (!alias_found) {
    lines.push_back("alias " + name + "='" + value + "'");
  }
  
  std::ofstream write_file(source_path);
  if (write_file.is_open()) {
    for (const auto& l : lines) {
      write_file << l << std::endl;
    }
    write_file.close();
    
    if (g_debug_mode) {
      std::cout << "Alias " << name << " saved to " << source_path.string() << std::endl;
    }
  } else {
    PRINT_ERROR("Error: Unable to open source file for writing at " + source_path.string());
  }
  return 0;
}

int Built_ins::unalias_command(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    std::cerr << "unalias: not enough arguments" << std::endl;
    return 1;
  }

  bool success = true;
  auto& aliases = g_shell->get_aliases();

  for (size_t i = 1; i < args.size(); ++i) {
    const std::string& name = args[i];
    auto it = aliases.find(name);
    
    if (it != aliases.end()) {
      aliases.erase(it);
      remove_alias_from_file(name);
      if (g_debug_mode) {
        std::cout << "Removed alias: " << name << std::endl;
      }
    } else {
      std::cerr << "unalias: " << name << ": not found" << std::endl;
      success = false;
    }
  }

  if (g_shell) {
    g_shell->set_aliases(aliases);
  }
  
  return success ? 0 : 1;
}

int Built_ins::remove_alias_from_file(const std::string& name) {
  std::filesystem::path source_path = cjsh_filesystem::g_cjsh_source_path;
  
  std::vector<std::string> lines;
  std::string line;
  std::ifstream read_file(source_path);
  
  if (read_file.is_open()) {
    while (std::getline(read_file, line)) {
      if (line.find("alias " + name + "=") != 0) {
        lines.push_back(line);
      }
    }
    read_file.close();
  }
  
  std::ofstream write_file(source_path);
  if (write_file.is_open()) {
    for (const auto& l : lines) {
      write_file << l << std::endl;
    }
    write_file.close();
    
    if (g_debug_mode) {
      std::cout << "Alias " << name << " removed from " << source_path.string() << std::endl;
    }
  } else {
    PRINT_ERROR("Error: Unable to open source file for writing at " + source_path.string());
  }
  return 0;
}

int Built_ins::save_env_var_to_file(const std::string& name, const std::string& value) {
  if (!shell || !shell->get_login_mode()) {
    if (g_debug_mode) {
      std::cerr << "Warning: Attempted to save environment variable to config file when not in login mode" << std::endl;
    }
    return 0;
  }
  
  std::filesystem::path config_path = cjsh_filesystem::g_cjsh_profile_path;
  
  std::vector<std::string> lines;
  std::string line;
  std::ifstream read_file(config_path);
  
  bool env_var_found = false;
  
  if (read_file.is_open()) {
    while (std::getline(read_file, line)) {
      if (line.find("export " + name + "=") == 0) {
        lines.push_back("export " + name + "='" + value + "'");
        env_var_found = true;
      } else {
        lines.push_back(line);
      }
    }
    read_file.close();
  }
  
  if (!env_var_found) {
    lines.push_back("export " + name + "='" + value + "'");
  }
  
  std::ofstream write_file(config_path);
  if (write_file.is_open()) {
    for (const auto& l : lines) {
      write_file << l << std::endl;
    }
    write_file.close();
    
    if (g_debug_mode) {
      std::cout << "Environment variable " << name << " saved to " << config_path.string() << std::endl;
    }
  } else {
    PRINT_ERROR("Error: Unable to open config file for writing at " + config_path.string());
  }
  return 0;
}

int Built_ins::remove_env_var_from_file(const std::string& name) {
  if (!shell || !shell->get_login_mode()) {
    if (g_debug_mode) {
      std::cerr << "Warning: Attempted to remove environment variable from config file when not in login mode" << std::endl;
    }
    return 0;
  }
  
  std::filesystem::path config_path = cjsh_filesystem::g_cjsh_profile_path;
  
  std::vector<std::string> lines;
  std::string line;
  std::ifstream read_file(config_path);
  
  if (read_file.is_open()) {
    while (std::getline(read_file, line)) {
      if (line.find("export " + name + "=") != 0) {
        lines.push_back(line);
      }
    }
    read_file.close();
  }

  std::ofstream write_file(config_path);
  if (write_file.is_open()) {
    for (const auto& l : lines) {
      write_file << l << std::endl;
    }
    write_file.close();
    
    if (g_debug_mode) {
      std::cout << "Environment variable " << name << " removed from " << config_path.string() << std::endl;
    }
  } else {
    PRINT_ERROR("Error: Unable to open config file for writing at " + config_path.string());
  }
  return 0;
}

int Built_ins::do_ai_request(const std::string& prompt) {
  if (!g_ai) {
    PRINT_ERROR("AI system not initialized.");
    return 1;
  }

  if (g_ai->getAPIKey().empty()) {
    PRINT_ERROR("Please set your OpenAI API key first using 'ai apikey set <YOUR_API_KEY>'.");
    return 1;
  }

  try {
    std::string response = g_ai->chatGPT(prompt, true);
    std::cout << response << std::endl;
    return 0;
  } catch (const std::exception& e) {
    PRINT_ERROR(std::string("Error communicating with AI: ") + e.what());
    return 1;
  }
}

int Built_ins::restart_command() {
  std::cout << "Restarting shell..." << std::endl;
  
  std::filesystem::path shell_path = cjsh_filesystem::g_cjsh_path;
  
  if (!std::filesystem::exists(shell_path)) {
    PRINT_ERROR("Error: Could not find shell executable at " + shell_path.string());
    last_terminal_output_error = "restart: executable not found";
    return 1;
  }
  
  std::string path_str = shell_path.string();
  const char* path_cstr = path_str.c_str();
  char* const args[] = {
    const_cast<char*>(path_cstr),
    nullptr
  };
  if (execv(path_cstr, args) == -1) {
    last_terminal_output_error = "Error restarting shell: " + std::string(strerror(errno));
    PRINT_ERROR(last_terminal_output_error);
     return 1;
   }
  return 0;
}

int Built_ins::eval_command(const std::vector<std::string>& args) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: eval_command called with " << args.size() << " arguments" << std::endl;
  }

  if (args.size() < 2) {
    PRINT_ERROR("eval: missing arguments");
    return 1;
  }

  std::string command_to_eval;
  for (size_t i = 1; i < args.size(); ++i) {
    if (i > 1) command_to_eval += " ";
    command_to_eval += args[i];
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: Evaluating command: " << command_to_eval << std::endl;
  }
  
  if (shell) {
    int result = shell->execute_command(command_to_eval);
    if (g_debug_mode) {
      std::cerr << "DEBUG: eval command returned: " << result << std::endl;
    }
    return result;
  } else {
    PRINT_ERROR("eval: shell not initialized");
    return 1;
  }
}
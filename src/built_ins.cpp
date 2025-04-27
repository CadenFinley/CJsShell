#include "built_ins.h"
#include "main.h"
#include "cjsh_filesystem.h"

bool Built_ins::builtin_command(const std::vector<std::string>& args) {
  if (args.empty()) return false;

  auto it = builtins.find(args[0]);
  if (it != builtins.end()) {
    return it->second(args);
  }

  return false;
}

bool Built_ins::change_directory(const std::string& dir, std::string& result) {
  std::string target_dir = dir;
  
  if (target_dir.empty()) {
    const char* home_dir = getenv("HOME");
    if (!home_dir) {
      result = "cjsh: HOME environment variable is not set";
      return false;
    }
    target_dir = home_dir;
  }
  
  if (target_dir[0] == '~') {
    const char* home_dir = getenv("HOME");
    if (!home_dir) {
      result = "cjsh: Cannot expand '~' - HOME environment variable is not set";
      return false;
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
      result = "cd: " + target_dir + ": No such file or directory";
      return false;
    }
    
    if (!std::filesystem::is_directory(dir_path)) {
      result = "cd: " + target_dir + ": Not a directory";
      return false;
    }
    
    std::filesystem::path canonical_path = std::filesystem::canonical(dir_path);
    current_directory = canonical_path.string();
    
    if (chdir(current_directory.c_str()) != 0) {
      result = "cd: " + std::string(strerror(errno));
      return false;
    }
    
    setenv("PWD", current_directory.c_str(), 1);
    
    return true;
  }
  catch (const std::filesystem::filesystem_error& e) {
    result = "cd: " + std::string(e.what());
    return false;
  }
  catch (const std::exception& e) {
    result = "cd: Unexpected error: " + std::string(e.what());
    return false;
  }
}

bool Built_ins::ai_commands(const std::vector<std::string>& args) {
  unsigned int command_index = 1;
  
  if (args.size() <= command_index) {
    g_menu_terminal = false;
    if (!g_ai->getChatCache().empty()) {
      std::cout << "Chat history:" << std::endl;
      for (const auto& message : g_ai->getChatCache()) {
        std::cout << message << std::endl;
      }
    }
    return true;
  }
  
  const std::string& cmd = args[command_index];
  
  if (cmd == "log") {
    std::string lastChatSent = g_ai->getLastPromptUsed();
    std::string lastChatReceived = g_ai->getLastResponseReceived();
    std::string fileName = (cjsh_filesystem::g_cjsh_data_path / ("OpenAPI_Chat_" + std::to_string(time(nullptr)) + ".txt")).string();
    std::ofstream file(fileName);
    if (file.is_open()) {
      file << "Chat Sent: " << lastChatSent << "\n";
      file << "Chat Received: " << lastChatReceived << "\n";
      file.close();
      std::cout << "Chat log saved to " << fileName << std::endl;
    } else {
      std::cerr << "Error: Unable to create the chat log file at " << fileName << std::endl;
    }
    return true;
  }
  
  if (cmd == "apikey") {
    if (args.size() <= command_index + 1) {
      std::cout << g_ai->getAPIKey() << std::endl;
      return true;
    }
    
    if (args[command_index + 1] == "set") {
      if (args.size() <= command_index + 2) {
        std::cerr << "Error: No API key provided. Try 'help' for a list of commands." << std::endl;
        return false;
      }
      g_ai->setAPIKey(args[command_index + 2]);
      if (g_ai->testAPIKey(g_ai->getAPIKey())) {
        std::cout << "OpenAI API key set successfully." << std::endl;
        // TODO: Implement writeUserData() equivalent
        return true;
      } else {
        std::cerr << "Error: Invalid API key." << std::endl;
        return false;
      }
    }
    
    if (args[command_index + 1] == "get") {
      std::cout << g_ai->getAPIKey() << std::endl;
      return true;
    }
    
    std::cerr << "Error: Unknown command. Try 'help' for a list of commands." << std::endl;
    return false;
  }
  
  if (cmd == "chat") {
    ai_chat_commands(args, command_index);
    return true;
  }
  
  if (cmd == "get") {
    if (args.size() <= command_index + 1) {
      std::cerr << "Error: No arguments provided. Try 'help' for a list of commands." << std::endl;
      return false;
    }
    std::cout << g_ai->getResponseData(args[command_index + 1]) << std::endl;
    return true;
  }
  
  if (cmd == "dump") {
    std::cout << g_ai->getResponseData("all") << std::endl;
    std::cout << g_ai->getLastPromptUsed() << std::endl;
    return true;
  }
  
  if (cmd == "mode") {
    if (args.size() <= command_index + 1) {
      std::cout << "The current assistant mode is " << g_ai->getAssistantType() << std::endl;
      return true;
    }
    g_ai->setAssistantType(args[command_index + 1]);
    std::cout << "Assistant mode set to " << args[command_index + 1] << std::endl;
    return true;
  }
  
  if (cmd == "file") {
    handle_ai_file_commands(args, command_index);
    return true;
  }
  
  if (cmd == "directory") {
    if (args.size() <= command_index + 1) {
      std::cout << "The current directory is " << g_ai->getSaveDirectory() << std::endl;
      return true;
    }
    
    if (args[command_index + 1] == "set") {
      g_ai->setSaveDirectory(current_directory);
      std::cout << "Directory set to " << current_directory << std::endl;
      return true;
    }
    
    if (args[command_index + 1] == "clear") {
      g_ai->setSaveDirectory(cjsh_filesystem::g_cjsh_data_path);
      std::cout << "Directory set to default." << std::endl;
      return true;
    }
    return false;
  }
  
  if (cmd == "model") {
    if (args.size() <= command_index + 1) {
      std::cout << "The current model is " << g_ai->getModel() << std::endl;
      return true;
    }
    g_ai->setModel(args[command_index + 1]);
    std::cout << "Model set to " << args[command_index + 1] << std::endl;
    return true;
  }
  
  if (cmd == "rejectchanges") {
    g_ai->rejectChanges();
    std::cout << "Changes rejected." << std::endl;
    return true;
  }
  
  if (cmd == "timeoutflag") {
    if (args.size() <= command_index + 1) {
      std::cout << "The current timeout flag is " << g_ai->getTimeoutFlagSeconds() << std::endl;
      return true;
    }
    
    try {
      int timeout = std::stoi(args[command_index + 1]);
      g_ai->setTimeoutFlagSeconds(timeout);
      std::cout << "Timeout flag set to " << timeout << " seconds." << std::endl;
    } catch (const std::exception& e) {
      std::cerr << "Error: Invalid timeout value. Please provide a number." << std::endl;
    }
    return false;
  }
  
  if (cmd == "help") {
    std::cout << "AI settings commands:" << std::endl;
    std::cout << " log: Save recent chat exchange to a file" << std::endl;
    std::cout << " apikey: Manage OpenAI API key (set/get)" << std::endl;
    std::cout << " chat: Access AI chat commands" << std::endl;
    std::cout << " get [KEY]: Retrieve specific response data" << std::endl;
    std::cout << " dump: Display all response data and last prompt" << std::endl;
    std::cout << " mode [TYPE]: Set the assistant mode" << std::endl;
    std::cout << " file: Manage files for context (add, remove, active, available, refresh, clear)" << std::endl;
    std::cout << " directory: Manage save directory (set, clear)" << std::endl;
    std::cout << " model [MODEL]: Set the AI model" << std::endl;
    std::cout << " rejectchanges: Reject AI suggested changes" << std::endl;
    std::cout << " timeoutflag [SECONDS]: Set the timeout duration" << std::endl;
    return true;
  }
  
  // If we get here, treat as a direct message to AI
  std::string message = cmd;
  for (unsigned int i = command_index + 1; i < args.size(); i++) {
    message += " " + args[i];
  }
  do_ai_request(message);
  return true;
}

bool Built_ins::ai_chat_commands(const std::vector<std::string>& args, int cmd_index) {
  if (args.size() <= static_cast<unsigned int>(cmd_index) + 1) {
    std::cerr << "Error: No arguments provided. Try 'help' for a list of commands." << std::endl;
    return false;
  }
  
  const std::string& subcmd = args[cmd_index + 1];
  
  if (subcmd == "history") {
    if (args.size() <= static_cast<unsigned int>(cmd_index) + 2) {
      // Show chat history
      if (!g_ai->getChatCache().empty()) {
        std::cout << "Chat history:" << std::endl;
        for (const auto& message : g_ai->getChatCache()) {
          std::cout << message << std::endl;
        }
      } else {
        std::cout << "No chat history available." << std::endl;
      }
      return true;
    }
    
    if (args[cmd_index + 2] == "clear") {
      g_ai->clearChatCache();
      // TODO: Update savedChatCache and writeUserData()
      std::cout << "Chat history cleared." << std::endl;
      return true;
    }
  }
  
  if (subcmd == "cache") {
    if (args.size() <= static_cast<unsigned int>(cmd_index) + 2) {
      std::cerr << "Error: No arguments provided. Try 'help' for a list of commands." << std::endl;
      return false;
    }
    
    if (args[cmd_index + 2] == "enable") {
      g_ai->setCacheTokens(true);
      std::cout << "Cache tokens enabled." << std::endl;
      return true;
    }
    
    if (args[cmd_index + 2] == "disable") {
      g_ai->setCacheTokens(false);
      std::cout << "Cache tokens disabled." << std::endl;
      return true;
    }
    
    if (args[cmd_index + 2] == "clear") {
      g_ai->clearAllCachedTokens();
      std::cout << "Chat history cleared." << std::endl;
      return true;
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
    return true;
  }
  
  // If we get here, treat as a direct message to AI
  std::string message = subcmd;
  for (unsigned int i = cmd_index + 2; i < args.size(); i++) {
    message += " " + args[i];
  }
  
  std::cout << "Sent message to GPT: " << message << std::endl;
  do_ai_request(message);
  return true;
}

bool Built_ins::handle_ai_file_commands(const std::vector<std::string>& args, int cmd_index) {
  std::vector<std::string> filesAtPath;
  try {
    for (const auto& entry : std::filesystem::directory_iterator(current_directory)) {
      if (entry.is_regular_file()) {
        filesAtPath.push_back(entry.path().filename().string());
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Error reading directory: " << e.what() << std::endl;
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
    return true;
  }
  
  const std::string& subcmd = args[cmd_index + 1];
  
  if (subcmd == "add") {
    if (args.size() <= static_cast<unsigned int>(cmd_index) + 2) {
      std::cerr << "Error: No file specified. Try 'help' for a list of commands." << std::endl;
      return false;
    }
    
    if (args[cmd_index + 2] == "all") {
      int charsProcessed = g_ai->addFiles(filesAtPath);
      std::cout << "Processed " << charsProcessed << " characters from " << filesAtPath.size() << " files." << std::endl;
      return true;
    }
    
    std::string filename = args[cmd_index + 2];
    std::string filePath = current_directory + "/" + filename;
    
    if (!std::filesystem::exists(filePath)) {
      std::cerr << "Error: File not found: " << filename << std::endl;
      return false;
    }
    
    int charsProcessed = g_ai->addFile(filePath);
    std::cout << "Processed " << charsProcessed << " characters from file: " << filename << std::endl;
    return true;
  }
  
  if (subcmd == "remove") {
    if (args.size() <= static_cast<unsigned int>(cmd_index) + 2) {
      std::cerr << "Error: No file specified. Try 'help' for a list of commands." << std::endl;
      return false;
    }
    
    if (args[cmd_index + 2] == "all") {
      int fileCount = g_ai->getFiles().size();
      g_ai->clearFiles();
      std::cout << "Removed all " << fileCount << " files from context." << std::endl;
      return true;
    }
    
    std::string filename = args[cmd_index + 2];
    std::string filePath = current_directory + "/" + filename;
    
    if (!std::filesystem::exists(filePath)) {
      std::cerr << "Error: File not found: " << filename << std::endl;
      return false;
    }
    
    g_ai->removeFile(filePath);
    std::cout << "Removed file: " << filename << " from context." << std::endl;
    return true;
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
    return true;
  }
  
  if (subcmd == "available") {
    std::cout << "Files at current path: " << std::endl;
    for (const auto& file : filesAtPath) {
      std::cout << file << std::endl;
    }
    return true;
  }
  
  if (subcmd == "refresh") {
    g_ai->refreshFiles();
    std::cout << "Files refreshed." << std::endl;
    return true;
  }
  
  if (subcmd == "clear") {
    g_ai->clearFiles();
    std::cout << "Files cleared." << std::endl;
    return true;
  }
  
  std::cerr << "Error: Unknown command. Try 'help' for a list of commands." << std::endl;
  return false;
}

bool Built_ins::plugin_commands(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
    return false;
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
    return true;
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
    return true;
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
    return true;
  }

  if(cmd == "enableall"){
    if (g_plugin) {
      for (const auto& plugin : g_plugin->get_available_plugins()) {
        g_plugin->enable_plugin(plugin);
      }
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return true;
  }

  if(cmd == "disableall"){
    if (g_plugin) {
      for (const auto& plugin : g_plugin->get_enabled_plugins()) {
        g_plugin->disable_plugin(plugin);
      }
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return true;
  }
  
  if (cmd == "install" && args.size() > 2) {
    if (g_plugin) {
      std::string pluginPath = args[2];
      g_plugin->install_plugin(pluginPath);
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return true;
  }
  
  if (cmd == "uninstall" && args.size() > 2) {
    if (g_plugin) {
      std::string pluginName = args[2];
      g_plugin->uninstall_plugin(pluginName);
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return true;
  }
  
  if (cmd == "info" && args.size() > 2) {
    if (g_plugin) {
      std::string pluginName = args[2];
      std::cout << g_plugin->get_plugin_info(pluginName) << std::endl;
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return true;
  }
  
  if (cmd == "enable" && args.size() > 2) {
    if (g_plugin) {
      std::string pluginName = args[2];
      g_plugin->enable_plugin(pluginName);
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return true;
  }
  
  if (cmd == "disable" && args.size() > 2) {
    if (g_plugin) {
      std::string pluginName = args[2];
      g_plugin->disable_plugin(pluginName);
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return true;
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
    return true;
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
        return true;
      }
      
      if (args.size() > 4 && args[3] == "set") {
        std::string pluginName = args[2];
        std::string settingName = args[4];
        std::string settingValue = args.size() > 5 ? args[5] : "";
        
        if (g_plugin->update_plugin_setting(pluginName, settingName, settingValue)) {
          std::cout << "Setting " << settingName << " set to " << settingValue << " for plugin " << pluginName << std::endl;
          return true;
        } else {
          std::cout << "Setting " << settingName << " not found for plugin " << pluginName << std::endl;
          return true;
        }
      }
    } else {
      std::cerr << "Plugin manager not initialized" << std::endl;
    }
    return false;
  }
  
  if (g_plugin) {
    std::string pluginName = cmd;
    std::vector<std::string> enabledPlugins = g_plugin->get_enabled_plugins();
    if (std::find(enabledPlugins.begin(), enabledPlugins.end(), pluginName) != enabledPlugins.end()) {
      if (args.size() > 2) {
        if (args[2] == "enable") {
          g_plugin->enable_plugin(pluginName);
          return true;
        }
        if (args[2] == "disable") {
          g_plugin->disable_plugin(pluginName);
          return true;
        }
        if (args[2] == "info") {
          std::cout << g_plugin->get_plugin_info(pluginName) << std::endl;
          return true;
        }
        if (args[2] == "commands" || args[2] == "cmds" || args[2] == "help") {
          std::cout << "Commands for " << pluginName << ":" << std::endl;
          std::vector<std::string> listOfPluginCommands = g_plugin->get_plugin_commands(pluginName);
          for (const auto& cmd : listOfPluginCommands) {
            std::cout << "  " << cmd << std::endl;
          }
          return true;
        }
      }
    } else {
      std::vector<std::string> availablePlugins = g_plugin->get_available_plugins();
      if (std::find(availablePlugins.begin(), availablePlugins.end(), pluginName) != availablePlugins.end()) {
        if (args.size() > 2 && args[2] == "enable") {
          g_plugin->enable_plugin(pluginName);
          return true;
        }
        std::cerr << "Plugin: " << pluginName << " is disabled." << std::endl;
        return false;
      } else {
        std::cerr << "Plugin " << pluginName << " does not exist." << std::endl;
        return false;
      }
    }
  }
  
  std::cerr << "Unknown command. Try 'plugin help' for available commands." << std::endl;
  return false;
}

bool Built_ins::theme_commands(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    if (g_theme) {
      std::cout << "Current theme: " << g_current_theme << std::endl;
      std::cout << "Available themes: " << std::endl;
      for (const auto& theme : g_theme->list_themes()) {
        std::cout << "  " << theme << std::endl;
      }
    } else {
      std::cerr << "Theme manager not initialized" << std::endl;
    }
    return true;
  }
  
  if (args[1] == "load" && args.size() > 2) {
    if (g_theme) {
      std::string themeName = args[2];
      if (g_theme->load_theme(themeName)) {
        g_current_theme = themeName;
      }
    } else {
      std::cerr << "Theme manager not initialized" << std::endl;
    }
    return true;
  }
  
  if (g_theme) {
    std::string themeName = args[1];
    if (g_theme->load_theme(themeName)) {
      g_current_theme = themeName;
    }
  } else {
    std::cerr << "Theme manager not initialized" << std::endl;
  }
  return true;
}

bool Built_ins::approot_command() {
  std::string appRootPath = cjsh_filesystem::g_cjsh_data_path.string();
  std::string result;
  if (change_directory(appRootPath, result)) {
    std::cout << "Changed to application root directory: " << appRootPath << std::endl;
    return true;
  } else {
    std::cerr << result << std::endl;
    return false;
  }
}

bool Built_ins::version_command() {
  std::cout << "CJ's g_shell v" << c_version << std::endl;
  return true;
}

bool Built_ins::uninstall_command() {
  if (g_plugin) {
    std::vector<std::string> enabledPlugins = g_plugin->get_enabled_plugins();
    if (!enabledPlugins.empty()) {
      std::cerr << "Please disable all plugins before uninstalling." << std::endl;
      return false;
    }
  }
  
  std::cout << "Are you sure you want to uninstall cjsh? (y/n): ";
  char confirmation;
  std::cin >> confirmation;
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  
  if (confirmation == 'y' || confirmation == 'Y') {
    std::filesystem::path uninstallScriptPath = cjsh_filesystem::g_cjsh_data_path / "cjsh_uninstall.sh";
    
    if (!std::filesystem::exists(uninstallScriptPath)) {
      std::cerr << "Uninstall script not found." << std::endl;
      return false;
    }
    
    std::cout << "Do you want to remove all user data? (y/n): ";
    char removeUserData;
    std::cin >> removeUserData;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    
    std::string uninstallCommand = uninstallScriptPath.string();
    if (removeUserData == 'y' || removeUserData == 'Y') {
      uninstallCommand += " --all";
    }
    
    std::cout << "Running uninstall script..." << std::endl;
    system(uninstallCommand.c_str());
    g_exit_flag = true;
    return true;
  } else {
    std::cout << "Uninstall cancelled." << std::endl;
    return false;
  }
}

bool Built_ins::user_commands(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
    return false;
  }
  
  const std::string& cmd = args[1];
  
  if (cmd == "startup") {
    if (args.size() < 3) {
      if (!g_startup_commands.empty()) {
        std::cout << "Startup commands:" << std::endl;
        for (const auto& command : g_startup_commands) {
          std::cout << command << std::endl;
        }
      } else {
        std::cerr << "No startup commands." << std::endl;
      }
      return true;
    }
    
    if (args[2] == "add" && args.size() > 3) {
      g_startup_commands.push_back(args[3]);
      std::cout << "Command added to startup commands." << std::endl;
      return true;
    }
    
    if (args[2] == "remove" && args.size() > 3) {
      std::string commandToRemove = args[3];
      auto it = std::find(g_startup_commands.begin(), g_startup_commands.end(), commandToRemove);
      if (it != g_startup_commands.end()) {
        g_startup_commands.erase(it);
        std::cout << "Command removed from startup commands." << std::endl;
      } else {
        std::cerr << "Command not found in startup commands." << std::endl;
      }
      return true;
    }
    
    if (args[2] == "clear") {
      g_startup_commands.clear();
      std::cout << "Startup commands cleared." << std::endl;
      return true;
    }
    
    if (args[2] == "list") {
      if (!g_startup_commands.empty()) {
        std::cout << "Startup commands:" << std::endl;
        for (const auto& command : g_startup_commands) {
          std::cout << command << std::endl;
        }
      } else {
        std::cerr << "No startup commands." << std::endl;
      }
      return true;
    }
    
    if (args[2] == "help") {
      std::cout << "Startup commands:" << std::endl;
      std::cout << " add [CMD]: Add a startup command" << std::endl;
      std::cout << " remove [CMD]: Remove a startup command" << std::endl;
      std::cout << " clear: Remove all startup commands" << std::endl;
      std::cout << " list: Show all startup commands" << std::endl;
      return true;
    }
    
    std::cerr << "Unknown startup command. Try 'user startup help'" << std::endl;
    return false;
  }
  
  if (cmd == "testing") {
    if (args.size() < 3) {
      std::cout << "Debug mode is currently " << (g_debug_mode ? "enabled." : "disabled.") << std::endl;
      return true;
    }
    
    if (args[2] == "enable") {
      g_debug_mode = true;
      std::cout << "Debug mode enabled." << std::endl;
      return true;
    }
    
    if (args[2] == "disable") {
      g_debug_mode = false;
      std::cout << "Debug mode disabled." << std::endl;
      return true;
    }
    
    std::cerr << "Unknown testing command. Use 'enable' or 'disable'." << std::endl;
    return false;
  }
  
  if (cmd == "checkforupdates") {
    if (args.size() < 3) {
      std::cout << "Check for updates is currently " << (g_check_updates ? "enabled." : "disabled.") << std::endl;
      return true;
    }
    
    if (args[2] == "enable") {
      g_check_updates = true;
      std::cout << "Check for updates enabled." << std::endl;
      return true;
    }
    
    if (args[2] == "disable") {
      g_check_updates = false;
      std::cout << "Check for updates disabled." << std::endl;
      return true;
    }
    
    std::cerr << "Unknown command. Use 'enable' or 'disable'." << std::endl;
    return false;
  }
  
  if (cmd == "silentupdatecheck") {
    if (args.size() < 3) {
      std::cout << "Silent update check is currently " << (g_silent_update_check ? "enabled." : "disabled.") << std::endl;
      return true;
    }
    
    if (args[2] == "enable") {
      g_silent_update_check = true;
      std::cout << "Silent update check enabled." << std::endl;
      return true;
    }
    
    if (args[2] == "disable") {
      g_silent_update_check = false;
      std::cout << "Silent update check disabled." << std::endl;
      return true;
    }
    
    std::cerr << "Unknown command. Use 'enable' or 'disable'." << std::endl;
    return false;
  }
  
  if (cmd == "titleline") {
    if (args.size() < 3) {
      std::cout << "Title line is currently " << (g_title_line ? "enabled." : "disabled.") << std::endl;
      return true;
    }
    
    if (args[2] == "enable") {
      g_title_line = true;
      std::cout << "Title line enabled." << std::endl;
      return true;
    }
    
    if (args[2] == "disable") {
      g_title_line = false;
      std::cout << "Title line disabled." << std::endl;
      return true;
    }
    
    std::cerr << "Unknown command. Use 'enable' or 'disable'." << std::endl;
    return false;
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
      return true;
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
      return true;
    }
    
    if (args[2] == "interval" && args.size() > 3) {
      try {
        int hours = std::stoi(args[3]);
        if (hours < 1) {
          std::cerr << "Interval must be at least 1 hour" << std::endl;
          return false;
        }
        g_update_check_interval = hours * 3600;
        std::cout << "Update check interval set to " << hours << " hours" << std::endl;
        return true;
      } catch (const std::exception& e) {
        std::cerr << "Invalid interval value. Please specify hours as a number" << std::endl;
        return false;
      }
    }
    
    if (args[2] == "help") {
      std::cout << "Update commands:" << std::endl;
      std::cout << " check: Manually check for updates now" << std::endl;
      std::cout << " interval [HOURS]: Set update check interval in hours" << std::endl;
      std::cout << " help: Show this help message" << std::endl;
      return true;
    }
    
    std::cerr << "Unknown update command. Try 'help' for available commands." << std::endl;
    return false;
  }
  
  if (cmd == "help") {
    std::cout << "User settings commands:" << std::endl;
    std::cout << " startup: Manage startup commands (add, remove, clear, list)" << std::endl;
    std::cout << " testing: Toggle debug mode (enable/disable)" << std::endl;
    std::cout << " checkforupdates: Toggle update checking (enable/disable)" << std::endl;
    std::cout << " silentupdatecheck: Toggle silent update check (enable/disable)" << std::endl;
    std::cout << " titleline: Toggle title line display (enable/disable)" << std::endl;
    std::cout << " update: Manage update settings and perform manual update checks" << std::endl;
    return true;
  }
  
  std::cerr << "Unknown command. Try 'user help' for available commands." << std::endl;
  return false;
}

bool Built_ins::help_command() {
  std::cout << "Available commands:" << std::endl;
  std::cout << " ai: Access AI command settings and chat or switch to the ai menu" << std::endl;
  std::cout << " approot: Switch to the application directory" << std::endl;
  std::cout << " user: Access user settings" << std::endl;
  std::cout << " aihelp: Get AI troubleshooting help" << std::endl;
  std::cout << " theme: Manage themes (load/save)" << std::endl;
  std::cout << " version: Display application version" << std::endl;
  std::cout << " plugin: Manage plugins" << std::endl;
  std::cout << " uninstall: Uninstall the application" << std::endl;
  std::cout << std::endl;
  
  std::cout << " Built-in g_shell commands:" << std::endl;
  std::cout << " cd [DIR]: Change directory" << std::endl;
  std::cout << " alias [NAME=VALUE]: Create command alias" << std::endl;
  std::cout << " export [NAME=VALUE]: Set environment variable" << std::endl;
  std::cout << " unset [NAME]: Unset environment variable" << std::endl;
  std::cout << " source [FILE]: Execute commands from file" << std::endl;
  std::cout << " unalias [NAME]: Remove alias" << std::endl;
  std::cout << std::endl;
  
  std::cout << " Common system commands:" << std::endl;
  std::cout << " clear: Clear the terminal screen" << std::endl;
  std::cout << " exit: Exit the application" << std::endl;
  std::cout << " quit: Exit the application" << std::endl;
  std::cout << " help: Show this help message" << std::endl;
  
  return true;
}

bool Built_ins::aihelp_command(const std::vector<std::string>& args) {
  if (!g_ai || g_ai->getAPIKey().empty()) {
    std::cerr << "Please set your OpenAI API key first." << std::endl;
    return false;
  }
  
  std::string message;
  if (args.size() > 1) {
    for (size_t i = 1; i < args.size(); ++i) {
      message += args[i] + " ";
    }
  } else {
    message = "I am encountering some issues with the cjsh g_shell and would like some help. This is the most recent output: " + g_shell -> last_terminal_output_error + " Here is the command I used: " + g_shell ->last_command;
  }
  
  if (g_debug_mode) {
    std::cout << "Sending to AI: " << message << std::endl;
  }
  
  std::cout << g_ai->forceDirectChatGPT(message, false) << std::endl;
  return true;
}

bool Built_ins::alias_command(const std::vector<std::string>& args) {
  if (args.size() == 1) {
    auto& aliases = g_shell->get_aliases();
    if (aliases.empty()) {
      std::cout << "No aliases defined." << std::endl;
    } else {
      for (const auto& [name, value] : aliases) {
        std::cout << "alias " << name << "='" << value << "'" << std::endl;
      }
    }
    return true;
  }

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
      auto it = aliases.find(args[i]);
      if (it != aliases.end()) {
        std::cout << "alias " << it->first << "='" << it->second << "'" << std::endl;
      } else {
        std::cerr << "alias: " << args[i] << ": not found" << std::endl;
      }
    }
  }

  if (g_shell) {
    g_shell->set_aliases(aliases);
  }
  
  return true;
}

bool Built_ins::export_command(const std::vector<std::string>& args) {
  if (args.size() == 1) {
    extern char **environ;
    for (char **env = environ; *env; ++env) {
      std::cout << "export " << *env << std::endl;
    }
    return true;
  }

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
        std::cerr << "export: " << args[i] << ": not found" << std::endl;
      }
    }
  }

  if (g_shell) {
    g_shell->set_env_vars(env_vars);
  }
  
  return true;
}

bool Built_ins::unalias_command(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    std::cerr << "unalias: not enough arguments" << std::endl;
    return false;
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
  
  return success;
}

bool Built_ins::unset_command(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    std::cerr << "unset: not enough arguments" << std::endl;
    return false;
  }

  bool success = true;
  
  for (size_t i = 1; i < args.size(); ++i) {
    const std::string& name = args[i];
    
    env_vars.erase(name);
    
    if (unsetenv(name.c_str()) != 0) {
      std::cerr << "unset: error unsetting " << name << ": " << strerror(errno) << std::endl;
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
  
  return success;
}

bool Built_ins::parse_assignment(const std::string& arg, std::string& name, std::string& value) {
  size_t equals_pos = arg.find('=');
  if (equals_pos == std::string::npos || equals_pos == 0) {
    return false;
  }
  
  name = arg.substr(0, equals_pos);
  value = arg.substr(equals_pos + 1);
  
  // Trim quotes if present
  if (value.size() >= 2) {
    if ((value.front() == '"' && value.back() == '"') ||
        (value.front() == '\'' && value.back() == '\'')) {
      value = value.substr(1, value.size() - 2);
    }
  }
  
  return true;
}

void Built_ins::save_alias_to_file(const std::string& name, const std::string& value) {
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
    std::cerr << "Error: Unable to open source file for writing at " << source_path.string() << std::endl;
  }
}

void Built_ins::remove_alias_from_file(const std::string& name) {
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
    std::cerr << "Error: Unable to open source file for writing at " << source_path.string() << std::endl;
  }
}

void Built_ins::save_env_var_to_file(const std::string& name, const std::string& value) {
  if (!shell || !shell->get_login_mode()) {
    if (g_debug_mode) {
      std::cerr << "Warning: Attempted to save environment variable to config file when not in login mode" << std::endl;
    }
    return;
  }
  
  std::filesystem::path config_path = cjsh_filesystem::g_cjsh_config_path;
  
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
    std::cerr << "Error: Unable to open config file for writing at " << config_path.string() << std::endl;
  }
}

void Built_ins::remove_env_var_from_file(const std::string& name) {
  if (!shell || !shell->get_login_mode()) {
    if (g_debug_mode) {
      std::cerr << "Warning: Attempted to remove environment variable from config file when not in login mode" << std::endl;
    }
    return;
  }
  
  std::filesystem::path config_path = cjsh_filesystem::g_cjsh_config_path;
  
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
    std::cerr << "Error: Unable to open config file for writing at " << config_path.string() << std::endl;
  }
}

void Built_ins::do_ai_request(const std::string& prompt) {
  if (!g_ai) {
    std::cerr << "AI system not initialized." << std::endl;
    return;
  }

  if (g_ai->getAPIKey().empty()) {
    std::cerr << "Please set your OpenAI API key first using 'ai apikey set <YOUR_API_KEY>'." << std::endl;
    return;
  }

  try {
    std::string response = g_ai->chatGPT(prompt, true);
    std::cout << response << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Error communicating with AI: " << e.what() << std::endl;
  }
}
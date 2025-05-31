#include "ai_command.h"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "built_ins.h"
#include "cjsh_filesystem.h"
#include "main.h"

#define PRINT_ERROR(MSG)      \
  do {                        \
    std::cerr << MSG << '\n'; \
  } while (0)

int ai_command(const std::vector<std::string>& args, Built_ins* built_ins) {
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
    built_ins->get_shell()->set_menu_active(false);
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
    handle_ai_file_commands(args, command_index,
                            built_ins->get_current_directory());
    return 0;
  }

  if (cmd == "directory") {
    if (args.size() <= command_index + 1) {
      std::cout << "The current directory is " << g_ai->getSaveDirectory()
                << std::endl;
      return 0;
    }

    if (args[command_index + 1] == "set") {
      g_ai->setSaveDirectory(built_ins->get_current_directory());
      std::cout << "Directory set to " << built_ins->get_current_directory()
                << std::endl;
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
      return 1;
    }
    return 0;
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

int ai_chat_commands(const std::vector<std::string>& args, int cmd_index) {
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

int handle_ai_file_commands(const std::vector<std::string>& args, int cmd_index,
                            const std::string& current_directory) {
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

int do_ai_request(const std::string& prompt) {
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

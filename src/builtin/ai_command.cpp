#include "ai_command.h"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "system_prompts.h"

int ai_command(const std::vector<std::string>& args, Built_ins* built_ins) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: ai_commands called with " << args.size()
              << " arguments" << std::endl;
    if (args.size() > 1)
      std::cerr << "DEBUG: ai subcommand: " << args[1] << std::endl;
  }

  if (!config::ai_enabled) {
    print_error({ErrorType::RUNTIME_ERROR, "ai", "AI is disabled", {}});
    return 1;
  }

  if (g_ai == nullptr) {
    print_error({ErrorType::RUNTIME_ERROR, "ai", 
                 "AI is not initialized - API configuration required", {}});
    return 1;
  }
  unsigned int command_index = 1;

  if (args.size() <= command_index) {
    std::cout << "To invoke regular commands prefix all commands with ':'"
              << std::endl;
    built_ins->get_shell()->set_menu_active(false);
    if (!g_ai->get_chat_cache().empty()) {
      std::cout << "Chat history:" << std::endl;
      for (const auto& message : g_ai->get_chat_cache()) {
        std::cout << message << std::endl;
      }
    }
    return 0;
  }

  const std::string& cmd = args[command_index];

  if (cmd == "log") {
    std::string lastChatSent = g_ai->get_last_prompt_used();
    std::string lastChatReceived = g_ai->get_last_response_received();
    std::string fileName =
        (cjsh_filesystem::g_cjsh_data_path /
         ("OpenAPI_Chat_" + std::to_string(time(nullptr)) + ".txt"))
            .string();
    std::ofstream file(fileName);
    if (!file.is_open()) {
      print_error({ErrorType::RUNTIME_ERROR, "ai", 
                   "unable to create the chat log file at " + fileName, {}});
    } else {
      file << "Chat Sent: " << lastChatSent << "\n";
      file << "Chat Received: " << lastChatReceived << "\n";
      file.close();
      std::cout << "Chat log saved to " << fileName << std::endl;
    }
    return 0;
  }

  if (cmd == "apikey") {
    std::string api_key =
        getenv("OPENAI_API_KEY") ? getenv("OPENAI_API_KEY") : "";
    if (api_key.empty()) {
      std::cout << "No OpenAI API key is set." << std::endl;
      std::cout << "To set your OpenAI API key, set the OPENAI_API_KEY "
                   "environment variable."
                << std::endl;
      return 1;
    } else {
      std::cout << "The current OpenAI API key is set." << std::endl;
      std::cout << api_key << std::endl;
      return 0;
    }
  }

  if (cmd == "chat") {
    ai_chat_commands(args, command_index);
    return 0;
  }

  if (cmd == "get") {
    if (args.size() <= command_index + 1) {
      print_error({ErrorType::INVALID_ARGUMENT, "ai", 
                   "no arguments provided. try 'help' for a list of commands", {}});
      return 1;
    }
    std::cout << g_ai->get_response_data(args[command_index + 1]) << std::endl;
    return 0;
  }

  if (cmd == "dump") {
    std::cout << g_ai->get_response_data("all") << std::endl;
    std::cout << g_ai->get_last_prompt_used() << std::endl;
    return 0;
  }

  if (cmd == "mode") {
    if (args.size() <= command_index + 1) {
      std::cout << "The current assistant mode is "
                << g_ai->get_assistant_type() << std::endl;
      return 0;
    }
    g_ai->set_assistant_type(args[command_index + 1]);
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
      std::cout << "The current directory is " << g_ai->get_save_directory()
                << std::endl;
      return 0;
    }

    if (args[command_index + 1] == "set") {
      g_ai->set_save_directory(built_ins->get_current_directory());
      std::cout << "Directory set to " << built_ins->get_current_directory()
                << std::endl;
      return 0;
    }

    if (args[command_index + 1] == "clear") {
      g_ai->set_save_directory(cjsh_filesystem::g_cjsh_data_path);
      std::cout << "Directory set to default." << std::endl;
      return 0;
    }
    print_error({ErrorType::INVALID_ARGUMENT, "ai", 
                 "invalid directory command. use 'set' or 'clear'", {}});
    return 1;
  }

  if (cmd == "initialinstruction") {
    if (args.size() <= command_index + 1) {
      std::cout << "The current initial instruction is:\n"
                << g_ai->get_initial_instruction() << std::endl;
      return 0;
    }
    std::string instruction = args[command_index + 1];
    for (unsigned int i = command_index + 2; i < args.size(); i++) {
      instruction += " " + args[i];
    }
    g_ai->set_initial_instruction(instruction);
    std::cout << "Initial instruction set to:\n"
              << g_ai->get_initial_instruction() << std::endl;
    return 0;
  }

  if (cmd == "model") {
    if (args.size() <= command_index + 1) {
      std::cout << "The current model is " << g_ai->get_model() << std::endl;
      return 0;
    }
    g_ai->set_model(args[command_index + 1]);
    std::cout << "Model set to " << args[command_index + 1] << std::endl;
    return 0;
  }

  if (cmd == "rejectchanges") {
    g_ai->reject_changes();
    std::cout << "Changes rejected." << std::endl;
    return 0;
  }

  if (cmd == "timeoutflag") {
    if (args.size() <= command_index + 1) {
      std::cout << "The current timeout flag is "
                << g_ai->get_timeout_flag_seconds() << std::endl;
      return 0;
    }

    try {
      int timeout = std::stoi(args[command_index + 1]);
      g_ai->set_timeout_flag_seconds(timeout);
      std::cout << "Timeout flag set to " << timeout << " seconds."
                << std::endl;
    } catch (const std::exception& e) {
      print_error({ErrorType::INVALID_ARGUMENT, "ai", 
                   "invalid timeout value. please provide a number", {}});
      return 1;
    }
    return 0;
  }

  if (cmd == "name") {
    if (args.size() <= command_index + 1) {
      if (g_ai->get_assistant_name().length() > 0) {
        std::cout << "The current assistant name is "
                  << g_ai->get_assistant_name() << std::endl;
      } else {
        std::cout << "No assistant name is set." << std::endl;
      }
      return 0;
    }
    std::string name = args[command_index + 1];
    for (unsigned int i = command_index + 2; i < args.size(); i++) {
      name += " " + args[i];
    }
    g_ai->set_assistant_name(name);
    std::cout << "Assistant name set to " << name << std::endl;
    return 0;
  }

  if (cmd == "saveconfig") {
    g_ai->save_ai_config();
    std::cout << "AI configuration saved." << std::endl;
    return 0;
  }

  if (cmd == "config") {
    if (args.size() <= command_index + 1) {
      std::cout << "Current AI config: " << g_ai->get_config_name()
                << std::endl;
      return 0;
    }

    const std::string& subcmd = args[command_index + 1];

    if (subcmd == "list") {
      std::vector<std::string> configs = g_ai->list_configs();
      if (configs.empty()) {
        std::cout << "No AI configs found." << std::endl;
      } else {
        std::cout << "Available AI configs:" << std::endl;
        for (const auto& config : configs) {
          if (config == g_ai->get_config_name()) {
            std::cout << "* " << config << " (current)" << std::endl;
          } else {
            std::cout << "  " << config << std::endl;
          }
        }
      }
      return 0;
    }

    if (subcmd == "switch" || subcmd == "load") {
      if (args.size() <= command_index + 2) {
        std::cout << "ai: missing config name. Usage: ai config switch <name>"
                  << std::endl;
        return 1;
      }
      std::string config_name = args[command_index + 2];
      if (g_ai->load_config(config_name)) {
        if (!g_startup_active) {
          std::cout << "Switched to AI config: " << config_name << std::endl;
        }
      } else {
        std::cout << "Failed to switch to AI config: " << config_name
                  << std::endl;
        return 1;
      }
      return 0;
    }

    if (subcmd == "save" || subcmd == "saveas") {
      if (args.size() <= command_index + 2) {
        std::cout << "ai: missing config name. Usage: ai config save <name>"
                  << std::endl;
        return 1;
      }
      std::string config_name = args[command_index + 2];
      if (g_ai->save_config_as(config_name)) {
        std::cout << "Saved AI config as: " << config_name << std::endl;
      } else {
        std::cout << "Failed to save AI config as: " << config_name
                  << std::endl;
        return 1;
      }
      return 0;
    }

    std::cout
        << "Unknown config command. Available commands: list, switch, save"
        << std::endl;
    return 1;
  }

  if (cmd == "voice") {
    if (args.size() <= command_index + 1) {
      std::cout << "The current voice is " << g_ai->get_voice_dictation_voice()
                << std::endl;
      return 0;
    }
    g_ai->set_voice_dictation_voice(args[command_index + 1]);
    std::cout << "Voice set to " << args[command_index + 1] << std::endl;
    return 0;
  }

  if (cmd == "voicedictation") {
    if (args.size() <= command_index + 1) {
      std::cout << "Voice dictation is currently "
                << (g_ai->get_voice_dictation_enabled() ? "enabled"
                                                        : "disabled")
                << std::endl;
      return 0;
    }
    if (args[command_index + 1] == "enable") {
      g_ai->set_voice_dictation_enabled(true);
      std::cout << "Voice dictation enabled." << std::endl;
      return 0;
    }
    if (args[command_index + 1] == "disable") {
      g_ai->set_voice_dictation_enabled(false);
      std::cout << "Voice dictation disabled." << std::endl;
      return 0;
    }
    print_error({ErrorType::INVALID_ARGUMENT, "ai", 
                 "invalid argument. use 'enable' or 'disable'", {}});
    return 1;
  }

  if (cmd == "voicedictationinstructions") {
    if (args.size() <= command_index + 1) {
      std::cout << "The current voice dictation instructions are:\n"
                << g_ai->get_voice_dictation_instructions() << std::endl;
      return 0;
    }
    std::string instructions = args[command_index + 1];
    for (unsigned int i = command_index + 2; i < args.size(); i++) {
      instructions += " " + args[i];
    }
    g_ai->set_voice_dictation_instructions(instructions);
    std::cout << "Voice dictation instructions set to:\n"
              << g_ai->get_voice_dictation_instructions() << std::endl;
    return 0;
  }

  if (cmd == "help") {
    std::cout
        << "AI Command Help:\n"
        << "  ai                    - Enter AI mode and show chat history\n"
        << "  ai log                - Save the last chat to a file\n"
        << "  ai apikey             - Show API key status\n"
        << "  ai chat <message>     - Send a chat message\n"
        << "  ai chat history       - Show chat history\n"
        << "  ai chat history clear - Clear chat history\n"
        << "  ai chat help          - Show chat help\n"
        << "  ai get <key>          - Get a specific response data\n"
        << "  ai dump               - Dump all response data and last prompt\n"
        << "  ai mode [type]        - Get or set the assistant mode\n"
        << "  ai file               - Manage files in AI context\n"
        << "  ai directory          - Show or set AI save directory\n"
        << "  ai model [name]       - Show or set AI model\n"
        << "  ai rejectchanges      - Reject changes from AI\n"
        << "  ai timeoutflag [sec]  - Show or set timeout in seconds\n"
        << "  ai help               - Show this help message\n"
        << "  ai initialinstruction [instruction] - Show or set the initial "
           "instruction\n"
        << "  ai name [name]        - Show or set the assistant name\n"
        << "  ai saveconfig         - Save the current AI configuration\n"
        << "  ai config             - Show current config name\n"
        << "  ai config list        - List available configs\n"
        << "  ai config switch <name> - Switch to another config\n"
        << "  ai config save <name> - Save current config with a new name\n"
        << "  ai voice [voice]      - Show or set the voice for dictation\n"
        << "  ai voicedictation [enable|disable] - Enable or disable voice "
           "dictation\n"
        << "  ai voicedictationinstructions [instructions] - Show or set voice "
           "dictation instructions\n";
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
    print_error({ErrorType::INVALID_ARGUMENT, "ai", 
                 "no arguments provided. Try 'help' for a list of commands", {}});
    return 1;
  }

  const std::string& subcmd = args[cmd_index + 1];

  if (subcmd == "history") {
    if (args.size() <= static_cast<unsigned int>(cmd_index) + 2) {
      if (!g_ai->get_chat_cache().empty()) {
        std::cout << "Chat history:" << std::endl;
        for (const auto& message : g_ai->get_chat_cache()) {
          std::cout << message << std::endl;
        }
      } else {
        std::cout << "No chat history available." << std::endl;
      }
      return 0;
    }

    if (args[cmd_index + 2] == "clear") {
      g_ai->clear_chat_cache();
      std::cout << "Chat history cleared." << std::endl;
      return 0;
    }
  }

  if (subcmd == "help") {
    std::cout << "AI Chat Help:\n"
              << "  ai chat <message>     - Send a chat message\n"
              << "  ai chat history       - Show chat history\n"
              << "  ai chat history clear - Clear chat history\n"
              << "  ai chat help          - Show this help message\n";
    return 0;
  }

  std::string message = subcmd;
  for (unsigned int i = cmd_index + 2; i < args.size(); i++) {
    message += " " + args[i];
  }

  std::cout << getenv("USER") << ": " << message << std::endl;
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
    print_error({ErrorType::RUNTIME_ERROR, "ai", 
                 "Error reading directory: " + std::string(e.what()), {}});
  }

  if (args.size() <= static_cast<unsigned int>(cmd_index) + 1) {
    std::vector<std::string> activeFiles = g_ai->get_files();
    std::cout << "Active Files: " << std::endl;
    for (const auto& file : activeFiles) {
      std::cout << file << std::endl;
    }
    std::cout << "Total characters processed: "
              << g_ai->get_file_contents().length() << std::endl;
    std::cout << "Files at current path: " << std::endl;
    for (const auto& file : filesAtPath) {
      std::cout << file << std::endl;
    }
    return 0;
  }

  const std::string& subcmd = args[cmd_index + 1];

  if (subcmd == "add") {
    if (args.size() <= static_cast<unsigned int>(cmd_index) + 2) {
      print_error({ErrorType::INVALID_ARGUMENT, "ai", 
                   "no file specified. Try 'help' for a list of commands", {}});
      return 1;
    }

    if (args[cmd_index + 2] == "all") {
      int charsProcessed = g_ai->add_files(filesAtPath);
      std::cout << "Processed " << charsProcessed << " characters from "
                << filesAtPath.size() << " files." << std::endl;
      return 0;
    }

    std::string filename = args[cmd_index + 2];
    std::string filePath = current_directory + "/" + filename;

    if (!std::filesystem::exists(filePath)) {
      print_error({ErrorType::FILE_NOT_FOUND, "ai", 
                   "file not found: " + filename, {}});
      return 1;
    }

    int charsProcessed = g_ai->add_file(filePath);
    std::cout << "Processed " << charsProcessed
              << " characters from file: " << filename << std::endl;
    return 0;
  }

  if (subcmd == "remove") {
    if (args.size() <= static_cast<unsigned int>(cmd_index) + 2) {
      print_error({ErrorType::INVALID_ARGUMENT, "ai", 
                   "no file specified. Try 'help' for a list of commands", {}});
      return 1;
    }

    if (args[cmd_index + 2] == "all") {
      int fileCount = g_ai->get_files().size();
      g_ai->clear_files();
      std::cout << "Removed all " << fileCount << " files from context."
                << std::endl;
      return 0;
    }

    std::string filename = args[cmd_index + 2];
    std::string filePath = current_directory + "/" + filename;

    if (!std::filesystem::exists(filePath)) {
      print_error({ErrorType::FILE_NOT_FOUND, "ai", 
                   "file not found: " + filename, {}});
      return 1;
    }

    g_ai->remove_file(filePath);
    std::cout << "Removed file: " << filename << " from context." << std::endl;
    return 0;
  }

  if (subcmd == "active") {
    std::vector<std::string> activeFiles = g_ai->get_files();
    std::cout << "Active Files: " << std::endl;
    if (activeFiles.empty()) {
      std::cout << "  No active files." << std::endl;
    } else {
      for (const auto& file : activeFiles) {
        std::cout << "  " << file << std::endl;
      }
      std::cout << "Total characters processed: "
                << g_ai->get_file_contents().length() << std::endl;
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
    g_ai->refresh_files();
    std::cout << "Files refreshed." << std::endl;
    return 0;
  }

  if (subcmd == "clear") {
    g_ai->clear_files();
    std::cout << "Files cleared." << std::endl;
    return 0;
  }

  print_error({ErrorType::INVALID_ARGUMENT, "ai", 
               "unknown command. try 'help' for a list of commands", {}});
  return 1;
}

int do_ai_request(const std::string& prompt) {
  try {
    std::string system_prompt = build_system_prompt();

    std::string response = g_ai->chat_gpt(system_prompt, prompt, true);
    if (g_ai->get_assistant_name().length() > 0) {
      std::cout << g_ai->get_assistant_name() << ": " << response << std::endl;
      return 0;
    }
    std::cout << g_ai->get_model() << ": " << response << std::endl;
    return 0;
  } catch (const std::exception& e) {
    return 1;
  }
}

#include "ai_command.h"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "ai.h"
#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "shell.h"
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
        initialize_ai();
    }
    unsigned int command_index = 1;

    if (args.size() <= command_index) {
        if (!g_startup_active) {
            std::cout
                << "To invoke regular commands prefix all commands with ':'"
                << std::endl;
            built_ins->get_shell()->set_menu_active(false);
            if (!g_ai->get_chat_cache().empty()) {
                std::cout << "Chat history:" << std::endl;
                for (const auto& message : g_ai->get_chat_cache()) {
                    std::cout << message << std::endl;
                }
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
            print_error({ErrorType::RUNTIME_ERROR,
                         "ai",
                         "unable to create the chat log file at " + fileName,
                         {}});
        } else {
            file << "Chat Sent: " << lastChatSent << "\n";
            file << "Chat Received: " << lastChatReceived << "\n";
            file.close();
            if (!g_startup_active) {
                std::cout << "Chat log saved to " << fileName << std::endl;
            }
        }
        return 0;
    }

    if (cmd == "apikey") {
        std::string api_key =
            getenv("OPENAI_API_KEY") ? getenv("OPENAI_API_KEY") : "";
        if (api_key.empty()) {
            if (!g_startup_active) {
                std::cout << "No OpenAI API key is set." << std::endl;
                std::cout
                    << "To set your OpenAI API key, set the OPENAI_API_KEY "
                       "environment variable."
                    << std::endl;
            }
            return 1;
        } else {
            if (!g_startup_active) {
                std::cout << "The current OpenAI API key is set." << std::endl;
                std::cout << api_key << std::endl;
            }
            return 0;
        }
    }

    if (cmd == "chat") {
        ai_chat_commands(args, command_index);
        return 0;
    }

    if (cmd == "get") {
        if (args.size() <= command_index + 1) {
            print_error(
                {ErrorType::INVALID_ARGUMENT,
                 "ai",
                 "no arguments provided. try 'help' for a list of commands",
                 {}});
            return 1;
        }
        if (!g_startup_active) {
            std::cout << g_ai->get_response_data(args[command_index + 1])
                      << std::endl;
        }
        return 0;
    }

    if (cmd == "dump") {
        if (!g_startup_active) {
            std::cout << g_ai->get_response_data("all") << std::endl;
            std::cout << g_ai->get_last_prompt_used() << std::endl;
        }
        return 0;
    }

    if (cmd == "mode") {
        if (args.size() <= command_index + 1) {
            if (!g_startup_active) {
                std::cout << "The current assistant mode is "
                          << g_ai->get_assistant_type() << std::endl;
            }
            return 0;
        }
        g_ai->set_assistant_type(args[command_index + 1]);
        if (!g_startup_active) {
            std::cout << "Assistant mode set to " << args[command_index + 1]
                      << std::endl;
        }
        return 0;
    }

    if (cmd == "file") {
        handle_ai_file_commands(args, command_index,
                                built_ins->get_current_directory());
        return 0;
    }

    if (cmd == "directory") {
        if (args.size() <= command_index + 1) {
            if (!g_startup_active) {
                std::cout << "The current directory is "
                          << g_ai->get_save_directory() << std::endl;
            }
            return 0;
        }

        if (args[command_index + 1] == "set") {
            g_ai->set_save_directory(built_ins->get_current_directory());
            if (!g_startup_active) {
                std::cout << "Directory set to "
                          << built_ins->get_current_directory() << std::endl;
            }
            return 0;
        }

        if (args[command_index + 1] == "clear") {
            g_ai->set_save_directory(cjsh_filesystem::g_cjsh_data_path);
            if (!g_startup_active) {
                std::cout << "Directory set to default." << std::endl;
            }
            return 0;
        }
        print_error({ErrorType::INVALID_ARGUMENT,
                     "ai",
                     "invalid directory command. use 'set' or 'clear'",
                     {}});
        return 1;
    }

    if (cmd == "initialinstruction") {
        if (args.size() <= command_index + 1) {
            if (!g_startup_active) {
                std::cout << "The current initial instruction is:\n"
                          << g_ai->get_initial_instruction() << std::endl;
            }
            return 0;
        }
        std::string instruction = args[command_index + 1];
        for (unsigned int i = command_index + 2; i < args.size(); i++) {
            instruction += " " + args[i];
        }
        g_ai->set_initial_instruction(instruction);
        if (!g_startup_active) {
            std::cout << "Initial instruction set to:\n"
                      << g_ai->get_initial_instruction() << std::endl;
        }
        return 0;
    }

    if (cmd == "model") {
        if (args.size() <= command_index + 1) {
            if (!g_startup_active) {
                std::cout << "The current model is " << g_ai->get_model()
                          << std::endl;
            }
            return 0;
        }
        g_ai->set_model(args[command_index + 1]);
        if (!g_startup_active) {
            std::cout << "Model set to " << args[command_index + 1]
                      << std::endl;
        }
        return 0;
    }

    if (cmd == "rejectchanges") {
        g_ai->reject_changes();
        if (!g_startup_active) {
            std::cout << "Changes rejected." << std::endl;
        }
        return 0;
    }

    if (cmd == "timeoutflag") {
        if (args.size() <= command_index + 1) {
            if (!g_startup_active) {
                std::cout << "The current timeout flag is "
                          << g_ai->get_timeout_flag_seconds() << std::endl;
            }
            return 0;
        }

        try {
            int timeout = std::stoi(args[command_index + 1]);
            g_ai->set_timeout_flag_seconds(timeout);
            if (!g_startup_active) {
                std::cout << "Timeout flag set to " << timeout << " seconds."
                          << std::endl;
            }
        } catch (const std::exception& e) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "ai",
                         "invalid timeout value. please provide a number",
                         {}});
            return 1;
        }
        return 0;
    }

    if (cmd == "name") {
        if (args.size() <= command_index + 1) {
            if (!g_startup_active) {
                if (g_ai->get_assistant_name().length() > 0) {
                    std::cout << "The current assistant name is "
                              << g_ai->get_assistant_name() << std::endl;
                } else {
                    std::cout << "No assistant name is set." << std::endl;
                }
            }
            return 0;
        }
        std::string name = args[command_index + 1];
        for (unsigned int i = command_index + 2; i < args.size(); i++) {
            name += " " + args[i];
        }
        g_ai->set_assistant_name(name);
        if (!g_startup_active) {
            std::cout << "Assistant name set to " << name << std::endl;
        }
        return 0;
    }

    if (cmd == "voice") {
        if (args.size() <= command_index + 1) {
            if (!g_startup_active) {
                std::cout << "The current voice is "
                          << g_ai->get_voice_dictation_voice() << std::endl;
            }
            return 0;
        }
        g_ai->set_voice_dictation_voice(args[command_index + 1]);
        if (!g_startup_active) {
            std::cout << "Voice set to " << args[command_index + 1]
                      << std::endl;
        }
        return 0;
    }

    if (cmd == "voicedictation") {
        if (args.size() <= command_index + 1) {
            if (!g_startup_active) {
                std::cout << "Voice dictation is currently "
                          << (g_ai->get_voice_dictation_enabled() ? "enabled"
                                                                  : "disabled")
                          << std::endl;
            }
            return 0;
        }
        if (args[command_index + 1] == "enable") {
            g_ai->set_voice_dictation_enabled(true);
            if (!g_startup_active) {
                std::cout << "Voice dictation enabled." << std::endl;
            }
            return 0;
        }
        if (args[command_index + 1] == "disable") {
            g_ai->set_voice_dictation_enabled(false);
            if (!g_startup_active) {
                std::cout << "Voice dictation disabled." << std::endl;
            }
            return 0;
        }
        print_error({ErrorType::INVALID_ARGUMENT,
                     "ai",
                     "invalid argument. use 'enable' or 'disable'",
                     {}});
        return 1;
    }

    if (cmd == "voicedictationinstructions") {
        if (args.size() <= command_index + 1) {
            if (!g_startup_active) {
                std::cout << "The current voice dictation instructions are:\n"
                          << g_ai->get_voice_dictation_instructions()
                          << std::endl;
            }
            return 0;
        }
        std::string instructions = args[command_index + 1];
        for (unsigned int i = command_index + 2; i < args.size(); i++) {
            instructions += " " + args[i];
        }
        g_ai->set_voice_dictation_instructions(instructions);
        if (!g_startup_active) {
            std::cout << "Voice dictation instructions set to:\n"
                      << g_ai->get_voice_dictation_instructions() << std::endl;
        }
        return 0;
    }

    if (cmd == "help") {
        if (!g_startup_active) {
            std::cout
                << "AI Command Help:\n"
                << "  ai                             Enter AI mode and show "
                   "chat "
                   "history\n"
                << "  ai log                         Save the last chat to a "
                   "file\n"
                << "  ai apikey                      Show API key status\n"
                << "  ai chat <message>              Send a chat message\n"
                << "  ai chat history [clear]        Show or clear chat "
                   "history\n"
                << "  ai chat help                   Show chat-specific help\n"
                << "  ai file                        List active files and "
                   "files "
                   "in "
                   "the current directory\n"
                << "  ai file add <file>|all         Add file(s) from the "
                   "current "
                   "directory\n"
                << "  ai file remove <file>|all      Remove file(s) from "
                   "context\n"
                << "  ai file active                 Show files currently in "
                   "context\n"
                << "  ai file available              List files from the "
                   "current "
                   "directory\n"
                << "  ai file refresh                Re-read active files from "
                   "disk\n"
                << "  ai file clear                  Remove all files from "
                   "context\n"
                << "  ai directory                   Show the current save "
                   "directory\n"
                << "  ai directory set               Use the present working "
                   "directory for saves\n"
                << "  ai directory clear             Reset the save directory "
                   "to "
                   "default\n"
                << "  ai get <key>                   Show a specific response "
                   "field\n"
                << "  ai dump                        Dump all response data "
                   "and "
                   "the "
                   "last prompt\n"
                << "  ai mode [type]                 Get or set the assistant "
                   "mode\n"
                << "  ai model [name]                Get or set the model\n"
                << "  ai initialinstruction [text]   Get or set the initial "
                   "system "
                   "instruction\n"
                << "  ai name [name]                 Get or set the assistant "
                   "name\n"
                << "  ai timeoutflag [sec]           Get or set the request "
                   "timeout "
                   "in seconds\n"
                << "  ai rejectchanges               Reject the most recent AI "
                   "suggested edits\n"
                << "  ai voice [voice]               Get or set the dictation "
                   "voice\n"
                << "  ai voicedictation [enable|disable]  Toggle voice "
                   "dictation\n"
                << "  ai voicedictationinstructions [text] Set dictation "
                   "instructions\n"
                << "  ai help                        Show this summary\n";
        }
        return 0;
    }

    print_error({ErrorType::INVALID_ARGUMENT,
                 "ai",
                 "invalid argument. try 'help' for a list of commands",
                 {}});
    return 1;
}

int ai_chat_commands(const std::vector<std::string>& args, int cmd_index) {
    if (args.size() <= static_cast<unsigned int>(cmd_index) + 1) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "ai",
                     "no arguments provided. Try 'help' for a list of commands",
                     {}});
        return 1;
    }

    const std::string& subcmd = args[cmd_index + 1];

    if (subcmd == "history") {
        if (args.size() <= static_cast<unsigned int>(cmd_index) + 2) {
            if (!g_startup_active) {
                if (!g_ai->get_chat_cache().empty()) {
                    std::cout << "Chat history:" << std::endl;
                    for (const auto& message : g_ai->get_chat_cache()) {
                        std::cout << message << std::endl;
                    }
                } else {
                    std::cout << "No chat history available." << std::endl;
                }
            }
            return 0;
        }

        if (args[cmd_index + 2] == "clear") {
            g_ai->clear_chat_cache();
            if (!g_startup_active) {
                std::cout << "Chat history cleared." << std::endl;
            }
            return 0;
        }
    }

    if (subcmd == "help") {
        if (!g_startup_active) {
            std::cout << "AI Chat Help:\n"
                      << "  ai chat <message>     - Send a chat message\n"
                      << "  ai chat history       - Show chat history\n"
                      << "  ai chat history clear - Clear chat history\n"
                      << "  ai chat help          - Show this help message\n";
        }
        return 0;
    }

    std::string message = subcmd;
    for (unsigned int i = cmd_index + 2; i < args.size(); i++) {
        message += " " + args[i];
    }

    if (!g_startup_active) {
        std::cout << getenv("USER") << ": " << message << std::endl;
    }
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
        print_error({ErrorType::RUNTIME_ERROR,
                     "ai",
                     "Error reading directory: " + std::string(e.what()),
                     {}});
    }

    if (args.size() <= static_cast<unsigned int>(cmd_index) + 1) {
        if (!g_startup_active) {
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
        }
        return 0;
    }

    const std::string& subcmd = args[cmd_index + 1];

    if (subcmd == "add") {
        if (args.size() <= static_cast<unsigned int>(cmd_index) + 2) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "ai",
                         "no file specified. Try 'help' for a list of commands",
                         {}});
            return 1;
        }

        if (args[cmd_index + 2] == "all") {
            int charsProcessed = g_ai->add_files(filesAtPath);
            if (!g_startup_active) {
                std::cout << "Processed " << charsProcessed
                          << " characters from " << filesAtPath.size()
                          << " files." << std::endl;
            }
            return 0;
        }

        std::string filename = args[cmd_index + 2];
        std::string filePath = current_directory + "/" + filename;

        if (!std::filesystem::exists(filePath)) {
            print_error({ErrorType::FILE_NOT_FOUND,
                         "ai",
                         "file not found: " + filename,
                         {}});
            return 1;
        }

        int charsProcessed = g_ai->add_file(filePath);
        if (!g_startup_active) {
            std::cout << "Processed " << charsProcessed
                      << " characters from file: " << filename << std::endl;
        }
        return 0;
    }

    if (subcmd == "remove") {
        if (args.size() <= static_cast<unsigned int>(cmd_index) + 2) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "ai",
                         "no file specified. Try 'help' for a list of commands",
                         {}});
            return 1;
        }

        if (args[cmd_index + 2] == "all") {
            int fileCount = g_ai->get_files().size();
            g_ai->clear_files();
            if (!g_startup_active) {
                std::cout << "Removed all " << fileCount
                          << " files from context." << std::endl;
            }
            return 0;
        }

        std::string filename = args[cmd_index + 2];
        std::string filePath = current_directory + "/" + filename;

        if (!std::filesystem::exists(filePath)) {
            print_error({ErrorType::FILE_NOT_FOUND,
                         "ai",
                         "file not found: " + filename,
                         {}});
            return 1;
        }

        g_ai->remove_file(filePath);
        if (!g_startup_active) {
            std::cout << "Removed file: " << filename << " from context."
                      << std::endl;
        }
        return 0;
    }

    if (subcmd == "active") {
        if (!g_startup_active) {
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
        }
        return 0;
    }

    if (subcmd == "available") {
        if (!g_startup_active) {
            std::cout << "Files at current path: " << std::endl;
            for (const auto& file : filesAtPath) {
                std::cout << file << std::endl;
            }
        }
        return 0;
    }

    if (subcmd == "refresh") {
        g_ai->refresh_files();
        if (!g_startup_active) {
            std::cout << "Files refreshed." << std::endl;
        }
        return 0;
    }

    if (subcmd == "clear") {
        g_ai->clear_files();
        if (!g_startup_active) {
            std::cout << "Files cleared." << std::endl;
        }
        return 0;
    }

    print_error({ErrorType::INVALID_ARGUMENT,
                 "ai",
                 "unknown command. try 'help' for a list of commands",
                 {}});
    return 1;
}

int do_ai_request(const std::string& prompt) {
    try {
        std::string system_prompt = build_system_prompt();

        std::string response = g_ai->chat_gpt(system_prompt, prompt, true);
        if (!g_startup_active) {
            if (g_ai->get_assistant_name().length() > 0) {
                std::cout << g_ai->get_assistant_name() << ": " << response
                          << std::endl;
                return 0;
            }
            std::cout << g_ai->get_model() << ": " << response << std::endl;
        }
        return 0;
    } catch (const std::exception& e) {
        return 1;
    }
}

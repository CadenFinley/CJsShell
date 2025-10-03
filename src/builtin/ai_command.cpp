#include "ai_command.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
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

namespace {

void print_ai_command_help() {
    std::cout << "AI Command Help:\n"
              << "  ai                             Enter AI mode and show chat history\n"
              << "  ai --help                      Show this summary\n"
              << "  ai help                        Show this summary\n"
              << "  ai log                         Save the last chat to a file\n"
              << "  ai apikey                      Show API key status\n"
              << "  ai chat <message>              Send a chat message\n"
              << "  ai chat history [clear]        Show or clear chat history\n"
              << "  ai file <subcommand>           Manage attached context files\n"
              << "      add <file>|all             Attach files from the current directory\n"
              << "      remove <file>|all          Remove attached files\n"
              << "      active                     List attached files\n"
              << "      available                  List files in the current directory\n"
              << "      refresh                    Re-read attached files from disk\n"
              << "      clear                      Remove all attachments\n"
              << "  ai directory [set|clear]       Show or change the save directory\n"
              << "  ai mode [type]                 Get or set assistant mode\n"
              << "  ai model [name]                Get or set the model ID\n"
              << "  ai initialinstruction [text]   Get or set the system instruction\n"
              << "  ai name [name]                 Get or set the assistant name\n"
              << "  ai timeoutflag [seconds]       Get or set the request timeout\n"
              << "  ai voice [voice]               Get or set dictation voice\n"
              << "  ai voicedictation enable|disable Toggle voice dictation\n"
              << "  ai voicedictationinstructions [text] Set dictation instructions\n"
              << "  ai get <key>                   Show a specific response field\n"
              << "  ai dump                        Dump all response data and last prompt\n"
              << "  ai rejectchanges               Reject pending AI edits\n";
}

}  // namespace

int ai_command(const std::vector<std::string>& args, Built_ins* built_ins) {
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
            std::cout << "To invoke regular commands prefix all commands with ':'" << std::endl;
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

    if (cmd == "--help" || cmd == "-h") {
        if (!g_startup_active) {
            print_ai_command_help();
        }
        return 0;
    }

    if (cmd == "log") {
        std::string lastChatSent = g_ai->get_last_prompt_used();
        std::string lastChatReceived = g_ai->get_last_response_received();
        std::string fileName = (cjsh_filesystem::g_cjsh_data_path /
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
        std::string api_key = getenv("OPENAI_API_KEY") ? getenv("OPENAI_API_KEY") : "";
        if (api_key.empty()) {
            if (!g_startup_active) {
                std::cout << "No OpenAI API key is set." << std::endl;
                std::cout << "To set your OpenAI API key, set the OPENAI_API_KEY "
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
            print_error({ErrorType::INVALID_ARGUMENT,
                         "ai",
                         "no arguments provided. try 'help' for a list of commands",
                         {}});
            return 1;
        }
        if (!g_startup_active) {
            std::cout << g_ai->get_response_data(args[command_index + 1]) << std::endl;
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
                std::cout << "The current assistant mode is " << g_ai->get_assistant_type()
                          << std::endl;
            }
            return 0;
        }

        const std::string& requested_mode = args[command_index + 1];
        if (g_ai->get_assistant_type() == requested_mode) {
            return 0;
        }

        g_ai->set_assistant_type(requested_mode);
        if (!g_startup_active) {
            std::cout << "Assistant mode set to " << requested_mode << std::endl;
        }
        return 0;
    }

    if (cmd == "file") {
        handle_ai_file_commands(args, command_index, built_ins->get_current_directory());
        return 0;
    }

    if (cmd == "directory") {
        if (args.size() <= command_index + 1) {
            if (!g_startup_active) {
                std::cout << "The current directory is " << g_ai->get_save_directory() << std::endl;
            }
            return 0;
        }

        if (args[command_index + 1] == "set") {
            std::string directory = built_ins->get_current_directory();
            if (!directory.empty() && directory.back() != '/') {
                directory += "/";
            }

            if (g_ai->get_save_directory() == directory) {
                return 0;
            }

            g_ai->set_save_directory(built_ins->get_current_directory());
            if (!g_startup_active) {
                std::cout << "Directory set to " << built_ins->get_current_directory() << std::endl;
            }
            return 0;
        }

        if (args[command_index + 1] == "clear") {
            std::string default_directory = cjsh_filesystem::g_cjsh_data_path.string();
            if (!default_directory.empty() && default_directory.back() != '/') {
                default_directory += "/";
            }

            if (g_ai->get_save_directory() == default_directory) {
                return 0;
            }

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

        if (g_ai->get_initial_instruction() == instruction) {
            return 0;
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
                std::cout << "The current model is " << g_ai->get_model() << std::endl;
            }
            return 0;
        }
        const std::string& requested_model = args[command_index + 1];
        if (g_ai->get_model() == requested_model) {
            return 0;
        }

        g_ai->set_model(requested_model);
        if (!g_startup_active) {
            std::cout << "Model set to " << requested_model << std::endl;
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
                std::cout << "The current timeout flag is " << g_ai->get_timeout_flag_seconds()
                          << std::endl;
            }
            return 0;
        }

        try {
            int timeout = std::stoi(args[command_index + 1]);
            float current_timeout = g_ai->get_timeout_flag_seconds();
            if (std::fabs(current_timeout - static_cast<float>(timeout)) <=
                std::numeric_limits<float>::epsilon()) {
                return 0;
            }

            g_ai->set_timeout_flag_seconds(timeout);
            if (!g_startup_active) {
                std::cout << "Timeout flag set to " << timeout << " seconds." << std::endl;
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
                    std::cout << "The current assistant name is " << g_ai->get_assistant_name()
                              << std::endl;
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

        if (g_ai->get_assistant_name() == name) {
            return 0;
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
                std::cout << "The current voice is " << g_ai->get_voice_dictation_voice()
                          << std::endl;
            }
            return 0;
        }
        const std::string& requested_voice = args[command_index + 1];
        if (g_ai->get_voice_dictation_voice() == requested_voice) {
            return 0;
        }

        g_ai->set_voice_dictation_voice(requested_voice);
        if (!g_startup_active) {
            std::cout << "Voice set to " << requested_voice << std::endl;
        }
        return 0;
    }

    if (cmd == "voicedictation") {
        if (args.size() <= command_index + 1) {
            if (!g_startup_active) {
                std::cout << "Voice dictation is currently "
                          << (g_ai->get_voice_dictation_enabled() ? "enabled" : "disabled")
                          << std::endl;
            }
            return 0;
        }
        if (args[command_index + 1] == "enable") {
            if (g_ai->get_voice_dictation_enabled()) {
                return 0;
            }

            g_ai->set_voice_dictation_enabled(true);
            if (!g_startup_active) {
                std::cout << "Voice dictation enabled." << std::endl;
            }
            return 0;
        }
        if (args[command_index + 1] == "disable") {
            if (!g_ai->get_voice_dictation_enabled()) {
                return 0;
            }

            g_ai->set_voice_dictation_enabled(false);
            if (!g_startup_active) {
                std::cout << "Voice dictation disabled." << std::endl;
            }
            return 0;
        }
        print_error(
            {ErrorType::INVALID_ARGUMENT, "ai", "invalid argument. use 'enable' or 'disable'", {}});
        return 1;
    }

    if (cmd == "voicedictationinstructions") {
        if (args.size() <= command_index + 1) {
            if (!g_startup_active) {
                std::cout << "The current voice dictation instructions are:\n"
                          << g_ai->get_voice_dictation_instructions() << std::endl;
            }
            return 0;
        }
        std::string instructions = args[command_index + 1];
        for (unsigned int i = command_index + 2; i < args.size(); i++) {
            instructions += " " + args[i];
        }

        if (g_ai->get_voice_dictation_instructions() == instructions) {
            return 0;
        }

        g_ai->set_voice_dictation_instructions(instructions);
        if (!g_startup_active) {
            std::cout << "Voice dictation instructions set to:\n"
                      << g_ai->get_voice_dictation_instructions() << std::endl;
        }
        return 0;
    }

    print_error({ErrorType::INVALID_ARGUMENT,
                 "ai",
                 "invalid argument. try '--help' for a list of commands",
                 {}});
    return 1;
}

int ai_chat_commands(const std::vector<std::string>& args, int cmd_index) {
    if (args.size() <= static_cast<unsigned int>(cmd_index) + 1) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "ai",
                     "no arguments provided. Try '--help' for a list of commands",
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
        for (const auto& entry : std::filesystem::directory_iterator(current_directory)) {
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
            std::cout << "Total characters processed: " << g_ai->get_file_contents().length()
                      << std::endl;
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
                std::cout << "Processed " << charsProcessed << " characters from "
                          << filesAtPath.size() << " files." << std::endl;
            }
            return 0;
        }

        std::string filename = args[cmd_index + 2];
        std::string filePath = current_directory + "/" + filename;

        if (!std::filesystem::exists(filePath)) {
            print_error({ErrorType::FILE_NOT_FOUND, "ai", "file not found: " + filename, {}});
            return 1;
        }

        int charsProcessed = g_ai->add_file(filePath);
        if (!g_startup_active) {
            std::cout << "Processed " << charsProcessed << " characters from file: " << filename
                      << std::endl;
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
                std::cout << "Removed all " << fileCount << " files from context." << std::endl;
            }
            return 0;
        }

        std::string filename = args[cmd_index + 2];
        std::string filePath = current_directory + "/" + filename;

        if (!std::filesystem::exists(filePath)) {
            print_error({ErrorType::FILE_NOT_FOUND, "ai", "file not found: " + filename, {}});
            return 1;
        }

        g_ai->remove_file(filePath);
        if (!g_startup_active) {
            std::cout << "Removed file: " << filename << " from context." << std::endl;
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
                std::cout << "Total characters processed: " << g_ai->get_file_contents().length()
                          << std::endl;
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
                std::cout << g_ai->get_assistant_name() << ": " << response << std::endl;
                return 0;
            }
            std::cout << g_ai->get_model() << ": " << response << std::endl;
        }
        return 0;
    } catch (const std::exception& e) {
        return 1;
    }
}

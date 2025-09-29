#include "aihelp_command.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "cjsh.h"
#include "error_out.h"
#include "system_prompts.h"

int aihelp_command(const std::vector<std::string>& args) {
    if (!config::ai_enabled) {
        print_error({ErrorType::RUNTIME_ERROR, "aihelp", "AI is disabled", {}});
        return 1;
    }

    if (g_ai == nullptr) {
        initialize_ai();
    }

    if (!g_ai || g_ai->get_api_key().empty()) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "aihelp",
                     "Please set your OpenAI API key first",
                     {}});
        return 1;
    }

    bool force_mode = false;
    std::string custom_prompt;
    std::string custom_model = g_ai->get_model();
    std::vector<std::string> remaining_args;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-f") {
            force_mode = true;
        } else if (args[i] == "-p" && i + 1 < args.size()) {
            custom_prompt = args[++i];
        } else if (args[i] == "-m" && i + 1 < args.size()) {
            custom_model = args[++i];
        } else {
            remaining_args.push_back(args[i]);
        }
    }

    if (!force_mode) {
        const char* status_env = getenv("?");
        if (!status_env) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "aihelp",
                         "The last executed command status is unavailable",
                         {}});
            return 0;
        }
        int status = std::atoi(status_env);
        if (status == 0) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "aihelp",
                         "The last executed command returned exitcode 0",
                         {}});
            return 0;
        }
    }

    std::string message;

    if (!custom_prompt.empty()) {
        message = custom_prompt;
    } else if (!remaining_args.empty()) {
        message = remaining_args[0];
        for (size_t i = 1; i < remaining_args.size(); ++i) {
            message += " " + remaining_args[i];
        }
    } else {
        // Enhanced error context
        message =
            "I need help fixing a shell command error. Please analyze the "
            "error and provide:\n"
            "1. What went wrong (brief explanation)\n"
            "2. Specific fix commands I can run\n"
            "3. Prevention tips for the future\n\n"
            "ERROR: " +
            g_shell->last_terminal_output_error +
            "\n"
            "COMMAND: " +
            g_shell->last_command +
            "\n"
            "DIRECTORY: " +
            std::string(getenv("PWD") ? getenv("PWD") : "unknown") +
            "\n"
            "EXIT_CODE: " +
            std::string(getenv("?") ? getenv("?") : "unknown") + "\n";

        // Add directory listing for context
        message += "CURRENT_FILES: ";
        try {
            for (const auto& entry : std::filesystem::directory_iterator(".")) {
                if (entry.is_regular_file()) {
                    message += entry.path().filename().string() + " ";
                }
            }
        } catch (...) {
            message += "(could not list files) ";
        }
        message += "\n";
    }

    if (g_debug_mode) {
        std::cout << "Sending to AI: " << message << std::endl;
        std::cout << "Using model: " << custom_model << std::endl;
    }

    std::string response = g_ai->force_direct_chat_gpt(
        message + create_help_system_prompt() + "\n" + build_system_prompt(),
        false);

    std::cout << response << std::endl;

    // Check for --fix flag to attempt auto-fix
    bool auto_fix = false;
    for (const auto& arg : args) {
        if (arg == "--fix" || arg == "-F") {
            auto_fix = true;
            break;
        }
    }

    if (auto_fix && !remaining_args.empty() && remaining_args[0] != "--fix" &&
        remaining_args[0] != "-F") {
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "AUTO-FIX ATTEMPT:\n";

        // Extract command suggestions from response
        std::string fix_prompt =
            "Based on the error analysis above, provide ONLY the exact shell "
            "command(s) to fix the issue. "
            "One command per line, no explanations, no markdown formatting.";

        std::string fix_commands =
            g_ai->force_direct_chat_gpt(fix_prompt, false);

        std::cout << "Suggested fix command(s):\n" << fix_commands << std::endl;
        std::cout << "\nRun these commands? [y/N]: ";

        char choice;
        std::cin >> choice;
        if (choice == 'y' || choice == 'Y') {
            std::cout << "Executing fix commands...\n";
            // Here you would integrate with the shell's command execution
            // For now, just show what would be executed
            std::cout << "[This would execute the commands above]\n";
        }
    }

    return 0;
}

#include <iostream>
#include <string>
#include <vector>

#include "error_out.h"

void print_error(const ErrorInfo& error) {
    std::cerr << "cjsh: ";

    if (!error.command_used.empty()) {
        std::cerr << error.command_used << ": ";
    }

    switch (error.type) {
        case ErrorType::COMMAND_NOT_FOUND:
            std::cerr << "command not found";
            break;
        case ErrorType::SYNTAX_ERROR:
            std::cerr << "syntax error";
            break;
        case ErrorType::PERMISSION_DENIED:
            std::cerr << "permission denied";
            break;
        case ErrorType::FILE_NOT_FOUND:
            std::cerr << "file not found";
            break;
        case ErrorType::INVALID_ARGUMENT:
            std::cerr << "invalid argument";
            break;
        case ErrorType::RUNTIME_ERROR:
            std::cerr << "runtime error";
            break;
        case ErrorType::UNKNOWN_ERROR:
        default:
            std::cerr << "unknown error";
            break;
    }

    if (!error.message.empty()) {
        std::cerr << ": " << error.message;
    }

    std::cerr << std::endl;

    if (!error.suggestions.empty()) {
        // Check if suggestions are command suggestions (start with "Did you
        // mean")
        std::vector<std::string> commands;
        bool has_command_suggestions = false;

        for (const auto& suggestion : error.suggestions) {
            if (suggestion.find("Did you mean '") != std::string::npos) {
                // Extract command name from "Did you mean 'command'?" format
                size_t start = suggestion.find("'") + 1;
                size_t end = suggestion.find("'", start);
                if (start != std::string::npos && end != std::string::npos &&
                    end > start) {
                    commands.push_back(suggestion.substr(start, end - start));
                    has_command_suggestions = true;
                }
            }
        }

        if (has_command_suggestions && !commands.empty()) {
            // Print command suggestions on one line
            std::cerr << "Did you mean: ";
            for (size_t i = 0; i < commands.size(); ++i) {
                std::cerr << commands[i];
                if (i < commands.size() - 1) {
                    std::cerr << ", ";
                }
            }
            std::cerr << "?" << std::endl;

            // Print any non-command suggestions on separate lines
            for (const auto& suggestion : error.suggestions) {
                if (suggestion.find("Did you mean '") == std::string::npos) {
                    std::cerr << suggestion << std::endl;
                }
            }
        } else {
            // Print all suggestions as before if they're not command
            // suggestions
            for (const auto& suggestion : error.suggestions) {
                std::cerr << suggestion << std::endl;
            }
        }
    }
}
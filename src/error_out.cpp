#include "error_out.h"
#include <iostream>
#include <string>
#include <vector>

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

    // if (!error.context.empty()) {
    //     std::cerr << "\n" << error.context;
    // }

    std::cerr << '\n';

    if (!error.suggestions.empty()) {
        std::vector<std::string> commands;
        bool has_command_suggestions = false;

        for (const auto& suggestion : error.suggestions) {
            if (suggestion.find("Did you mean '") != std::string::npos) {
                size_t start = suggestion.find('\'') + 1;
                size_t end = suggestion.find('\'', start);
                if (start != std::string::npos && end != std::string::npos && end > start) {
                    commands.push_back(suggestion.substr(start, end - start));
                    has_command_suggestions = true;
                }
            }
        }

        if (has_command_suggestions && !commands.empty()) {
            std::cerr << "Did you mean: ";
            for (size_t i = 0; i < commands.size(); ++i) {
                std::cerr << commands[i];
                if (i < commands.size() - 1) {
                    std::cerr << ", ";
                }
            }
            std::cerr << "?" << '\n';

            for (const auto& suggestion : error.suggestions) {
                if (suggestion.find("Did you mean '") == std::string::npos) {
                    std::cerr << suggestion << '\n';
                }
            }
        } else {
            for (const auto& suggestion : error.suggestions) {
                std::cerr << suggestion << '\n';
            }
        }
    }
}

ErrorInfo::ErrorInfo()
    : type(ErrorType::UNKNOWN_ERROR),
      severity(ErrorSeverity::ERROR),
      command_used(""),
      message(""),
      suggestions() {
}

ErrorInfo::ErrorInfo(ErrorType t, ErrorSeverity s, const std::string& cmd, const std::string& msg,
                     const std::vector<std::string>& sugg, const std::string& ctx)
    : type(t), severity(s), command_used(cmd), message(msg), suggestions(sugg), context(ctx) {
}

ErrorInfo::ErrorInfo(ErrorType t, const std::string& cmd, const std::string& msg,
                     const std::vector<std::string>& sugg, const std::string& ctx)
    : type(t),
      severity(get_default_severity(t)),
      command_used(cmd),
      message(msg),
      suggestions(sugg),
      context(ctx) {
}

ErrorSeverity ErrorInfo::get_default_severity(ErrorType type) {
    switch (type) {
        case ErrorType::SYNTAX_ERROR:
            return ErrorSeverity::CRITICAL;
        case ErrorType::COMMAND_NOT_FOUND:
            return ErrorSeverity::ERROR;
        case ErrorType::PERMISSION_DENIED:
            return ErrorSeverity::ERROR;
        case ErrorType::FILE_NOT_FOUND:
            return ErrorSeverity::ERROR;
        case ErrorType::INVALID_ARGUMENT:
            return ErrorSeverity::WARNING;
        case ErrorType::RUNTIME_ERROR:
            return ErrorSeverity::ERROR;
        case ErrorType::UNKNOWN_ERROR:
        default:
            return ErrorSeverity::ERROR;
    }
}
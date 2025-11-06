#include <iostream>
#include <string>
#include <vector>

#include "cjsh.h"
#include "error_out.h"
#include "shell.h"
#include "shell_script_interpreter_error_reporter.h"

namespace {

void print_error_to_stderr(const ErrorInfo& error) {
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

}  // namespace

ErrorInfo::ErrorInfo()
    : type(ErrorType::UNKNOWN_ERROR),
      severity(ErrorSeverity::ERROR),
      command_used(""),
      message(""),
      suggestions() {
}

ErrorInfo::ErrorInfo(ErrorType t, ErrorSeverity s, const std::string& cmd, const std::string& msg,
                     const std::vector<std::string>& sugg)
    : type(t), severity(s), command_used(cmd), message(msg), suggestions(sugg) {
}

ErrorInfo::ErrorInfo(ErrorType t, const std::string& cmd, const std::string& msg,
                     const std::vector<std::string>& sugg)
    : type(t),
      severity(get_default_severity(t)),
      command_used(cmd),
      message(msg),
      suggestions(sugg) {
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

void print_error(const ErrorInfo& error) {
    if (shell_script_interpreter::report_error(error)) {
        return;
    }
    print_error_to_stderr(error);
}

void print_error_fallback(const ErrorInfo& error) {
    print_error_to_stderr(error);
}

bool should_abort_on_error(const ErrorInfo& error) {
    if (error.severity == ErrorSeverity::CRITICAL) {
        return true;
    }

    if (!g_shell || !g_shell->is_errexit_enabled()) {
        return false;
    }

    if (g_shell) {
        std::string severity_threshold = g_shell->get_errexit_severity();

        ErrorSeverity threshold = ErrorSeverity::ERROR;
        if (severity_threshold == "info") {
            threshold = ErrorSeverity::INFO;
        } else if (severity_threshold == "warning") {
            threshold = ErrorSeverity::WARNING;
        } else if (severity_threshold == "critical") {
            threshold = ErrorSeverity::CRITICAL;
        }

        return error.severity >= threshold;
    }

    return error.severity >= ErrorSeverity::ERROR;
}
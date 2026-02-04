/*
  error_out.cpp

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "cjsh.h"
#include "error_out.h"
#include "shell.h"

namespace {

constexpr const char* kResetColor = "\x1b[0m";

const char* severity_to_color(ErrorSeverity severity) {
    switch (severity) {
        case ErrorSeverity::INFO:
            return "\x1b[4m\x1b[58;5;51m";
        case ErrorSeverity::WARNING:
            return "\x1b[4m\x1b[58;5;214m";
        case ErrorSeverity::ERROR:
            return "\x1b[4m\x1b[58;5;160m";
        case ErrorSeverity::CRITICAL:
            return "\x1b[4m\x1b[58;5;196m";
        default:
            return "\x1b[4m\x1b[58;5;160m";
    }
}

bool should_colorize_output() {
    if (!g_shell || !g_shell->get_interactive_mode()) {
        return false;
    }

    return isatty(STDERR_FILENO) != 0;
}

}  // namespace

ErrorInfo::ErrorInfo() : type(ErrorType::UNKNOWN_ERROR), severity(ErrorSeverity::ERROR) {
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
            return ErrorSeverity::INFO;
        case ErrorType::RUNTIME_ERROR:
            return ErrorSeverity::ERROR;
        case ErrorType::FATAL_ERROR:
            return ErrorSeverity::CRITICAL;
        case ErrorType::UNKNOWN_ERROR:
        default:
            return ErrorSeverity::ERROR;
    }
}

void print_error(const ErrorInfo& error) {
    const bool colorize_output = should_colorize_output();
    const char* color_prefix = colorize_output ? severity_to_color(error.severity) : "";
    const char* color_reset = colorize_output ? kResetColor : "";

    if (colorize_output) {
        std::cerr << color_prefix;
    }

    std::cerr << "cjsh: ";

    if (!error.command_used.empty()) {
        std::cerr << error.command_used;
        if (error.type != ErrorType::UNKNOWN_ERROR) {
            std::cerr << ": ";
        }
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
        case ErrorType::FATAL_ERROR:
            std::cerr << "fatal error";
            break;
        case ErrorType::UNKNOWN_ERROR:
        default:
            break;
    }

    if (!error.message.empty()) {
        std::cerr << ": " << error.message;
    }

    std::cerr << '\n';

    if (!error.suggestions.empty() && error.type == ErrorType::FATAL_ERROR) {
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

    if (colorize_output) {
        std::cerr << color_reset;
    }

    if (ErrorType::FATAL_ERROR == error.type) {
        bool is_interactive = false;
        if (g_shell) {
            is_interactive = g_shell->get_interactive_mode();
        }
        if (is_interactive) {
            std::cout << "press enter to exit...";
            std::cin.get();
            std::cout << "cjsh exit\n";
            std::cout.flush();
        }
        // fatal errors exit
        _exit(EXIT_FAILURE);
    }
}

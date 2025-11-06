#pragma once

#include <string>
#include <vector>

#include <cstdint>

enum class ErrorSeverity : std::uint8_t {
    INFO = 0,
    WARNING = 1,
    ERROR = 2,
    CRITICAL = 3
};

enum class ErrorType : std::uint8_t {
    COMMAND_NOT_FOUND,
    SYNTAX_ERROR,
    PERMISSION_DENIED,
    FILE_NOT_FOUND,
    INVALID_ARGUMENT,
    RUNTIME_ERROR,
    UNKNOWN_ERROR
};

struct ErrorInfo {
    ErrorType type;
    ErrorSeverity severity;
    std::string command_used = "";
    std::string message = "";
    std::vector<std::string> suggestions = {};
    std::string context = "";

    ErrorInfo();

    ErrorInfo(ErrorType t, ErrorSeverity s, const std::string& cmd = "",
              const std::string& msg = "", const std::vector<std::string>& sugg = {},
              const std::string& ctx = "");

    ErrorInfo(ErrorType t, const std::string& cmd = "", const std::string& msg = "",
              const std::vector<std::string>& sugg = {}, const std::string& ctx = "");

    static ErrorSeverity get_default_severity(ErrorType type);
};

void print_error(const ErrorInfo& error);
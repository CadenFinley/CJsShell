#pragma once

#include <string>
#include <vector>

#include <cstdint>

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
    std::string command_used;
    std::string message;
    std::vector<std::string> suggestions;
};

void print_error(const ErrorInfo& error);
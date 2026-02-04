/*
  error_out.h

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
    UNKNOWN_ERROR,
    FATAL_ERROR
};

struct ErrorInfo {
    ErrorType type;
    ErrorSeverity severity;
    std::string command_used;
    std::string message;
    std::vector<std::string> suggestions;

    ErrorInfo();

    ErrorInfo(ErrorType t, ErrorSeverity s, const std::string& cmd, const std::string& msg,
              const std::vector<std::string>& sugg);

    ErrorInfo(ErrorType t, const std::string& cmd, const std::string& msg,
              const std::vector<std::string>& sugg);

    static ErrorSeverity get_default_severity(ErrorType type);
};

void print_error(const ErrorInfo& error);

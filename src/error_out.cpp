#include <iostream>
#include <string>
#include <vector>

#include "error_out.h"

void print_error(const ErrorInfo& error) {
  std::cerr << "cjsh: ";

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

  if (!error.command_used.empty()) {
    std::cerr << ": " << error.command_used;
  }

  if (!error.message.empty()) {
    std::cerr << ": " << error.message;
  }

  std::cerr << std::endl;

  if (!error.suggestions.empty()) {
    for (const auto& suggestion : error.suggestions) {
      std::cerr << suggestion << std::endl;
    }
  }
}
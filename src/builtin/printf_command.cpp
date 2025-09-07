#include "printf_command.h"

#include <climits>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

// Parse printf format specifiers and apply them
std::string format_printf_arg(const std::string& format_spec,
                              const std::string& arg) {
  std::ostringstream result;

  if (format_spec.empty()) return arg;

  char spec = format_spec.back();

  switch (spec) {
    case 'd':
    case 'i': {
      // Integer
      long long val = 0;
      try {
        val = std::stoll(arg.empty() ? "0" : arg);
      } catch (...) {
        val = 0;
      }
      result << val;
      break;
    }
    case 'o': {
      // Octal
      unsigned long long val = 0;
      try {
        val = std::stoull(arg.empty() ? "0" : arg);
      } catch (...) {
        val = 0;
      }
      result << std::oct << val;
      break;
    }
    case 'x': {
      // Lowercase hex
      unsigned long long val = 0;
      try {
        val = std::stoull(arg.empty() ? "0" : arg);
      } catch (...) {
        val = 0;
      }
      result << std::hex << std::nouppercase << val;
      break;
    }
    case 'X': {
      // Uppercase hex
      unsigned long long val = 0;
      try {
        val = std::stoull(arg.empty() ? "0" : arg);
      } catch (...) {
        val = 0;
      }
      result << std::hex << std::uppercase << val;
      break;
    }
    case 'u': {
      // Unsigned integer
      unsigned long long val = 0;
      try {
        val = std::stoull(arg.empty() ? "0" : arg);
      } catch (...) {
        val = 0;
      }
      result << val;
      break;
    }
    case 'f':
    case 'F': {
      // Float
      double val = 0.0;
      try {
        val = std::stod(arg.empty() ? "0" : arg);
      } catch (...) {
        val = 0.0;
      }
      result << std::fixed << val;
      break;
    }
    case 'e': {
      // Scientific notation (lowercase)
      double val = 0.0;
      try {
        val = std::stod(arg.empty() ? "0" : arg);
      } catch (...) {
        val = 0.0;
      }
      result << std::scientific << std::nouppercase << val;
      break;
    }
    case 'E': {
      // Scientific notation (uppercase)
      double val = 0.0;
      try {
        val = std::stod(arg.empty() ? "0" : arg);
      } catch (...) {
        val = 0.0;
      }
      result << std::scientific << std::uppercase << val;
      break;
    }
    case 'g':
    case 'G': {
      // General format
      double val = 0.0;
      try {
        val = std::stod(arg.empty() ? "0" : arg);
      } catch (...) {
        val = 0.0;
      }
      result << val;
      break;
    }
    case 'c': {
      // Character
      if (!arg.empty()) {
        if (arg[0] >= '0' && arg[0] <= '9') {
          // Numeric value
          int val = std::atoi(arg.c_str());
          if (val >= 0 && val <= 255) {
            result << static_cast<char>(val);
          }
        } else {
          result << arg[0];
        }
      }
      break;
    }
    case 's':
    default: {
      // String (default)
      result << arg;
      break;
    }
  }

  return result.str();
}

// Process escape sequences in printf format string
std::string process_printf_escapes(const std::string& input) {
  std::string result;
  for (size_t i = 0; i < input.length(); ++i) {
    if (input[i] == '\\' && i + 1 < input.length()) {
      char next = input[i + 1];
      switch (next) {
        case 'a':
          result += '\a';
          i++;
          break;  // alert (bell)
        case 'b':
          result += '\b';
          i++;
          break;  // backspace
        case 'f':
          result += '\f';
          i++;
          break;  // form feed
        case 'n':
          result += '\n';
          i++;
          break;  // newline
        case 'r':
          result += '\r';
          i++;
          break;  // carriage return
        case 't':
          result += '\t';
          i++;
          break;  // horizontal tab
        case 'v':
          result += '\v';
          i++;
          break;  // vertical tab
        case '\\':
          result += '\\';
          i++;
          break;  // backslash
        case '"':
          result += '"';
          i++;
          break;  // quote
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7': {
          // Octal escape sequence
          int octal = 0;
          int digits = 0;
          while (i + 1 < input.length() && digits < 3 && input[i + 1] >= '0' &&
                 input[i + 1] <= '7') {
            i++;
            octal = octal * 8 + (input[i] - '0');
            digits++;
          }
          result += static_cast<char>(octal);
          break;
        }
        default:
          result += next;
          i++;
          break;
      }
    } else {
      result += input[i];
    }
  }
  return result;
}

int printf_command(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    std::cerr << "printf: missing format string\n";
    return 1;
  }

  std::string format = process_printf_escapes(args[1]);
  std::vector<std::string> printf_args;

  // Copy arguments, excluding the command name and format string
  for (size_t i = 2; i < args.size(); ++i) {
    printf_args.push_back(args[i]);
  }

  size_t arg_index = 0;
  std::string result;

  for (size_t i = 0; i < format.length(); ++i) {
    if (format[i] == '%' && i + 1 < format.length()) {
      if (format[i + 1] == '%') {
        // Literal %
        result += '%';
        i++;
      } else {
        // Parse format specifier
        size_t spec_start = i;
        i++;  // Skip %

        // Skip flags
        while (i < format.length() &&
               (format[i] == '-' || format[i] == '+' || format[i] == ' ' ||
                format[i] == '#' || format[i] == '0')) {
          i++;
        }

        // Skip width
        while (i < format.length() && format[i] >= '0' && format[i] <= '9') {
          i++;
        }

        // Skip precision
        if (i < format.length() && format[i] == '.') {
          i++;
          while (i < format.length() && format[i] >= '0' && format[i] <= '9') {
            i++;
          }
        }

        // Get conversion specifier
        if (i < format.length()) {
          std::string format_spec =
              format.substr(spec_start + 1, i - spec_start);
          std::string arg =
              (arg_index < printf_args.size()) ? printf_args[arg_index] : "";

          result += format_printf_arg(format_spec, arg);
          arg_index++;
        }
      }
    } else {
      result += format[i];
    }
  }

  std::cout << result;
  return 0;
}

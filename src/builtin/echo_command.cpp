#include "echo_command.h"

#include <iostream>
#include <string>
#include <unistd.h>

// Process escape sequences in a string
std::string process_escape_sequences(const std::string& input) {
  std::string result;
  for (size_t i = 0; i < input.length(); ++i) {
    if (input[i] == '\\' && i + 1 < input.length()) {
      char next = input[i + 1];
      switch (next) {
        case 'a': result += '\a'; i++; break;  // alert (bell)
        case 'b': result += '\b'; i++; break;  // backspace
        case 'f': result += '\f'; i++; break;  // form feed
        case 'n': result += '\n'; i++; break;  // newline
        case 'r': result += '\r'; i++; break;  // carriage return
        case 't': result += '\t'; i++; break;  // horizontal tab
        case 'v': result += '\v'; i++; break;  // vertical tab
        case '\\': result += '\\'; i++; break; // backslash
        case '0': {
          // Octal escape sequence \0nnn
          if (i + 4 < input.length() && 
              input[i + 2] >= '0' && input[i + 2] <= '7' &&
              input[i + 3] >= '0' && input[i + 3] <= '7' &&
              input[i + 4] >= '0' && input[i + 4] <= '7') {
            int octal = (input[i + 2] - '0') * 64 + 
                       (input[i + 3] - '0') * 8 + 
                       (input[i + 4] - '0');
            result += static_cast<char>(octal);
            i += 4;
          } else {
            result += input[i];
          }
          break;
        }
        default:
          result += input[i];  // Keep backslash for unknown escapes
          break;
      }
    } else {
      result += input[i];
    }
  }
  return result;
}

int echo_command(const std::vector<std::string>& args) {
  // POSIX-compliant echo implementation
  // Supports -n (no newline), -e (enable escapes), -E (disable escapes)
  
  std::vector<std::string> echo_args = args;
  bool redirect_to_stderr = false;
  bool suppress_newline = false;
  bool interpret_escapes = false;
  
  // Check if last argument is a redirection
  if (args.size() > 1 && args.back() == ">&2") {
    redirect_to_stderr = true;
    echo_args.pop_back(); // Remove >&2 from arguments
  }
  
  // Parse flags
  size_t start_idx = 1;
  while (start_idx < echo_args.size() && echo_args[start_idx][0] == '-' && echo_args[start_idx].length() > 1) {
    const std::string& flag = echo_args[start_idx];
    
    if (flag == "-n") {
      suppress_newline = true;
    } else if (flag == "-e") {
      interpret_escapes = true;
    } else if (flag == "-E") {
      interpret_escapes = false;
    } else if (flag == "--") {
      start_idx++;
      break;  // End of options
    } else {
      break;  // Unknown option, treat as regular argument
    }
    start_idx++;
  }
  
  bool first = true;
  
  for (size_t i = start_idx; i < echo_args.size(); ++i) {
    if (!first) {
      if (redirect_to_stderr) {
        std::cerr << " ";
      } else {
        std::cout << " ";
      }
    }
    
    std::string output = echo_args[i];
    if (interpret_escapes) {
      output = process_escape_sequences(output);
    }
    
    if (redirect_to_stderr) {
      std::cerr << output;
    } else {
      std::cout << output;
    }
    first = false;
  }
  
  if (!suppress_newline) {
    if (redirect_to_stderr) {
      std::cerr << "\n";
    } else {
      std::cout << "\n";
    }
  }
  
  return 0;
}

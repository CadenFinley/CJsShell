#include "read_command.h"
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include "readonly_command.h"
#include "shell.h"

int read_command(const std::vector<std::string>& args, Shell* shell) {
  if (!shell) {
    std::cerr << "read: internal error - no shell context\n";
    return 1;
  }

  // Default options
  bool raw_mode = false;
  int nchars = -1;
  std::string prompt;
  std::string delim = "\n";

  std::vector<std::string> var_names;

  // Parse arguments
  for (size_t i = 1; i < args.size(); ++i) {
    const std::string& arg = args[i];

    if (arg == "-r") {
      raw_mode = true;
    } else if (arg == "-n" && i + 1 < args.size()) {
      // -n <num> format (with space)
      try {
        nchars = std::stoi(args[i + 1]);
        i++;  // Skip the next argument
      } catch (const std::exception&) {
        std::cerr << "read: invalid number of characters: " << args[i + 1]
                  << "\n";
        return 1;
      }
    } else if (arg.substr(0, 2) == "-n" && arg.length() > 2) {
      // -n<num> format (no space)
      try {
        nchars = std::stoi(arg.substr(2));
      } catch (const std::exception&) {
        std::cerr << "read: invalid number of characters: " << arg.substr(2)
                  << "\n";
        return 1;
      }
    } else if (arg == "-p" && i + 1 < args.size()) {
      // -p <prompt> format (with space)
      prompt = args[i + 1];
      i++;  // Skip the next argument
    } else if (arg.substr(0, 2) == "-p" && arg.length() > 2) {
      // -p<prompt> format (no space)
      prompt = arg.substr(2);
    } else if (arg == "-d" && i + 1 < args.size()) {
      // -d <delim> format (with space)
      delim = args[i + 1];
      i++;  // Skip the next argument
    } else if (arg.substr(0, 2) == "-d" && arg.length() > 2) {
      // -d<delim> format (no space)
      delim = arg.substr(2);
    } else if (arg == "-t" && i + 1 < args.size()) {
      // -t <timeout> format - not implemented yet
      std::cerr << "read: timeout option not implemented\n";
      return 1;
    } else if (arg.substr(0, 2) == "-t" && arg.length() > 2) {
      // -t<timeout> format - not implemented yet
      std::cerr << "read: timeout option not implemented\n";
      return 1;
    } else if (arg == "--help") {
      std::cout << "Usage: read [-r] [-p prompt] [-n nchars] [-d delim] [-t "
                   "timeout] [name ...]\n";
      std::cout
          << "Read a line from standard input and split it into fields.\n\n";
      std::cout << "Options:\n";
      std::cout << "  -r            do not allow backslashes to escape any "
                   "characters\n";
      std::cout << "  -p prompt     output the string PROMPT without a "
                   "trailing newline before reading\n";
      std::cout << "  -n nchars     return after reading NCHARS characters "
                   "rather than waiting for a newline\n";
      std::cout << "  -d delim      continue until the first character of "
                   "DELIM is read, rather than newline\n";
      std::cout << "  -t timeout    time out and return failure if a complete "
                   "line is not read within TIMEOUT seconds\n";
      return 0;
    } else if (arg[0] == '-') {
      std::cerr << "read: invalid option -- '" << arg << "'\n";
      std::cerr << "Try 'read --help' for more information.\n";
      return 1;
    } else {
      // Variable name
      var_names.push_back(arg);
    }
  }

  // If no variable names specified, use REPLY
  if (var_names.empty()) {
    var_names.push_back("REPLY");
  }

  // Output prompt if specified
  if (!prompt.empty()) {
    std::cout << prompt << std::flush;
  }

  // Read input
  std::string input;
  char c;
  int chars_read = 0;

  if (nchars > 0) {
    // Read specific number of characters
    while (chars_read < nchars && std::cin.get(c)) {
      input += c;
      chars_read++;
    }
  } else {
    // Read until delimiter
    while (std::cin.get(c)) {
      if (delim.find(c) != std::string::npos) {
        break;
      }
      input += c;
    }
  }

  // Check for EOF
  if (std::cin.eof() && input.empty()) {
    return 1;
  }

  // Process backslashes if not in raw mode
  if (!raw_mode) {
    std::string processed;
    for (size_t i = 0; i < input.length(); ++i) {
      if (input[i] == '\\' && i + 1 < input.length()) {
        char next = input[i + 1];
        switch (next) {
          case 'n':
            processed += '\n';
            i++;
            break;
          case 't':
            processed += '\t';
            i++;
            break;
          case 'r':
            processed += '\r';
            i++;
            break;
          case 'b':
            processed += '\b';
            i++;
            break;
          case 'a':
            processed += '\a';
            i++;
            break;
          case 'v':
            processed += '\v';
            i++;
            break;
          case 'f':
            processed += '\f';
            i++;
            break;
          case '\\':
            processed += '\\';
            i++;
            break;
          default:
            processed += input[i];
            break;
        }
      } else {
        processed += input[i];
      }
    }
    input = processed;
  }

  // Split input into fields (whitespace separated)
  std::vector<std::string> fields;
  std::istringstream iss(input);
  std::string field;

  // Split by whitespace for multiple variables
  while (iss >> field) {
    fields.push_back(field);
  }

  // If no fields were read but we have input, it means the input was all
  // whitespace or the line was empty. In this case, set the first variable to
  // the entire input.
  if (fields.empty() && !input.empty()) {
    fields.push_back(input);
  }

  // Assign values to variables
  for (size_t i = 0; i < var_names.size(); ++i) {
    const std::string& var_name = var_names[i];

    // Check if variable is readonly
    if (ReadonlyManager::instance().is_readonly(var_name)) {
      std::cerr << "read: " << var_name << ": readonly variable\n";
      return 1;
    }

    std::string value;
    if (i < fields.size()) {
      if (i == var_names.size() - 1 && fields.size() > var_names.size()) {
        // Last variable gets all remaining fields
        for (size_t j = i; j < fields.size(); ++j) {
          if (j > i)
            value += " ";
          value += fields[j];
        }
      } else {
        value = fields[i];
      }
    }
    // If no more fields, variable gets empty string

    // Set the environment variable
    if (setenv(var_name.c_str(), value.c_str(), 1) != 0) {
      perror("read: setenv");
      return 1;
    }

    // Also update shell's env_vars map
    shell->get_env_vars()[var_name] = value;
  }

  return 0;
}

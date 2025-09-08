#include "test_command.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

// Built-in test command implementation
// Supports common test operations used in shell scripts
int test_command(const std::vector<std::string>& args) {
  // test command returns 0 for true, 1 for false

  // Handle empty arguments
  if (args.empty()) {
    return 1;  // false
  }

  // Handle [ command - remove the trailing ] if present
  std::vector<std::string> test_args = args;
  if (args[0] == "[" && args.size() > 1 && args.back() == "]") {
    test_args.pop_back();                // Remove trailing ]
    test_args.erase(test_args.begin());  // Remove leading [
  } else if (args[0] == "test") {
    test_args.erase(test_args.begin());  // Remove "test"
  } else if (args[0] == "[") {
    test_args.erase(test_args.begin());  // Remove "["
  }

  // Handle no arguments after [ or test
  if (test_args.empty()) {
    return 1;  // false
  }

  // Single argument - test if non-empty string
  if (test_args.size() == 1) {
    return test_args[0].empty() ? 1 : 0;
  }

  // Two arguments - unary operators
  if (test_args.size() == 2) {
    const std::string& op = test_args[0];
    const std::string& arg = test_args[1];

    if (op == "-z") {
      // String is empty
      return arg.empty() ? 0 : 1;
    } else if (op == "-n") {
      // String is non-empty
      return arg.empty() ? 1 : 0;
    } else if (op == "-f") {
      // File exists and is a regular file
      struct stat st;
      return (stat(arg.c_str(), &st) == 0 && S_ISREG(st.st_mode)) ? 0 : 1;
    } else if (op == "-d") {
      // File exists and is a directory
      struct stat st;
      return (stat(arg.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) ? 0 : 1;
    } else if (op == "-e") {
      // File exists
      return access(arg.c_str(), F_OK) == 0 ? 0 : 1;
    } else if (op == "-r") {
      // File exists and is readable
      return access(arg.c_str(), R_OK) == 0 ? 0 : 1;
    } else if (op == "-w") {
      // File exists and is writable
      return access(arg.c_str(), W_OK) == 0 ? 0 : 1;
    } else if (op == "-x") {
      // File exists and is executable
      return access(arg.c_str(), X_OK) == 0 ? 0 : 1;
    } else if (op == "-s") {
      // File exists and has size > 0
      struct stat st;
      return (stat(arg.c_str(), &st) == 0 && st.st_size > 0) ? 0 : 1;
    } else if (op == "!") {
      // Logical negation - test if argument is empty
      return arg.empty() ? 0 : 1;
    }
  }

  // Three arguments - binary operators or negation
  if (test_args.size() == 3) {
    if (test_args[0] == "!") {
      // Negation - recursively call test on the remaining arguments
      std::vector<std::string> neg_args = {"test", test_args[1], test_args[2]};
      return test_command(neg_args) == 0 ? 1 : 0;  // Invert result
    }

    const std::string& arg1 = test_args[0];
    const std::string& op = test_args[1];
    const std::string& arg2 = test_args[2];

    if (op == "=") {
      // String equality
      return arg1 == arg2 ? 0 : 1;
    } else if (op == "!=") {
      // String inequality
      return arg1 != arg2 ? 0 : 1;
    } else if (op == "-eq") {
      // Numeric equality
      try {
        int n1 = std::stoi(arg1);
        int n2 = std::stoi(arg2);
        return n1 == n2 ? 0 : 1;
      } catch (...) {
        return 1;  // false if not numbers
      }
    } else if (op == "-ne") {
      // Numeric inequality
      try {
        int n1 = std::stoi(arg1);
        int n2 = std::stoi(arg2);
        return n1 != n2 ? 0 : 1;
      } catch (...) {
        return 1;  // false if not numbers
      }
    } else if (op == "-lt") {
      // Numeric less than
      try {
        int n1 = std::stoi(arg1);
        int n2 = std::stoi(arg2);
        return n1 < n2 ? 0 : 1;
      } catch (...) {
        return 1;  // false if not numbers
      }
    } else if (op == "-le") {
      // Numeric less than or equal
      try {
        int n1 = std::stoi(arg1);
        int n2 = std::stoi(arg2);
        return n1 <= n2 ? 0 : 1;
      } catch (...) {
        return 1;  // false if not numbers
      }
    } else if (op == "-gt") {
      // Numeric greater than
      try {
        int n1 = std::stoi(arg1);
        int n2 = std::stoi(arg2);
        return n1 > n2 ? 0 : 1;
      } catch (...) {
        return 1;  // false if not numbers
      }
    } else if (op == "-ge") {
      // Numeric greater than or equal
      try {
        int n1 = std::stoi(arg1);
        int n2 = std::stoi(arg2);
        return n1 >= n2 ? 0 : 1;
      } catch (...) {
        return 1;  // false if not numbers
      }
    }
  }

  // Four arguments - negation with binary operator
  if (test_args.size() == 4 && test_args[0] == "!") {
    // Negation - recursively call test on the remaining arguments
    std::vector<std::string> neg_args = {"test", test_args[1], test_args[2],
                                         test_args[3]};
    return test_command(neg_args) == 0 ? 1 : 0;  // Invert result
  }

  // Unsupported or invalid syntax
  return 1;  // false
}

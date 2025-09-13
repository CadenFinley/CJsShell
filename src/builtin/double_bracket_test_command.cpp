#include "double_bracket_test_command.h"

#include <fnmatch.h>
#include <regex>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

#include "test_command.h"

// Helper function to perform pattern matching using fnmatch
bool pattern_match(const std::string& string, const std::string& pattern) {
  return fnmatch(pattern.c_str(), string.c_str(), 0) == 0;
}

// Helper function to perform regex matching
bool regex_match(const std::string& string, const std::string& pattern) {
  try {
    std::regex regex_pattern(pattern);
    return std::regex_search(string, regex_pattern);
  } catch (const std::regex_error&) {
    return false;
  }
}

// Helper function to evaluate a single condition
int evaluate_condition(const std::vector<std::string>& condition) {
  if (condition.empty()) {
    return 1;  // false
  }

  // Single argument - test if non-empty string
  if (condition.size() == 1) {
    return condition[0].empty() ? 1 : 0;
  }

  // Two arguments - unary operators (same as single bracket test)
  if (condition.size() == 2) {
    const std::string& op = condition[0];
    const std::string& arg = condition[1];

    if (op == "-z") {
      return arg.empty() ? 0 : 1;
    } else if (op == "-n") {
      return arg.empty() ? 1 : 0;
    } else if (op == "-f") {
      struct stat st;
      return (stat(arg.c_str(), &st) == 0 && S_ISREG(st.st_mode)) ? 0 : 1;
    } else if (op == "-d") {
      struct stat st;
      return (stat(arg.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) ? 0 : 1;
    } else if (op == "-e") {
      return access(arg.c_str(), F_OK) == 0 ? 0 : 1;
    } else if (op == "-r") {
      return access(arg.c_str(), R_OK) == 0 ? 0 : 1;
    } else if (op == "-w") {
      return access(arg.c_str(), W_OK) == 0 ? 0 : 1;
    } else if (op == "-x") {
      return access(arg.c_str(), X_OK) == 0 ? 0 : 1;
    } else if (op == "-s") {
      struct stat st;
      return (stat(arg.c_str(), &st) == 0 && st.st_size > 0) ? 0 : 1;
    } else if (op == "!") {
      return arg.empty() ? 0 : 1;
    }
  }

  // Three arguments - binary operators
  if (condition.size() == 3) {
    if (condition[0] == "!") {
      // Negation - recursively evaluate the remaining arguments
      std::vector<std::string> neg_condition = {condition[1], condition[2]};
      return evaluate_condition(neg_condition) == 0 ? 1 : 0;
    }

    const std::string& arg1 = condition[0];
    const std::string& op = condition[1];
    const std::string& arg2 = condition[2];

    if (op == "=" || op == "==") {
      // String equality (== is bash extension)
      return arg1 == arg2 ? 0 : 1;
    } else if (op == "!=") {
      // Pattern matching with != in double brackets
      return !pattern_match(arg1, arg2) ? 0 : 1;
    } else if (op == "=~") {
      // Regular expression matching (bash extension)
      return regex_match(arg1, arg2) ? 0 : 1;
    } else if (op == "-eq") {
      try {
        int n1 = std::stoi(arg1);
        int n2 = std::stoi(arg2);
        return n1 == n2 ? 0 : 1;
      } catch (...) {
        return 1;
      }
    } else if (op == "-ne") {
      try {
        int n1 = std::stoi(arg1);
        int n2 = std::stoi(arg2);
        return n1 != n2 ? 0 : 1;
      } catch (...) {
        return 1;
      }
    } else if (op == "-lt") {
      try {
        int n1 = std::stoi(arg1);
        int n2 = std::stoi(arg2);
        return n1 < n2 ? 0 : 1;
      } catch (...) {
        return 1;
      }
    } else if (op == "-le") {
      try {
        int n1 = std::stoi(arg1);
        int n2 = std::stoi(arg2);
        return n1 <= n2 ? 0 : 1;
      } catch (...) {
        return 1;
      }
    } else if (op == "-gt") {
      try {
        int n1 = std::stoi(arg1);
        int n2 = std::stoi(arg2);
        return n1 > n2 ? 0 : 1;
      } catch (...) {
        return 1;
      }
    } else if (op == "-ge") {
      try {
        int n1 = std::stoi(arg1);
        int n2 = std::stoi(arg2);
        return n1 >= n2 ? 0 : 1;
      } catch (...) {
        return 1;
      }
    }

    // Handle pattern matching with == in double brackets
    if (op == "==") {
      return pattern_match(arg1, arg2) ? 0 : 1;
    }
  }

  // Four arguments - negation with binary operator
  if (condition.size() == 4 && condition[0] == "!") {
    std::vector<std::string> neg_condition = {condition[1], condition[2],
                                              condition[3]};
    return evaluate_condition(neg_condition) == 0 ? 1 : 0;
  }

  // Unsupported or invalid syntax
  return 1;  // false
}

// Parse and evaluate logical expressions with && and ||
int parse_logical_expression(const std::vector<std::string>& args) {
  if (args.empty()) {
    return 1;  // false
  }

  // Find logical operators && and ||
  std::vector<std::vector<std::string>> conditions;
  std::vector<std::string> operators;
  std::vector<std::string> current_condition;

  for (const auto& arg : args) {
    if (arg == "&&" || arg == "||") {
      if (!current_condition.empty()) {
        conditions.push_back(current_condition);
        operators.push_back(arg);
        current_condition.clear();
      }
    } else {
      current_condition.push_back(arg);
    }
  }

  // Add the last condition
  if (!current_condition.empty()) {
    conditions.push_back(current_condition);
  }

  // If no logical operators, evaluate as single condition
  if (operators.empty()) {
    return evaluate_condition(args);
  }

  // Evaluate conditions with logical operators
  int result = evaluate_condition(conditions[0]);

  for (size_t i = 0; i < operators.size(); ++i) {
    if (operators[i] == "&&") {
      if (result != 0) {
        // Short-circuit: if left side is false, whole expression is false
        return 1;
      }
      result = evaluate_condition(conditions[i + 1]);
    } else if (operators[i] == "||") {
      if (result == 0) {
        // Short-circuit: if left side is true, whole expression is true
        return 0;
      }
      result = evaluate_condition(conditions[i + 1]);
    }
  }

  return result;
}

// Double bracket test command implementation [[ ]]
int double_bracket_test_command(const std::vector<std::string>& args) {
  // [[ ]] command returns 0 for true, 1 for false

  if (args.empty()) {
    return 1;  // false
  }

  // Remove [[ and ]] if present
  std::vector<std::string> test_args = args;
  if (args.size() >= 2 && args[0] == "[[" && args.back() == "]]") {
    test_args.pop_back();                // Remove trailing ]]
    test_args.erase(test_args.begin());  // Remove leading [[
  } else if (args[0] == "[[") {
    test_args.erase(test_args.begin());  // Remove [[
  }

  // Handle empty arguments after removing brackets
  if (test_args.empty()) {
    return 1;  // false
  }

  // Parse and evaluate the logical expression
  return parse_logical_expression(test_args);
}
#include "double_bracket_command.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <regex>
#include <sstream>

#include "cjsh.h"  // For g_debug_mode

// Helper function to check if a string matches a shell pattern
bool pattern_match(const std::string& text, const std::string& pattern) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: pattern_match called with text='" << text
              << "' (len=" << text.size() << ") pattern='" << pattern
              << "' (len=" << pattern.size() << ")" << std::endl;
    std::cerr << "DEBUG: text chars: ";
    for (size_t k = 0; k < text.size(); ++k) {
      std::cerr << "[" << k << "]=" << (int)(unsigned char)text[k] << " ";
    }
    std::cerr << std::endl;
    std::cerr << "DEBUG: pattern chars: ";
    for (size_t k = 0; k < pattern.size(); ++k) {
      std::cerr << "[" << k << "]=" << (int)(unsigned char)pattern[k] << " ";
    }
    std::cerr << std::endl;
  }
  // Convert shell pattern to regex
  std::string regex_pattern;
  for (size_t i = 0; i < pattern.size(); ++i) {
    char c = pattern[i];
    if (g_debug_mode) {
      std::cerr << "DEBUG: pattern_match processing char[" << i
                << "]=" << (int)(unsigned char)c << std::endl;
    }
    switch (c) {
      case '*':
        regex_pattern += ".*";
        break;
      case '?':
        regex_pattern += ".";
        break;
      case '[': {
        // Handle character classes properly
        regex_pattern += "[";
        size_t j = i + 1;
        while (j < pattern.size() && pattern[j] != ']') {
          regex_pattern += pattern[j];
          j++;
        }
        if (j < pattern.size()) {
          regex_pattern += "]";
          i = j;
        } else {
          // Unclosed bracket, treat as literal
          regex_pattern += "\\[";
        }
        break;
      }
      case '\\':
        if (g_debug_mode) {
          std::cerr << "DEBUG: Handling backslash at pos " << i
                    << ", next char at pos " << (i + 1) << std::endl;
        }
        if (i + 1 < pattern.size()) {
          if (g_debug_mode) {
            std::cerr << "DEBUG: Next char after backslash: "
                      << (int)(unsigned char)pattern[i + 1] << std::endl;
          }
          // Escape both the backslash and the following character
          regex_pattern +=
              "\\\\";  // This creates \\ in the regex (literal backslash)

          // Now handle the character after the backslash - escape it if it's a
          // regex special char
          char next_char = pattern[i + 1];
          if (next_char == '.' || next_char == '^' || next_char == '$' ||
              next_char == '*' || next_char == '+' || next_char == '?' ||
              next_char == '(' || next_char == ')' || next_char == '[' ||
              next_char == ']' || next_char == '{' || next_char == '}' ||
              next_char == '|' || next_char == '\\') {
            regex_pattern += "\\";
          }
          regex_pattern += next_char;
          i++;
        } else {
          regex_pattern += "\\\\";
        }
        if (g_debug_mode) {
          std::cerr << "DEBUG: After backslash processing, regex_pattern='"
                    << regex_pattern << "', next i=" << (i + 1) << std::endl;
        }
        break;
      case '.':
      case '^':
      case '$':
      case '+':
      case '(':
      case ')':
      case '{':
      case '}':
      case '|':
        regex_pattern += "\\";
        regex_pattern += c;
        break;
      default:
        regex_pattern += c;
        break;
    }
  }

  try {
    std::regex re(regex_pattern);
    if (g_debug_mode) {
      std::cerr << "DEBUG: pattern_match: text='" << text << "' pattern='"
                << pattern << "' regex='" << regex_pattern << "'" << std::endl;
    }
    return std::regex_match(text, re);
  } catch (const std::regex_error&) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: pattern_match: regex error with pattern='"
                << regex_pattern << "'" << std::endl;
    }
    return false;
  }
}

// Helper function to evaluate a single expression
int evaluate_expression(const std::vector<std::string>& tokens) {
  if (tokens.empty()) {
    return 1;
  }

  if (tokens.size() == 1) {
    // Single argument: true if non-empty
    return tokens[0].empty() ? 1 : 0;
  }

  if (tokens.size() == 2) {
    const std::string& op = tokens[0];
    const std::string& arg = tokens[1];

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

  if (tokens.size() == 3) {
    if (tokens[0] == "!") {
      // Negation
      std::vector<std::string> sub_tokens(tokens.begin() + 1, tokens.end());
      return evaluate_expression(sub_tokens) == 0 ? 1 : 0;
    }

    const std::string& arg1 = tokens[0];
    const std::string& op = tokens[1];
    const std::string& arg2 = tokens[2];

    if (op == "=" || op == "==") {
      // Enhanced string comparison with pattern matching
      if (g_debug_mode) {
        std::cerr << "DEBUG: Comparing '" << arg1 << "' == '" << arg2 << "'"
                  << std::endl;
      }
      return pattern_match(arg1, arg2) ? 0 : 1;
    } else if (op == "!=") {
      return pattern_match(arg1, arg2) ? 1 : 0;
    } else if (op == "=~") {
      // Regular expression matching
      try {
        std::regex re(arg2);
        return std::regex_search(arg1, re) ? 0 : 1;
      } catch (const std::regex_error&) {
        return 1;
      }
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
  }

  if (tokens.size() == 4 && tokens[0] == "!") {
    // Negation with 3-argument expression
    std::vector<std::string> sub_tokens(tokens.begin() + 1, tokens.end());
    return evaluate_expression(sub_tokens) == 0 ? 1 : 0;
  }

  return 1;
}

int double_bracket_command(const std::vector<std::string>& args) {
  if (args.empty()) {
    return 1;
  }

  // Debug: print all arguments
  // std::cerr << "DEBUG [[ args: ";
  // for (const auto& arg : args) {
  //   std::cerr << "'" << arg << "' ";
  // }
  // std::cerr << std::endl;

  std::vector<std::string> expression_args = args;

  // Remove [[ and ]] tokens
  if (args[0] == "[[" && args.size() > 1 && args.back() == "]]") {
    expression_args.pop_back();                      // Remove ]]
    expression_args.erase(expression_args.begin());  // Remove [[
  } else if (args[0] == "[[") {
    expression_args.erase(expression_args.begin());  // Remove [[
  }

  if (expression_args.empty()) {
    return 1;
  }

  // Handle logical operators (&& and ||)
  std::vector<std::vector<std::string>> expressions;
  std::vector<std::string> operators;
  std::vector<std::string> current_expr;

  for (size_t i = 0; i < expression_args.size(); ++i) {
    const std::string& token = expression_args[i];

    if (token == "&&" || token == "||") {
      if (!current_expr.empty()) {
        expressions.push_back(current_expr);
        current_expr.clear();
      }
      operators.push_back(token);
    } else {
      current_expr.push_back(token);
    }
  }

  if (!current_expr.empty()) {
    expressions.push_back(current_expr);
  }

  if (expressions.empty()) {
    return 1;
  }

  // Evaluate expressions with short-circuit logic
  int result = evaluate_expression(expressions[0]);

  for (size_t i = 0; i < operators.size() && i + 1 < expressions.size(); ++i) {
    if (operators[i] == "&&") {
      if (result == 0) {  // Previous expression was true
        result = evaluate_expression(expressions[i + 1]);
      }
      // If previous was false, short-circuit (result stays non-zero)
    } else if (operators[i] == "||") {
      if (result != 0) {  // Previous expression was false
        result = evaluate_expression(expressions[i + 1]);
      }
      // If previous was true, short-circuit (result stays 0)
    }
  }

  return result;
}
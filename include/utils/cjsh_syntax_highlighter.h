#pragma once

#include <string>
#include <unordered_set>

#include "isocline/isocline.h"

class SyntaxHighlighter {
 public:
  static void initialize();

  static void highlight(ic_highlight_env_t* henv, const char* input, void* arg);

 private:
  static const std::unordered_set<std::string> basic_unix_commands_;
  static std::unordered_set<std::string> external_executables_;
  static const std::unordered_set<std::string> command_operators_;
  static const std::unordered_set<std::string> shell_keywords_;
  static const std::unordered_set<std::string> shell_built_ins_;

  static bool is_shell_keyword(const std::string& token);
  static bool is_shell_builtin(const std::string& token);
  static bool is_variable_reference(const std::string& token);
  static bool is_quoted_string(const std::string& token, char& quote_type);
  static bool is_redirection_operator(const std::string& token);
  static bool is_glob_pattern(const std::string& token);
  static void highlight_quotes_and_variables(ic_highlight_env_t* henv,
                                             const char* input, size_t start,
                                             size_t length);
};

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "cjsh.h"

class Shell;

enum class DebugLevel {
  NONE = 0,
  BASIC = 1,
  VERBOSE = 2,
  TRACE = 3
};

class ShellScriptInterpreter {
 public:
  ShellScriptInterpreter();
  ~ShellScriptInterpreter();

  void set_debug_level(DebugLevel level);
  DebugLevel get_debug_level() const;

  // Provide parser dependency explicitly to avoid relying on global g_shell
  void set_parser(Parser* parser) {
    this->shell_parser = parser;
  }

  int execute_block(const std::vector<std::string>& lines);
  std::vector<std::string> parse_into_lines(const std::string& script) {
    return shell_parser->parse_into_lines(script);
  }

  // Syntax validation functions
  struct SyntaxError {
    size_t line_number;
    std::string message;
    std::string line_content;
  };
  
  std::vector<SyntaxError> validate_script_syntax(const std::vector<std::string>& lines);
  bool has_syntax_errors(const std::vector<std::string>& lines, bool print_errors = true);

 private:
  DebugLevel debug_level;
  Parser* shell_parser = nullptr;
  // Simple function registry: name -> body lines
  std::unordered_map<std::string, std::vector<std::string>> functions;

  // Parameter expansion helper
  std::string expand_parameter_expression(const std::string& param_expr);
  std::string get_variable_value(const std::string& var_name);
  bool variable_is_set(const std::string& var_name);
  std::string pattern_match_prefix(const std::string& value,
                                   const std::string& pattern,
                                   bool longest = false);
  std::string pattern_match_suffix(const std::string& value,
                                   const std::string& pattern,
                                   bool longest = false);
  bool matches_pattern(const std::string& text, const std::string& pattern);
};

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
  size_t get_terminal_width() const;
  void set_parser(Parser* parser) {
    this->shell_parser = parser;
  }

  int execute_block(const std::vector<std::string>& lines);
  std::vector<std::string> parse_into_lines(const std::string& script) {
    return shell_parser->parse_into_lines(script);
  }

  enum class ErrorSeverity {
    INFO = 0,     // Informational messages
    WARNING = 1,  // Potential issues that won't break execution
    ERROR = 2,    // Syntax errors that will prevent execution
    CRITICAL = 3  // Critical errors that indicate major problems
  };

  enum class ErrorCategory {
    SYNTAX,        // Basic syntax errors (quotes, parentheses, etc.)
    CONTROL_FLOW,  // Control structure issues (if/then/fi, loops, etc.)
    REDIRECTION,   // Input/output redirection problems
    VARIABLES,     // Variable usage and definition issues
    COMMANDS,      // Command existence and parameter issues
    SEMANTICS,     // Semantic analysis issues
    STYLE,         // Style and best practice recommendations
    PERFORMANCE    // Performance-related suggestions
  };

  struct ErrorPosition {
    size_t line_number;
    size_t column_start;  // 0-based column where error starts
    size_t column_end;    // 0-based column where error ends
    size_t char_offset;   // Absolute character offset in the script
  };

  struct SyntaxError {
    ErrorPosition position;
    ErrorSeverity severity;
    ErrorCategory category;
    std::string
        error_code;       // Unique error identifier (e.g., "SH001", "VAR002")
    std::string message;  // Human-readable error message
    std::string line_content;  // The problematic line content
    std::string suggestion;    // Suggested fix or improvement
    std::vector<std::string>
        related_info;               // Additional context or related errors
    std::string documentation_url;  // Link to documentation or help

    SyntaxError(size_t line_num, const std::string& msg,
                const std::string& line_content)
        : position({line_num, 0, 0, 0}),
          severity(ErrorSeverity::ERROR),
          category(ErrorCategory::SYNTAX),
          error_code("SYN001"),
          message(msg),
          line_content(line_content) {
    }
    SyntaxError(ErrorPosition pos, ErrorSeverity sev, ErrorCategory cat,
                const std::string& code, const std::string& msg,
                const std::string& line_content = "",
                const std::string& suggestion = "")
        : position(pos),
          severity(sev),
          category(cat),
          error_code(code),
          message(msg),
          line_content(line_content),
          suggestion(suggestion) {
    }
  };

  std::vector<SyntaxError> validate_script_syntax(
      const std::vector<std::string>& lines);
  bool has_syntax_errors(const std::vector<std::string>& lines,
                         bool print_errors = true);

  std::vector<SyntaxError> validate_comprehensive_syntax(
      const std::vector<std::string>& lines, bool check_semantics = true,
      bool check_style = false, bool check_performance = false);

  std::vector<SyntaxError> validate_variable_usage(
      const std::vector<std::string>& lines);

  std::vector<SyntaxError> validate_command_existence(
      const std::vector<std::string>& lines);

  std::vector<SyntaxError> validate_redirection_syntax(
      const std::vector<std::string>& lines);

  std::vector<SyntaxError> validate_arithmetic_expressions(
      const std::vector<std::string>& lines);

  std::vector<SyntaxError> validate_parameter_expansions(
      const std::vector<std::string>& lines);

  std::vector<SyntaxError> analyze_control_flow(
      const std::vector<std::string>& lines);

  std::vector<SyntaxError> check_style_guidelines(
      const std::vector<std::string>& lines);

  void print_error_report(const std::vector<SyntaxError>& errors,
                          bool show_suggestions = true,
                          bool show_context = true) const;

  // Function management methods
  bool has_function(const std::string& name) const;
  std::vector<std::string> get_function_names() const;

 private:
  DebugLevel debug_level;
  Parser* shell_parser = nullptr;
  std::unordered_map<std::string, std::vector<std::string>> functions;

  std::string expand_parameter_expression(const std::string& param_expr);
  std::string get_variable_value(const std::string& var_name);
  bool variable_is_set(const std::string& var_name);
  std::string pattern_match_prefix(const std::string& value,
                                   const std::string& pattern,
                                   bool longest = false);
  std::string pattern_match_suffix(const std::string& value,
                                   const std::string& pattern,
                                   bool longest = false);
  std::string pattern_substitute(const std::string& value,
                                 const std::string& replacement_expr,
                                 bool global = false);
  std::string case_convert(const std::string& value, const std::string& pattern,
                           bool uppercase, bool all_chars);
  bool matches_pattern(const std::string& text, const std::string& pattern);
};

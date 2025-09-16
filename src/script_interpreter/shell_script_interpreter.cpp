#include "shell_script_interpreter.h"

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include "cjsh.h"
#include "error_out.h"
#include "job_control.h"
#include "readonly_command.h"
#include "shell.h"

ShellScriptInterpreter::ShellScriptInterpreter() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing ShellScriptInterpreter" << std::endl;
  debug_level = DebugLevel::NONE;

  shell_parser = nullptr;
}

ShellScriptInterpreter::~ShellScriptInterpreter() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Destroying ShellScriptInterpreter" << std::endl;
}

size_t ShellScriptInterpreter::get_terminal_width() const {
  struct winsize w;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
    return w.ws_col;
  }
  return 80;
}

void ShellScriptInterpreter::set_debug_level(DebugLevel level) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Setting script interpreter debug level to "
              << static_cast<int>(level) << std::endl;
  debug_level = level;
}

DebugLevel ShellScriptInterpreter::get_debug_level() const {
  return debug_level;
}

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::validate_script_syntax(
    const std::vector<std::string>& lines) {
  std::vector<SyntaxError> errors;

  auto trim = [](const std::string& s) -> std::string {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
      return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
  };

  auto strip_inline_comment = [](const std::string& s) -> std::string {
    bool in_quotes = false;
    char quote = '\0';
    bool escaped = false;

    for (size_t i = 0; i < s.size(); ++i) {
      char c = s[i];

      if (escaped) {
        escaped = false;
        continue;
      }

      if (c == '\\' && (!in_quotes || quote != '\'')) {
        escaped = true;
        continue;
      }

      if ((c == '"' || c == '\'') && !in_quotes) {
        in_quotes = true;
        quote = c;
      } else if (c == quote && in_quotes) {
        in_quotes = false;
        quote = '\0';
      } else if (!in_quotes && c == '#') {
        return s.substr(0, i);
      }
    }
    return s;
  };

  std::vector<std::pair<std::string, size_t>> control_stack;

  for (size_t line_num = 0; line_num < lines.size(); ++line_num) {
    const std::string& line = lines[line_num];
    size_t display_line = line_num + 1;

    std::string trimmed = line;
    size_t first_non_space = trimmed.find_first_not_of(" \t");
    if (first_non_space == std::string::npos ||
        trimmed[first_non_space] == '#') {
      continue;
    }
    trimmed = trimmed.substr(first_non_space);

    std::string line_without_comments = strip_inline_comment(line);
    bool in_quotes = false;
    char quote_char = '\0';
    bool escaped = false;

    for (size_t i = 0; i < line_without_comments.length(); ++i) {
      char c = line_without_comments[i];

      if (escaped) {
        escaped = false;
        continue;
      }

      if (c == '\\' && (!in_quotes || quote_char != '\'')) {
        escaped = true;
        continue;
      }

      if ((c == '"' || c == '\'') && !in_quotes) {
        in_quotes = true;
        quote_char = c;
      } else if (c == quote_char && in_quotes) {
        in_quotes = false;
        quote_char = '\0';
      }
    }

    if (in_quotes) {
      errors.push_back(
          {display_line,
           "Unclosed quote: missing closing " + std::string(1, quote_char),
           line});
    }

    int paren_balance = 0;
    in_quotes = false;
    quote_char = '\0';
    escaped = false;
    bool in_case_block = false;

    for (const auto& stack_item : control_stack) {
      if (stack_item.first == "case") {
        in_case_block = true;
        break;
      }
    }

    bool line_has_case = trimmed.find("case ") != std::string::npos &&
                         trimmed.find(" in ") != std::string::npos;

    bool looks_like_case_pattern = (in_case_block || line_has_case) &&
                                   (trimmed.find(")") != std::string::npos);

    if (in_case_block && trimmed.find(")") != std::string::npos) {
      size_t paren_pos = trimmed.find(")");
      if (paren_pos != std::string::npos) {
        std::string before_paren = trimmed.substr(0, paren_pos);

        before_paren = trim(before_paren);
        if (!before_paren.empty() &&
            (before_paren.front() == '"' || before_paren.front() == '\'' ||
             before_paren == "*" || isalnum(before_paren.front()))) {
          looks_like_case_pattern = true;
        }
      }
    }

    if (!looks_like_case_pattern) {
      for (size_t i = 0; i < line_without_comments.length(); ++i) {
        char c = line_without_comments[i];

        if (escaped) {
          escaped = false;
          continue;
        }

        if (c == '\\' && (!in_quotes || quote_char != '\'')) {
          escaped = true;
          continue;
        }

        if ((c == '"' || c == '\'') && !in_quotes) {
          in_quotes = true;
          quote_char = c;
        } else if (c == quote_char && in_quotes) {
          in_quotes = false;
          quote_char = '\0';
        }

        if (!in_quotes) {
          if (c == '(')
            paren_balance++;
          else if (c == ')')
            paren_balance--;
        }
      }

      if (paren_balance != 0) {
        if (paren_balance > 0) {
          errors.push_back(
              {display_line, "Unmatched opening parenthesis", line});
        } else {
          errors.push_back(
              {display_line, "Unmatched closing parenthesis", line});
        }
      }
    }

    std::string trimmed_for_parsing = strip_inline_comment(trimmed);

    if (!trimmed_for_parsing.empty() && trimmed_for_parsing.back() == ';') {
      trimmed_for_parsing.pop_back();
      trimmed_for_parsing = trim(trimmed_for_parsing);
    }

    if (trimmed_for_parsing.find("if ") == 0 &&
        trimmed_for_parsing.find("; then") != std::string::npos) {
      if (trimmed_for_parsing.find(" fi") != std::string::npos ||
          trimmed_for_parsing.find(";fi") != std::string::npos ||
          trimmed_for_parsing.find("fi;") != std::string::npos) {
      } else {
        control_stack.push_back({"if", display_line});

        control_stack.back().first = "then";
      }
    } else if (trimmed_for_parsing.find("while ") == 0 &&
               trimmed_for_parsing.find("; do") != std::string::npos) {
      if (trimmed_for_parsing.find(" done") != std::string::npos ||
          trimmed_for_parsing.find(";done") != std::string::npos ||
          trimmed_for_parsing.find("done;") != std::string::npos) {
      } else {
        control_stack.push_back({"do", display_line});
      }
    } else if (trimmed_for_parsing.find("until ") == 0 &&
               trimmed_for_parsing.find("; do") != std::string::npos) {
      if (trimmed_for_parsing.find(" done") != std::string::npos ||
          trimmed_for_parsing.find(";done") != std::string::npos ||
          trimmed_for_parsing.find("done;") != std::string::npos) {
      } else {
        control_stack.push_back({"do", display_line});
      }
    } else if (trimmed_for_parsing.find("for ") == 0 &&
               trimmed_for_parsing.find("; do") != std::string::npos) {
      if (trimmed_for_parsing.find(" done") != std::string::npos ||
          trimmed_for_parsing.find(";done") != std::string::npos ||
          trimmed_for_parsing.find("done;") != std::string::npos) {
      } else {
        control_stack.push_back({"do", display_line});
      }
    } else {
      std::vector<std::string> tokens;
      std::stringstream ss(trimmed_for_parsing);
      std::string token;
      while (ss >> token) {
        tokens.push_back(token);
      }

      if (!tokens.empty()) {
        const std::string& first_token = tokens[0];

        if (first_token == "if") {
          control_stack.push_back({"if", display_line});
        } else if (first_token == "then") {
          if (control_stack.empty() || control_stack.back().first != "if") {
            errors.push_back(
                {display_line, "'then' without matching 'if'", line});
          } else {
            control_stack.back().first = "then";
          }
        } else if (first_token == "elif") {
          if (control_stack.empty() || (control_stack.back().first != "then" &&
                                        control_stack.back().first != "elif")) {
            errors.push_back(
                {display_line, "'elif' without matching 'if...then'", line});
          } else {
            control_stack.back().first = "elif";
          }
        } else if (first_token == "else") {
          if (control_stack.empty() || (control_stack.back().first != "then" &&
                                        control_stack.back().first != "elif")) {
            errors.push_back(
                {display_line, "'else' without matching 'if...then'", line});
          } else {
            control_stack.back().first = "else";
          }
        } else if (first_token == "fi") {
          if (control_stack.empty() || (control_stack.back().first != "then" &&
                                        control_stack.back().first != "elif" &&
                                        control_stack.back().first != "else")) {
            errors.push_back(
                {display_line, "'fi' without matching 'if'", line});
          } else {
            control_stack.pop_back();
          }
        }

        else if (first_token == "while" || first_token == "until") {
          control_stack.push_back({first_token, display_line});
        } else if (first_token == "do") {
          if (control_stack.empty() || (control_stack.back().first != "while" &&
                                        control_stack.back().first != "until" &&
                                        control_stack.back().first != "for")) {
            errors.push_back(
                {display_line,
                 "'do' without matching 'while', 'until', or 'for'", line});
          } else {
            control_stack.back().first = "do";
          }
        } else if (first_token == "done") {
          if (control_stack.empty() || control_stack.back().first != "do") {
            errors.push_back(
                {display_line, "'done' without matching 'do'", line});
          } else {
            control_stack.pop_back();
          }
        }

        else if (first_token == "for") {
          if (tokens.size() >= 3 &&
              std::find(tokens.begin(), tokens.end(), "in") != tokens.end()) {
          } else if (tokens.size() < 3) {
          } else {
            errors.push_back(
                {display_line, "'for' statement missing 'in' clause", line});
          }
          control_stack.push_back({"for", display_line});
        }

        else if (first_token == "case") {
          if (tokens.size() >= 3 &&
              std::find(tokens.begin(), tokens.end(), "in") != tokens.end()) {
          } else if (tokens.size() < 3) {
          } else {
            errors.push_back(
                {display_line, "'case' statement missing 'in' clause", line});
          }

          if (trimmed_for_parsing.find(" esac") != std::string::npos ||
              trimmed_for_parsing.find(";esac") != std::string::npos ||
              trimmed_for_parsing.find("esac;") != std::string::npos) {
          } else {
            control_stack.push_back({"case", display_line});
          }
        } else if (first_token == "esac") {
          if (control_stack.empty() || control_stack.back().first != "case") {
            errors.push_back(
                {display_line, "'esac' without matching 'case'", line});
          } else {
            control_stack.pop_back();
          }
        }

        else if (first_token == "function") {
          if (tokens.size() < 2) {
            errors.push_back(
                {display_line, "'function' missing function name", line});
          }
          if (!trimmed.empty() && trimmed.back() == '{') {
            control_stack.push_back({"{", display_line});
          } else {
            control_stack.push_back({"function", display_line});
          }
        } else if (tokens.size() >= 2 && tokens[1] == "()") {
          if (!trimmed.empty() && trimmed.back() == '{') {
            control_stack.push_back({"{", display_line});
          } else {
            control_stack.push_back({"function", display_line});
          }
        }

        else if (!trimmed.empty() && trimmed.back() == '{') {
          control_stack.push_back({"{", display_line});
        } else if (first_token == "}") {
          if (control_stack.empty()) {
            errors.push_back(
                {display_line, "Unmatched closing brace '}'", line});
          } else if (control_stack.back().first == "{" ||
                     control_stack.back().first == "function") {
            control_stack.pop_back();
          } else {
            errors.push_back(
                {display_line, "Unmatched closing brace '}'", line});
          }
        }
      }
    }
  }

  while (!control_stack.empty()) {
    auto& unclosed = control_stack.back();
    std::string expected_close;

    if (unclosed.first == "if" || unclosed.first == "then" ||
        unclosed.first == "elif" || unclosed.first == "else") {
      expected_close = "fi";
    } else if (unclosed.first == "while" || unclosed.first == "for" ||
               unclosed.first == "do") {
      expected_close = "done";
    } else if (unclosed.first == "case") {
      expected_close = "esac";
    } else if (unclosed.first == "{" || unclosed.first == "function") {
      expected_close = "}";
    }

    errors.push_back(
        {unclosed.second,
         "Unclosed '" + unclosed.first + "' - missing '" + expected_close + "'",
         ""});
    control_stack.pop_back();
  }

  return errors;
}

bool ShellScriptInterpreter::has_syntax_errors(
    const std::vector<std::string>& lines, bool print_errors) {
  std::vector<SyntaxError> errors =
      validate_comprehensive_syntax(lines, true, false, false);

  // Only check for critical errors
  bool has_critical_errors = false;
  for (const auto& error : errors) {
    if (error.severity == ErrorSeverity::CRITICAL) {
      has_critical_errors = true;
      break;
    }
  }

  // Only print error report if there are critical errors and printing is
  // enabled
  if (has_critical_errors && print_errors) {
    // Filter to only show critical errors
    std::vector<SyntaxError> critical_errors;
    for (const auto& error : errors) {
      if (error.severity == ErrorSeverity::CRITICAL) {
        critical_errors.push_back(error);
      }
    }
    if (!critical_errors.empty()) {
      print_error_report(critical_errors, true, true);
    }
  }

  return has_critical_errors;
}

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::validate_comprehensive_syntax(
    const std::vector<std::string>& lines, bool check_semantics,
    bool check_style, bool check_performance) {
  (void)check_performance;

  std::vector<SyntaxError> all_errors;

  auto basic_errors = validate_script_syntax(lines);
  all_errors.insert(all_errors.end(), basic_errors.begin(), basic_errors.end());

  auto var_errors = validate_variable_usage(lines);
  all_errors.insert(all_errors.end(), var_errors.begin(), var_errors.end());

  auto redir_errors = validate_redirection_syntax(lines);
  all_errors.insert(all_errors.end(), redir_errors.begin(), redir_errors.end());

  auto arith_errors = validate_arithmetic_expressions(lines);
  all_errors.insert(all_errors.end(), arith_errors.begin(), arith_errors.end());

  auto param_errors = validate_parameter_expansions(lines);
  all_errors.insert(all_errors.end(), param_errors.begin(), param_errors.end());

  auto flow_errors = analyze_control_flow(lines);
  all_errors.insert(all_errors.end(), flow_errors.begin(), flow_errors.end());

  if (check_semantics) {
    auto cmd_errors = validate_command_existence(lines);
    all_errors.insert(all_errors.end(), cmd_errors.begin(), cmd_errors.end());
  }

  if (check_style) {
    auto style_errors = check_style_guidelines(lines);
    all_errors.insert(all_errors.end(), style_errors.begin(),
                      style_errors.end());
  }

  return all_errors;
}

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::validate_variable_usage(
    const std::vector<std::string>& lines) {
  std::vector<SyntaxError> errors;
  std::map<std::string, std::vector<size_t>> defined_vars;
  std::map<std::string, std::vector<size_t>> used_vars;

  for (size_t line_num = 0; line_num < lines.size(); ++line_num) {
    const std::string& line = lines[line_num];
    size_t display_line = line_num + 1;

    std::string trimmed = line;
    size_t first_non_space = trimmed.find_first_not_of(" \t");
    if (first_non_space == std::string::npos ||
        trimmed[first_non_space] == '#') {
      continue;
    }

    size_t eq_pos = line.find('=');
    if (eq_pos != std::string::npos) {
      std::string before_eq = line.substr(0, eq_pos);

      size_t start = before_eq.find_first_not_of(" \t");
      if (start != std::string::npos) {
        before_eq = before_eq.substr(start);

        bool is_var_assignment =
            !before_eq.empty() &&
            (std::isalpha(before_eq[0]) || before_eq[0] == '_');

        for (size_t i = 1; i < before_eq.length() && is_var_assignment; ++i) {
          if (!std::isalnum(before_eq[i]) && before_eq[i] != '_') {
            is_var_assignment = false;
          }
        }

        if (is_var_assignment) {
          size_t actual_line = display_line;
          for (size_t j = 0; j < eq_pos; ++j) {
            if (line[j] == '\n') {
              actual_line++;
            }
          }
          defined_vars[before_eq].push_back(actual_line);
        }
      }
    }

    bool in_quotes = false;
    char quote_char = '\0';
    bool escaped = false;

    for (size_t i = 0; i < line.length(); ++i) {
      char c = line[i];

      if (escaped) {
        escaped = false;
        continue;
      }

      if (c == '\\' && (!in_quotes || quote_char != '\'')) {
        escaped = true;
        continue;
      }

      if ((c == '"' || c == '\'') && !in_quotes) {
        in_quotes = true;
        quote_char = c;
      } else if (c == quote_char && in_quotes) {
        in_quotes = false;
        quote_char = '\0';
      }

      if (in_quotes && quote_char == '\'') {
        continue;
      }

      if (c == '$' && i + 1 < line.length()) {
        std::string var_name;
        size_t var_start = i + 1;
        size_t var_end = var_start;

        if (line[var_start] == '{') {
          var_start++;
          var_end = line.find('}', var_start);
          if (var_end != std::string::npos) {
            var_name = line.substr(var_start, var_end - var_start);

            size_t colon_pos = var_name.find(':');
            if (colon_pos != std::string::npos) {
              var_name = var_name.substr(0, colon_pos);
            }
          } else {
            errors.push_back(SyntaxError({display_line, i, i + 2, 0},
                                         ErrorSeverity::ERROR,
                                         ErrorCategory::VARIABLES, "VAR001",
                                         "Unclosed variable expansion ${", line,
                                         "Add closing brace '}'"));
            continue;
          }
        } else if (std::isalpha(line[var_start]) || line[var_start] == '_') {
          while (var_end < line.length() &&
                 (std::isalnum(line[var_end]) || line[var_end] == '_')) {
            var_end++;
          }
          var_name = line.substr(var_start, var_end - var_start);
        }

        if (!var_name.empty()) {
          size_t actual_line = display_line;
          for (size_t j = 0; j < i; ++j) {
            if (line[j] == '\n') {
              actual_line++;
            }
          }
          used_vars[var_name].push_back(actual_line);
        }
      }
    }
  }

  for (const auto& [var_name, usage_lines] : used_vars) {
    if (defined_vars.find(var_name) == defined_vars.end()) {
      if (var_name != "PATH" && var_name != "HOME" && var_name != "USER" &&
          var_name != "PWD" && var_name != "SHELL" && var_name != "TERM" &&
          var_name != "TMUX" && var_name != "DISPLAY" && var_name != "EDITOR" &&
          var_name != "PAGER" && var_name != "LANG" && var_name != "LC_ALL" &&
          var_name != "TZ" && var_name != "SSH_CLIENT" &&
          var_name != "SSH_TTY" && !std::isdigit(var_name[0])) {
        for (size_t line : usage_lines) {
          errors.push_back(SyntaxError(
              {line, 0, 0, 0}, ErrorSeverity::WARNING, ErrorCategory::VARIABLES,
              "VAR002",
              "Variable '" + var_name + "' used but not defined in this script",
              "", "Define the variable before use: " + var_name + "=value"));
        }
      }
    }
  }

  for (const auto& [var_name, def_lines] : defined_vars) {
    if (used_vars.find(var_name) == used_vars.end()) {
      for (size_t line : def_lines) {
        errors.push_back(SyntaxError(
            {line, 0, 0, 0}, ErrorSeverity::INFO, ErrorCategory::VARIABLES,
            "VAR003", "Variable '" + var_name + "' defined but never used", "",
            "Remove unused variable or add usage"));
      }
    }
  }

  return errors;
}

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::validate_redirection_syntax(
    const std::vector<std::string>& lines) {
  std::vector<SyntaxError> errors;

  for (size_t line_num = 0; line_num < lines.size(); ++line_num) {
    const std::string& line = lines[line_num];
    size_t display_line = line_num + 1;

    std::string trimmed = line;
    size_t first_non_space = trimmed.find_first_not_of(" \t");
    if (first_non_space == std::string::npos ||
        trimmed[first_non_space] == '#') {
      continue;
    }

    bool in_quotes = false;
    char quote_char = '\0';
    bool escaped = false;

    for (size_t i = 0; i < line.length(); ++i) {
      char c = line[i];

      if (escaped) {
        escaped = false;
        continue;
      }

      if (c == '\\' && (!in_quotes || quote_char != '\'')) {
        escaped = true;
        continue;
      }

      if ((c == '"' || c == '\'') && !in_quotes) {
        in_quotes = true;
        quote_char = c;
      } else if (c == quote_char && in_quotes) {
        in_quotes = false;
        quote_char = '\0';
      }

      if (!in_quotes) {
        if (c == '<' || c == '>') {
          size_t redir_start = i;
          std::string redir_op;

          if (c == '>' && i + 1 < line.length()) {
            if (line[i + 1] == '>') {
              redir_op = ">>";
              i++;
            } else if (line[i + 1] == '&') {
              redir_op = ">&";
              i++;
            } else if (line[i + 1] == '|') {
              redir_op = ">|";
              i++;
            } else {
              redir_op = ">";
            }
          } else if (c == '<' && i + 1 < line.length()) {
            if (line[i + 1] == '<') {
              if (i + 2 < line.length() && line[i + 2] == '<') {
                redir_op = "<<<";
                i += 2;
              } else {
                redir_op = "<<";
                i++;
              }
            } else {
              redir_op = "<";
            }
          } else {
            redir_op = c;
          }

          size_t target_start = i + 1;
          while (target_start < line.length() &&
                 std::isspace(line[target_start])) {
            target_start++;
          }

          if (target_start >= line.length()) {
            errors.push_back(SyntaxError(
                {display_line, redir_start, i + 1, 0}, ErrorSeverity::ERROR,
                ErrorCategory::REDIRECTION, "RED001",
                "Redirection '" + redir_op + "' missing target", line,
                "Add filename or file descriptor after " + redir_op));
            continue;
          }

          std::string target;
          size_t target_end = target_start;
          bool in_target_quotes = false;
          char target_quote = '\0';

          while (target_end < line.length()) {
            char tc = line[target_end];
            if (!in_target_quotes && std::isspace(tc)) {
              break;
            }
            if ((tc == '"' || tc == '\'') && !in_target_quotes) {
              in_target_quotes = true;
              target_quote = tc;
            } else if (tc == target_quote && in_target_quotes) {
              in_target_quotes = false;
              target_quote = '\0';
            }
            target_end++;
          }

          target = line.substr(target_start, target_end - target_start);

          if (redir_op == ">&" || redir_op == "<&") {
            if (target.empty() || (!std::isdigit(target[0]) && target != "-")) {
              errors.push_back(SyntaxError(
                  {display_line, target_start, target_end, 0},
                  ErrorSeverity::ERROR, ErrorCategory::REDIRECTION, "RED002",
                  "File descriptor redirection requires digit or '-'", line,
                  "Use format like 2>&1 or 2>&-"));
            }
          } else if (redir_op == "<<") {
            if (target.empty()) {
              errors.push_back(
                  SyntaxError({display_line, target_start, target_end, 0},
                              ErrorSeverity::ERROR, ErrorCategory::REDIRECTION,
                              "RED003", "Here document missing delimiter", line,
                              "Provide delimiter like: << EOF"));
            }
          }

          i = target_end - 1;
        }

        if (c == '|' && i + 1 < line.length()) {
          if (line[i + 1] == '|') {
            i++;
          } else {
            size_t pipe_pos = i;
            size_t after_pipe = i + 1;
            while (after_pipe < line.length() &&
                   std::isspace(line[after_pipe])) {
              after_pipe++;
            }

            if (after_pipe >= line.length() || line[after_pipe] == '|' ||
                line[after_pipe] == '&') {
              errors.push_back(
                  SyntaxError({display_line, pipe_pos, pipe_pos + 1, 0},
                              ErrorSeverity::ERROR, ErrorCategory::REDIRECTION,
                              "RED004", "Pipe missing command after '|'", line,
                              "Add command after pipe"));
            }
          }
        }
      }
    }
  }

  return errors;
}

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::validate_arithmetic_expressions(
    const std::vector<std::string>& lines) {
  std::vector<SyntaxError> errors;

  for (size_t line_num = 0; line_num < lines.size(); ++line_num) {
    const std::string& line = lines[line_num];
    size_t display_line = line_num + 1;

    std::string trimmed = line;
    size_t first_non_space = trimmed.find_first_not_of(" \t");
    if (first_non_space == std::string::npos ||
        trimmed[first_non_space] == '#') {
      continue;
    }

    bool in_quotes = false;
    char quote_char = '\0';
    bool escaped = false;

    for (size_t i = 0; i < line.length(); ++i) {
      char c = line[i];

      if (escaped) {
        escaped = false;
        continue;
      }

      if (c == '\\' && (!in_quotes || quote_char != '\'')) {
        escaped = true;
        continue;
      }

      if ((c == '"' || c == '\'') && !in_quotes) {
        in_quotes = true;
        quote_char = c;
      } else if (c == quote_char && in_quotes) {
        in_quotes = false;
        quote_char = '\0';
      }

      if (in_quotes && quote_char == '\'') {
        continue;
      }

      if (c == '$' && i + 2 < line.length() && line[i + 1] == '(' &&
          line[i + 2] == '(') {
        size_t start = i;
        size_t paren_count = 2;
        size_t j = i + 3;
        std::string expr;

        while (j < line.length() && paren_count > 0) {
          if (line[j] == '(') {
            paren_count++;
          } else if (line[j] == ')') {
            paren_count--;
          }
          if (paren_count > 0) {
            expr += line[j];
          }
          j++;
        }

        if (paren_count > 0) {
          size_t actual_line = display_line;
          for (size_t k = 0; k < start; ++k) {
            if (line[k] == '\n') {
              actual_line++;
            }
          }
          errors.push_back(SyntaxError(
              {actual_line, start, j, 0}, ErrorSeverity::ERROR,
              ErrorCategory::SYNTAX, "ARITH001",
              "Unclosed arithmetic expansion $(()", line, "Add closing ))"));
        } else {
          if (expr.empty()) {
            size_t actual_line = display_line;
            for (size_t k = 0; k < start; ++k) {
              if (line[k] == '\n') {
                actual_line++;
              }
            }
            errors.push_back(SyntaxError({actual_line, start, j, 0},
                                         ErrorSeverity::ERROR,
                                         ErrorCategory::SYNTAX, "ARITH002",
                                         "Empty arithmetic expression", line,
                                         "Provide expression inside $(( ))"));
          } else {
            if (expr.find("/0") != std::string::npos ||
                expr.find("% 0") != std::string::npos) {
              size_t actual_line = display_line;
              for (size_t k = 0; k < start; ++k) {
                if (line[k] == '\n') {
                  actual_line++;
                }
              }
              errors.push_back(SyntaxError({actual_line, start, j, 0},
                                           ErrorSeverity::WARNING,
                                           ErrorCategory::SEMANTICS, "ARITH004",
                                           "Potential division by zero", line,
                                           "Ensure divisor is not zero"));
            }

            int balance = 0;
            for (char ec : expr) {
              if (ec == '(')
                balance++;
              else if (ec == ')')
                balance--;
              if (balance < 0)
                break;
            }
            if (balance != 0) {
              errors.push_back(
                  SyntaxError({display_line, start, j, 0}, ErrorSeverity::ERROR,
                              ErrorCategory::SYNTAX, "ARITH005",
                              "Unbalanced parentheses in arithmetic expression",
                              line, "Check parentheses balance in expression"));
            }
          }
        }

        i = j - 1;
      }

      else if (c == '$' && i + 1 < line.length() && line[i + 1] == '[') {
        errors.push_back(
            SyntaxError({display_line, i, i + 2, 0}, ErrorSeverity::WARNING,
                        ErrorCategory::STYLE, "ARITH006",
                        "Deprecated arithmetic syntax $[...], use $((...))",
                        line, "Replace $[expr] with $((expr))"));
      }
    }
  }

  return errors;
}

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::validate_parameter_expansions(
    const std::vector<std::string>& lines) {
  (void)lines;
  std::vector<SyntaxError> errors;

  return errors;
}

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::validate_command_existence(
    const std::vector<std::string>& lines) {
  (void)lines;
  std::vector<SyntaxError> errors;

  return errors;
}

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::analyze_control_flow(
    const std::vector<std::string>& lines) {
  (void)lines;
  std::vector<SyntaxError> errors;

  return errors;
}

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::check_style_guidelines(
    const std::vector<std::string>& lines) {
  (void)lines;
  std::vector<SyntaxError> errors;

  return errors;
}

void ShellScriptInterpreter::print_error_report(
    const std::vector<SyntaxError>& errors, bool show_suggestions,
    bool show_context) const {
  if (errors.empty()) {
    std::cout << "\033[32m✓ No syntax errors found.\033[0m" << std::endl;
    return;
  }

  const std::string RESET = "\033[0m";
  const std::string BOLD = "\033[1m";
  const std::string DIM = "\033[2m";
  const std::string RED = "\033[31m";
  const std::string GREEN = "\033[32m";
  const std::string YELLOW = "\033[33m";
  const std::string BLUE = "\033[34m";
  const std::string MAGENTA = "\033[35m";
  const std::string CYAN = "\033[36m";
  const std::string WHITE = "\033[37m";
  const std::string BG_RED = "\033[41m";
  const std::string BG_YELLOW = "\033[43m";

  std::vector<SyntaxError> sorted_errors = errors;
  std::sort(sorted_errors.begin(), sorted_errors.end(),
            [](const SyntaxError& a, const SyntaxError& b) {
              if (a.position.line_number != b.position.line_number) {
                return a.position.line_number < b.position.line_number;
              }
              return a.position.column_start < b.position.column_start;
            });

  int error_count = 0;
  for (const auto& error : sorted_errors) {
    error_count++;

    std::string severity_color;
    std::string severity_icon;
    std::string severity_prefix;

    switch (error.severity) {
      case ErrorSeverity::CRITICAL:
        severity_color = BOLD + RED;
        severity_icon = "";
        severity_prefix = "CRITICAL";
        break;
      case ErrorSeverity::ERROR:
        severity_color = RED;
        severity_icon = "";
        severity_prefix = "ERROR";
        break;
      case ErrorSeverity::WARNING:
        severity_color = YELLOW;
        severity_icon = "";
        severity_prefix = "WARNING";
        break;
      case ErrorSeverity::INFO:
        severity_color = CYAN;
        severity_icon = "";
        severity_prefix = "INFO";
        break;
    }

    std::cout << BOLD << "┌─ " << error_count << ". " << severity_icon << " "
              << severity_color << severity_prefix << RESET << BOLD << " ["
              << BLUE << error.error_code << RESET << BOLD << "]" << RESET
              << std::endl;

    std::cout << "│  " << DIM << "at line " << BOLD
              << error.position.line_number << RESET;
    if (error.position.column_start > 0) {
      std::cout << DIM << ", column " << BOLD << error.position.column_start
                << RESET;
    }
    std::cout << std::endl;

    std::cout << "│  " << severity_color << error.message << RESET << std::endl;

    if (show_context && !error.line_content.empty()) {
      std::cout << "│" << std::endl;

      std::string line_num_str = std::to_string(error.position.line_number);
      std::cout << "│  " << DIM << line_num_str << " │ " << RESET;

      size_t terminal_width = get_terminal_width();
      size_t line_prefix_width = 6 + line_num_str.length();
      size_t available_width = terminal_width > line_prefix_width + 10
                                   ? terminal_width - line_prefix_width - 5
                                   : 60;

      std::string display_line = error.line_content;

      size_t adjusted_column_start = error.position.column_start;
      size_t adjusted_column_end = error.position.column_end;

      if (display_line.find('\n') != std::string::npos) {
        std::vector<std::string> lines;
        std::stringstream ss(display_line);
        std::string line;

        while (std::getline(ss, line)) {
          lines.push_back(line);
        }

        if (error.position.column_start > 0 && !lines.empty()) {
          size_t cumulative_length = 0;
          size_t target_line_index = 0;

          for (size_t i = 0; i < lines.size(); ++i) {
            if (error.position.column_start <=
                cumulative_length + lines[i].length()) {
              target_line_index = i;
              adjusted_column_start =
                  error.position.column_start - cumulative_length;
              adjusted_column_end =
                  std::min(error.position.column_end - cumulative_length,
                           lines[i].length());
              break;
            }
            cumulative_length += lines[i].length() + 1;
          }

          if (target_line_index < lines.size()) {
            display_line = lines[target_line_index];
          } else {
            display_line = lines[0];
            adjusted_column_start = 0;
            adjusted_column_end =
                std::min(error.position.column_end, display_line.length());
          }
        } else {
          display_line = lines.empty() ? "" : lines[0];
          adjusted_column_start = 0;
          adjusted_column_end = display_line.length();
        }
      }

      size_t pos = 0;
      while ((pos = display_line.find("\\x0a", pos)) != std::string::npos) {
        display_line.replace(pos, 4, "\n");
        pos += 1;
      }
      pos = 0;
      while ((pos = display_line.find("\\x09", pos)) != std::string::npos) {
        display_line.replace(pos, 4, "\t");
        pos += 1;
      }
      pos = 0;
      while ((pos = display_line.find("\\x0d", pos)) != std::string::npos) {
        display_line.replace(pos, 4, "\r");
        pos += 1;
      }

      for (size_t i = 0; i < display_line.length(); ++i) {
        if (display_line[i] == '\t') {
          display_line.replace(i, 1, "    ");
          i += 3;
        } else if (display_line[i] == '\n' || display_line[i] == '\r') {
          display_line[i] = ' ';
        } else if (display_line[i] < 32 && display_line[i] != ' ') {
          char hex_repr[5];
          snprintf(hex_repr, sizeof(hex_repr), "\\x%02x",
                   (unsigned char)display_line[i]);
          display_line.replace(i, 1, hex_repr);
          i += 3;
        }
      }

      size_t display_start_col = 0;
      size_t adjusted_start = adjusted_column_start;
      size_t adjusted_end = adjusted_column_end;

      if (display_line.length() > available_width) {
        size_t error_col = adjusted_start;

        size_t prefix_context = available_width / 4;
        size_t error_context = available_width / 2;
        size_t suffix_context =
            available_width - prefix_context - error_context;

        if (error_col <= prefix_context) {
          display_start_col = 0;
          size_t end_pos = std::min(display_line.length(), available_width - 3);
          display_line = display_line.substr(0, end_pos) + "...";
        } else if (error_col + suffix_context >= display_line.length()) {
          display_start_col =
              display_line.length() > available_width - 3
                  ? display_line.length() - (available_width - 3)
                  : 0;
          display_line = "..." + display_line.substr(display_start_col);
          adjusted_start = adjusted_start - display_start_col + 3;
          adjusted_end = adjusted_end - display_start_col + 3;
        } else {
          size_t ideal_start =
              error_col > prefix_context ? error_col - prefix_context : 0;

          display_start_col = ideal_start;
          for (size_t i = ideal_start;
               i > 0 && i > ideal_start - 10 && i < display_line.length();
               --i) {
            if (display_line[i] == ' ' || display_line[i] == '\t' ||
                display_line[i] == '(' || display_line[i] == '[' ||
                display_line[i] == '{' || display_line[i] == '"' ||
                display_line[i] == '\'') {
              display_start_col = i + 1;
              break;
            }
          }

          size_t display_end = std::min(display_start_col + available_width - 6,
                                        display_line.length());

          for (size_t i = display_end;
               i < display_line.length() && i < display_end + 10; ++i) {
            if (display_line[i] == ' ' || display_line[i] == '\t' ||
                display_line[i] == ')' || display_line[i] == ']' ||
                display_line[i] == '}' || display_line[i] == '"' ||
                display_line[i] == '\'') {
              display_end = i;
              break;
            }
          }

          display_line = "..." +
                         display_line.substr(display_start_col,
                                             display_end - display_start_col) +
                         "...";
          adjusted_start = adjusted_start - display_start_col + 3;
          adjusted_end = adjusted_end - display_start_col + 3;
        }
      }

      if (error.position.column_start > 0 &&
          error.position.column_end > error.position.column_start &&
          adjusted_start < display_line.length()) {
        size_t start = adjusted_start;
        size_t end = std::min(adjusted_end, display_line.length());

        if (start < display_line.length()) {
          std::cout << display_line.substr(0, start);
          std::cout << BG_RED << WHITE;
          if (end <= display_line.length()) {
            std::cout << display_line.substr(start, end - start);
            std::cout << RESET;
            if (end < display_line.length()) {
              std::cout << display_line.substr(end);
            }
          } else {
            std::cout << display_line.substr(start) << RESET;
          }
        } else {
          std::cout << display_line;
        }
      } else {
        std::cout << display_line;
      }
      std::cout << std::endl;

      if (error.position.column_start > 0 &&
          adjusted_start < display_line.length()) {
        std::cout << "│  " << DIM << std::string(line_num_str.length(), ' ')
                  << " │ " << RESET;
        std::cout << std::string(adjusted_start, ' ');
        std::cout << severity_color << "^";
        if (adjusted_end > adjusted_start + 1 &&
            adjusted_end <= display_line.length()) {
          size_t tilde_count =
              std::min(adjusted_end - adjusted_start - 1,
                       display_line.length() - adjusted_start - 1);
          std::cout << std::string(tilde_count, '~');
        }
        std::cout << RESET << std::endl;
      }
    }

    if (show_suggestions && !error.suggestion.empty()) {
      std::cout << "│" << std::endl;
      std::cout << "│  " << GREEN << "Suggestion: " << RESET << error.suggestion
                << std::endl;
    }

    // if (!error.related_info.empty()) {
    //   std::cout << "│" << std::endl;
    //   for (const auto& info : error.related_info) {
    //     std::cout << "│  " << DIM << "Note: " << info << RESET << std::endl;
    //   }
    // }

    // if (!error.documentation_url.empty()) {
    //   std::cout << "│" << std::endl;
    //   std::cout << "│  " << BLUE << "Documentation: " <<
    //   error.documentation_url
    //             << RESET << std::endl;
    // }

    size_t terminal_width = get_terminal_width();
    size_t content_width = 0;

    if (!error.line_content.empty()) {
      std::string line_num_str = std::to_string(error.position.line_number);
      size_t line_prefix_width = 6 + line_num_str.length();
      size_t max_line_display_width =
          terminal_width > line_prefix_width + 10
              ? terminal_width - line_prefix_width - 5
              : 60;

      size_t actual_line_width =
          std::min(error.line_content.length(), max_line_display_width);
      content_width =
          std::max(content_width, line_prefix_width + actual_line_width);
    }

    content_width = std::max(content_width, 3 + error.message.length());

    if (!error.suggestion.empty()) {
      content_width = std::max(content_width, 15 + error.suggestion.length());
    }

    size_t footer_width =
        std::min(content_width, terminal_width > 10 ? terminal_width - 2 : 50);
    footer_width = std::max(footer_width, static_cast<size_t>(50));
    footer_width = std::min(footer_width, terminal_width - 2);

    std::cout << "└";
    for (size_t i = 0; i < footer_width; i++) {
      std::cout << "—";
    }
  }
  std::cout << std::endl;
}

int ShellScriptInterpreter::execute_block(
    const std::vector<std::string>& lines) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Executing script block with " << lines.size()
              << " lines" << std::endl;

  if (g_shell == nullptr) {
    print_error(
        {ErrorType::RUNTIME_ERROR, "", "No shell instance available", {}});
  }

  if (!shell_parser) {
    print_error({ErrorType::RUNTIME_ERROR,
                 "",
                 "Script interpreter not properly initialized",
                 {}});
    return 1;
  }

  if (has_syntax_errors(lines)) {
    print_error(
        {ErrorType::SYNTAX_ERROR,
         "",
         "Critical syntax errors detected in script block, process aborted",
         {}});
    return 2;
  }

  auto trim = [](const std::string& s) -> std::string {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
      return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
  };

  auto strip_inline_comment = [](const std::string& s) -> std::string {
    bool in_quotes = false;
    bool in_brace_expansion = false;
    char quote = '\0';
    for (size_t i = 0; i < s.size(); ++i) {
      char c = s[i];

      if (!in_quotes && c == '$' && i + 1 < s.size() && s[i + 1] == '{') {
        in_brace_expansion = true;
      } else if (in_brace_expansion && c == '}') {
        in_brace_expansion = false;
      }

      if (!in_quotes && !in_brace_expansion && c == '$' && i + 1 < s.size()) {
        char next = s[i + 1];
        if (next == '#' || next == '?' || next == '$' || next == '*' ||
            next == '@' || next == '!' || isdigit(next)) {
          ++i;
          continue;
        }
      }

      if (c == '"' || c == '\'') {
        size_t backslash_count = 0;
        for (size_t j = i; j > 0; --j) {
          if (s[j - 1] == '\\') {
            backslash_count++;
          } else {
            break;
          }
        }

        bool is_escaped = (backslash_count % 2) == 1;

        if (!is_escaped) {
          if (!in_quotes) {
            in_quotes = true;
            quote = c;
          } else if (quote == c) {
            in_quotes = false;
            quote = '\0';
          }
        }
      } else if (!in_quotes && !in_brace_expansion && c == '#') {
        return s.substr(0, i);
      }
    }
    return s;
  };

  auto is_readable_file = [](const std::string& path) -> bool {
    struct stat st{};
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) &&
           access(path.c_str(), R_OK) == 0;
  };

  auto should_interpret_as_cjsh = [&](const std::string& path) -> bool {
    if (!is_readable_file(path))
      return false;

    if (path.size() >= 5 && path.substr(path.size() - 5) == ".cjsh")
      return true;
    std::ifstream f(path);
    if (!f)
      return false;
    std::string first_line;
    std::getline(f, first_line);
    if (first_line.rfind("#!", 0) == 0 &&
        first_line.find("cjsh") != std::string::npos)
      return true;
    if (first_line.find("cjsh") != std::string::npos)
      return true;
    return false;
  };

  std::function<int(const std::string&)> execute_simple_or_pipeline;

  execute_simple_or_pipeline = [&](const std::string& cmd_text) -> int {
    if (g_debug_mode) {
      std::cerr << "DEBUG: execute_simple_or_pipeline called with: " << cmd_text
                << std::endl;
    }

    std::string text = trim(strip_inline_comment(cmd_text));
    if (text.empty())
      return 0;

    auto capture_internal_output =
        [&](const std::string& content) -> std::string {
      char tmpl[] = "/tmp/cjsh_subst_XXXXXX";
      int fd = mkstemp(tmpl);
      if (fd >= 0)
        close(fd);
      std::string path = tmpl;

      int saved_stdout = dup(STDOUT_FILENO);

      FILE* temp_file = fopen(path.c_str(), "w");
      if (!temp_file) {
        int pipefd[2];
        if (pipe(pipefd) != 0) {
          return "";
        }

        pid_t pid = fork();
        if (pid == 0) {
          close(pipefd[0]);
          dup2(pipefd[1], STDOUT_FILENO);
          close(pipefd[1]);

          int exit_code = execute_simple_or_pipeline(content);
          exit(exit_code);
        } else if (pid > 0) {
          close(pipefd[1]);
          std::string result;
          char buf[4096];
          ssize_t n;
          while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
            result.append(buf, n);
          }
          close(pipefd[0]);

          int status;
          waitpid(pid, &status, 0);

          while (!result.empty() &&
                 (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();

          return result;
        } else {
          close(pipefd[0]);
          close(pipefd[1]);
          return "";
        }
      }

      int temp_fd = fileno(temp_file);
      dup2(temp_fd, STDOUT_FILENO);

      execute_simple_or_pipeline(content);

      fflush(stdout);
      fclose(temp_file);
      dup2(saved_stdout, STDOUT_FILENO);
      close(saved_stdout);

      std::ifstream ifs(path);
      std::stringstream buffer;
      buffer << ifs.rdbuf();
      std::string out = buffer.str();
      ::unlink(path.c_str());

      while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();

      return out;
    };

    auto eval_arith = [&](const std::string& expr) -> long long {
      struct Tok {
        enum T {
          NUM,
          OP,
          LP,
          RP
        } t;
        long long v;
        char op;
      };
      auto is_space = [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
      };
      auto prec = [](char op) {
        if (op == '+' || op == '-')
          return 1;
        if (op == '*' || op == '/' || op == '%')
          return 2;
        return 0;
      };
      auto apply = [](long long a, long long b, char op) -> long long {
        switch (op) {
          case '+':
            return a + b;
          case '-':
            return a - b;
          case '*':
            return a * b;
          case '/':
            return b == 0 ? 0 : a / b;
          case '%':
            return b == 0 ? 0 : a % b;
          default:
            return 0;
        }
      };
      std::vector<Tok> output;
      std::vector<char> ops;

      for (size_t i = 0; i < expr.size();) {
        if (is_space(expr[i])) {
          ++i;
          continue;
        }
        if (isdigit(expr[i])) {
          long long val = 0;
          size_t j = i;
          while (j < expr.size() && isdigit(expr[j])) {
            val = val * 10 + (expr[j] - '0');
            ++j;
          }
          output.push_back({Tok::NUM, val, 0});
          i = j;
          continue;
        }
        if (isalpha(expr[i]) || expr[i] == '_') {
          size_t j = i;
          while (j < expr.size() && (isalnum(expr[j]) || expr[j] == '_'))
            ++j;
          std::string name = expr.substr(i, j - i);
          const char* ev = getenv(name.c_str());
          long long val = 0;
          if (ev) {
            try {
              val = std::stoll(ev);
            } catch (...) {
              val = 0;
            }
          }
          output.push_back({Tok::NUM, val, 0});
          i = j;
          continue;
        }
        if (expr[i] == '(') {
          ops.push_back('(');
          ++i;
          continue;
        }
        if (expr[i] == ')') {
          while (!ops.empty() && ops.back() != '(') {
            char op = ops.back();
            ops.pop_back();
            output.push_back({Tok::OP, 0, op});
          }
          if (!ops.empty() && ops.back() == '(')
            ops.pop_back();
          ++i;
          continue;
        }
        char op = expr[i];
        if (op == '+' || op == '-' || op == '*' || op == '/' || op == '%') {
          while (!ops.empty() && ops.back() != '(' &&
                 prec(ops.back()) >= prec(op)) {
            char top = ops.back();
            ops.pop_back();
            output.push_back({Tok::OP, 0, top});
          }
          ops.push_back(op);
          ++i;
          continue;
        }

        ++i;
      }
      while (!ops.empty()) {
        char op = ops.back();
        ops.pop_back();
        if (op != '(')
          output.push_back({Tok::OP, 0, op});
      }

      std::vector<long long> st;
      for (auto& t : output) {
        if (t.t == Tok::NUM)
          st.push_back(t.v);
        else if (t.t == Tok::OP) {
          if (st.size() < 2) {
            st.clear();
            break;
          }
          long long b = st.back();
          st.pop_back();
          long long a = st.back();
          st.pop_back();
          st.push_back(apply(a, b, t.op));
        }
      }
      return st.empty() ? 0 : st.back();
    };

    auto expand_substitutions = [&](const std::string& in) -> std::string {
      std::string out;
      out.reserve(in.size());

      bool in_quotes = false;
      char q = '\0';
      bool escaped = false;

      auto find_matching = [&](const std::string& s, size_t start, char open_c,
                               char close_c, size_t& end_out) -> bool {
        int depth = 1;
        bool local_in_q = false;
        char local_q = '\0';
        for (size_t j = start; j < s.size(); ++j) {
          char d = s[j];
          if ((d == '"' || d == '\'') && (j == start || s[j - 1] != '\\')) {
            if (!local_in_q) {
              local_in_q = true;
              local_q = d;
            } else if (local_q == d) {
              local_in_q = false;
              local_q = '\0';
            }
          } else if (!local_in_q) {
            if (d == open_c)
              depth++;
            else if (d == close_c) {
              depth--;
              if (depth == 0) {
                end_out = j;
                return true;
              }
            }
          }
        }
        return false;
      };

      for (size_t i = 0; i < in.size(); ++i) {
        char c = in[i];

        if (escaped) {
          out += '\\';
          out += c;
          escaped = false;
          continue;
        }

        if (c == '\\' && (!in_quotes || q != '\'')) {
          escaped = true;
          continue;
        }

        if ((c == '"' || c == '\'') && (!in_quotes)) {
          in_quotes = true;
          q = c;
          out += c;
          continue;
        }
        if (in_quotes && c == q) {
          in_quotes = false;
          q = '\0';
          out += c;
          continue;
        }

        if (!in_quotes || q == '"') {
          if (c == '$' && i + 2 < in.size() && in[i + 1] == '(' &&
              in[i + 2] == '(') {
            size_t end_idx = 0;

            size_t inner_start = i + 3;
            int depth = 1;
            bool local_in_q = false;
            char local_q = '\0';
            size_t j = inner_start;
            bool found = false;
            for (; j < in.size(); ++j) {
              char d = in[j];
              if ((d == '"' || d == '\'') &&
                  (j == inner_start || in[j - 1] != '\\')) {
                if (!local_in_q) {
                  local_in_q = true;
                  local_q = d;
                } else if (local_q == d) {
                  local_in_q = false;
                  local_q = '\0';
                }
              } else if (!local_in_q) {
                if (d == '(')
                  depth++;
                else if (d == ')') {
                  depth--;
                  if (depth == 0) {
                    if (j + 1 < in.size() && in[j + 1] == ')') {
                      end_idx = j + 1;
                      found = true;
                    }
                    break;
                  }
                }
              }
            }
            if (found) {
              size_t expr_len = (j > inner_start + 1) ? (j - inner_start) : 0;
              std::string expr = in.substr(inner_start, expr_len);
              out += std::to_string(eval_arith(expr));
              i = end_idx;
              continue;
            }
          }

          if (c == '$' && i + 1 < in.size() && in[i + 1] == '(' &&
              !(i + 2 < in.size() && in[i + 2] == '(')) {
            size_t end_paren = 0;
            if (find_matching(in, i + 2, '(', ')', end_paren)) {
              std::string inner = in.substr(i + 2, end_paren - (i + 2));
              std::string repl = capture_internal_output(inner);
              if (in_quotes && q == '"') {
                std::string esc;
                esc.reserve(repl.size() * 1.1);
                for (char rc : repl) {
                  if (rc == '"' || rc == '\\') {
                    esc += '\\';
                  }
                  esc += rc;
                }

                out += "\x1E__NOENV_START__\x1E";
                out += esc;
                out += "\x1E__NOENV_END__\x1E";
              } else {
                out += repl;
              }
              i = end_paren;
              continue;
            }
          }

          if (c == '`') {
            size_t j = i + 1;
            bool found = false;
            while (j < in.size()) {
              if (in[j] == '\\') {
                if (j + 1 < in.size()) {
                  j += 2;
                  continue;
                }
              }
              if (in[j] == '`' && (j == 0 || in[j - 1] != '\\')) {
                found = true;
                break;
              }
              ++j;
            }
            if (found) {
              std::string inner = in.substr(i + 1, j - (i + 1));

              std::string content;
              content.reserve(inner.size());
              for (size_t k = 0; k < inner.size(); ++k) {
                if (inner[k] == '\\' && k + 1 < inner.size() &&
                    inner[k + 1] == '`') {
                  content.push_back('`');
                  ++k;
                } else {
                  content.push_back(inner[k]);
                }
              }
              std::string repl = capture_internal_output(content);
              if (in_quotes && q == '"') {
                std::string esc;
                esc.reserve(repl.size() * 1.1);
                for (char rc : repl) {
                  if (rc == '"' || rc == '\\') {
                    esc += '\\';
                  }
                  esc += rc;
                }
                out += "\x1E__NOENV_START__\x1E";
                out += esc;
                out += "\x1E__NOENV_END__\x1E";
              } else {
                out += repl;
              }
              i = j;
              continue;
            }
          }

          if (c == '$' && i + 1 < in.size() && in[i + 1] == '{') {
            size_t end_brace = 0;
            if (find_matching(in, i + 2, '{', '}', end_brace)) {
              std::string param_expr = in.substr(i + 2, end_brace - (i + 2));
              out += expand_parameter_expression(param_expr);
              i = end_brace;
              continue;
            }
          }
        }

        out += c;
      }

      return out;
    };

    text = expand_substitutions(text);

    std::vector<std::string> head = shell_parser->parse_command(text);
    if (!head.empty()) {
      const std::string& prog = head[0];
      if (should_interpret_as_cjsh(prog)) {
        if (g_debug_mode)
          std::cerr << "DEBUG: Interpreting script file: " << prog << std::endl;
        std::ifstream f(prog);
        if (!f) {
          print_error({ErrorType::RUNTIME_ERROR,
                       "",
                       "Failed to open script file: " + prog,
                       {}});
          return 1;
        }
        std::stringstream buffer;
        buffer << f.rdbuf();
        auto nested_lines = shell_parser->parse_into_lines(buffer.str());
        return execute_block(nested_lines);
      }
    }

    if ((text == "case" || text.rfind("case ", 0) == 0) &&
        (text.find(" in ") != std::string::npos) &&
        (text.find("esac") != std::string::npos)) {
      if (g_debug_mode) {
        std::cerr << "DEBUG: Handling inline case statement directly: " << text
                  << std::endl;
      }

      size_t in_pos = text.find(" in ");
      std::string case_part = text.substr(0, in_pos);
      std::string patterns_part = text.substr(in_pos + 4);

      std::vector<std::string> case_toks =
          shell_parser->parse_command(case_part);
      std::string case_value;
      if (case_toks.size() >= 2 && case_toks[0] == "case") {
        case_value = case_toks[1];
      }

      size_t esac_pos = patterns_part.find("esac");
      if (esac_pos != std::string::npos) {
        patterns_part = patterns_part.substr(0, esac_pos);
      }

      std::vector<std::string> pattern_sections;
      size_t start = 0;
      while (start < patterns_part.length()) {
        size_t sep_pos = patterns_part.find(";;", start);
        if (sep_pos == std::string::npos) {
          if (start < patterns_part.length()) {
            pattern_sections.push_back(trim(patterns_part.substr(start)));
          }
          break;
        }
        pattern_sections.push_back(
            trim(patterns_part.substr(start, sep_pos - start)));
        start = sep_pos + 2;
      }

      for (const auto& section : pattern_sections) {
        if (section.empty())
          continue;

        size_t paren_pos = section.find(')');
        if (paren_pos != std::string::npos) {
          std::string pattern = trim(section.substr(0, paren_pos));
          std::string command_part = trim(section.substr(paren_pos + 1));

          bool pattern_matches = false;
          if (pattern == "*") {
            pattern_matches = true;
          } else if (pattern == case_value) {
            pattern_matches = true;
          } else if (pattern.find('*') != std::string::npos) {
            if (pattern.back() == '*' && pattern.size() > 1) {
              std::string prefix = pattern.substr(0, pattern.size() - 1);
              pattern_matches = (case_value.substr(0, prefix.size()) == prefix);
            } else if (pattern.front() == '*' && pattern.size() > 1) {
              std::string suffix = pattern.substr(1);
              if (case_value.size() >= suffix.size()) {
                pattern_matches = (case_value.substr(case_value.size() -
                                                     suffix.size()) == suffix);
              }
            }
          }

          if (pattern_matches) {
            if (!command_part.empty()) {
              return execute_simple_or_pipeline(command_part);
            }
            return 0;
          }
        }
      }

      return 0;
    }

    try {
      std::vector<Command> cmds =
          shell_parser->parse_pipeline_with_preprocessing(text);

      bool has_redir_or_pipe = cmds.size() > 1;
      if (!has_redir_or_pipe && !cmds.empty()) {
        const auto& c = cmds[0];
        has_redir_or_pipe =
            c.background || !c.input_file.empty() || !c.output_file.empty() ||
            !c.append_file.empty() || c.stderr_to_stdout ||
            !c.stderr_file.empty() || !c.here_doc.empty() || c.both_output ||
            !c.here_string.empty() || !c.fd_redirections.empty() ||
            !c.fd_duplications.empty();
      }

      if (!has_redir_or_pipe && !cmds.empty()) {
        const auto& c = cmds[0];

        if (!c.args.empty() && c.args[0] == "__INTERNAL_SUBSHELL__") {
          bool has_redir = c.stderr_to_stdout || c.stdout_to_stderr ||
                           !c.input_file.empty() || !c.output_file.empty() ||
                           !c.append_file.empty() || !c.stderr_file.empty() ||
                           !c.here_doc.empty();

          if (g_debug_mode) {
            std::cerr << "DEBUG: INTERNAL_SUBSHELL has_redir=" << has_redir
                      << " stderr_to_stdout=" << c.stderr_to_stdout
                      << std::endl;
          }

          if (has_redir) {
            if (g_debug_mode) {
              std::cerr
                  << "DEBUG: Executing subshell with redirections via pipeline"
                  << std::endl;
            }
            int exit_code = g_shell->shell_exec->execute_pipeline(cmds);
            if (exit_code != 0) {
              // Only print errors for actual system errors, not simple command failures
              ErrorInfo error = g_shell->shell_exec->get_error();
              if (error.type != ErrorType::RUNTIME_ERROR || 
                  error.message.find("command failed with exit code") == std::string::npos) {
                g_shell->shell_exec->print_last_error();
              }
            }
            setenv("STATUS", std::to_string(exit_code).c_str(), 1);
            return exit_code;
          } else {
            if (c.args.size() >= 2) {
              std::string subshell_content = c.args[1];
              if (g_debug_mode) {
                std::cerr
                    << "DEBUG: Executing subshell content in child process: "
                    << subshell_content << std::endl;
              }

              pid_t pid = fork();
              if (pid == 0) {
                if (setpgid(0, 0) < 0) {
                  perror("cjsh: setpgid failed in subshell child");
                }

                int exit_code = g_shell->execute(subshell_content);

                int child_status;
                while (waitpid(-1, &child_status, WNOHANG) > 0) {
                }

                exit(exit_code);
              } else if (pid > 0) {
                int status;
                waitpid(pid, &status, 0);
                int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
                setenv("STATUS", std::to_string(exit_code).c_str(), 1);
                return exit_code;
              } else {
                std::cerr << "Failed to fork for subshell execution"
                          << std::endl;
                return 1;
              }
            } else {
              return 1;
            }
          }
        } else {
          std::vector<std::string> expanded_args =
              shell_parser->parse_command(text);
          if (expanded_args.empty())
            return 0;
          if (g_debug_mode) {
            std::cerr << "DEBUG: Simple exec: ";
            for (const auto& a : expanded_args)
              std::cerr << "[" << a << "]";
            if (c.background)
              std::cerr << " &";
            std::cerr << std::endl;
          }
          int exit_code = g_shell->execute_command(expanded_args, c.background);

          setenv("STATUS", std::to_string(exit_code).c_str(), 1);
          return exit_code;
        }
      }

      if (cmds.empty())
        return 0;
      if (g_debug_mode) {
        std::cerr << "DEBUG: Executing pipeline of size " << cmds.size()
                  << std::endl;
        for (size_t i = 0; i < cmds.size(); i++) {
          const auto& cmd = cmds[i];
          std::cerr << "DEBUG: Command " << i << ": ";
          for (const auto& arg : cmd.args) {
            std::cerr << "'" << arg << "' ";
          }
          std::cerr << " stderr_to_stdout=" << cmd.stderr_to_stdout
                    << std::endl;
        }
      }
      int exit_code = g_shell->shell_exec->execute_pipeline(cmds);
      if (exit_code != 0) {
        // Only print errors for actual system errors, not simple command failures
        ErrorInfo error = g_shell->shell_exec->get_error();
        if (error.type != ErrorType::RUNTIME_ERROR || 
            error.message.find("command failed with exit code") == std::string::npos) {
          g_shell->shell_exec->print_last_error();
        }
      }

      setenv("STATUS", std::to_string(exit_code).c_str(), 1);
      return exit_code;
    } catch (const std::bad_alloc& e) {
      std::vector<SyntaxError> errors;
      SyntaxError error(1, "Memory allocation failed", text);
      error.severity = ErrorSeverity::ERROR;
      error.category = ErrorCategory::COMMANDS;
      error.error_code = "MEM001";
      error.suggestion =
          "Command may be too complex or system is low on memory";
      errors.push_back(error);

      print_error_report(errors, true, true);

      setenv("STATUS", "3", 1);
      return 3;
    } catch (const std::system_error& e) {
      std::vector<SyntaxError> errors;
      SyntaxError error(1, "System error: " + std::string(e.what()), text);
      error.severity = ErrorSeverity::ERROR;
      error.category = ErrorCategory::COMMANDS;
      error.error_code = "SYS001";
      error.suggestion = "Check system resources and permissions";
      errors.push_back(error);

      print_error_report(errors, true, true);

      setenv("STATUS", "4", 1);
      return 4;
    } catch (const std::runtime_error& e) {
      std::vector<SyntaxError> errors;
      SyntaxError error(1, e.what(), text);
      std::string error_msg = e.what();

      // Determine error type based on message content
      if (error_msg.find("Unclosed quote") != std::string::npos ||
          error_msg.find("missing closing") != std::string::npos) {
        // Parsing/syntax error
        error.severity = ErrorSeverity::ERROR;
        error.category = ErrorCategory::SYNTAX;
        error.error_code = "SYN001";
        error.suggestion = "Make sure all quotes are properly closed";
      } else if (error_msg.find("Failed to open") != std::string::npos ||
                 error_msg.find("Failed to redirect") != std::string::npos ||
                 error_msg.find("Failed to write") != std::string::npos) {
        // I/O error
        error.severity = ErrorSeverity::ERROR;
        error.category = ErrorCategory::REDIRECTION;
        error.error_code = "IO001";
        error.suggestion = "Check file permissions and paths";
      } else {
        // Generic runtime error
        error.severity = ErrorSeverity::ERROR;
        error.category = ErrorCategory::COMMANDS;
        error.error_code = "RUN001";
        error.suggestion = "Check command syntax and system resources";
      }

      errors.push_back(error);
      print_error_report(errors, true, true);

      setenv("STATUS", "2", 1);
      return 2;
    } catch (const std::exception& e) {
      std::vector<SyntaxError> errors;
      SyntaxError error(1, "Unexpected error: " + std::string(e.what()), text);
      error.severity = ErrorSeverity::ERROR;
      error.category = ErrorCategory::COMMANDS;
      error.error_code = "UNK001";
      error.suggestion =
          "An unexpected error occurred, please report this as an issue, and "
          "how to replicate it.";
      errors.push_back(error);

      print_error_report(errors, true, true);

      setenv("STATUS", "5", 1);
      return 5;
    } catch (...) {
      std::vector<SyntaxError> errors;
      SyntaxError error(1, "Unknown error occurred", text);
      error.severity = ErrorSeverity::ERROR;
      error.category = ErrorCategory::COMMANDS;
      error.error_code = "UNK002";
      error.suggestion =
          "An unexpected error occurred, please report this as an issue, and "
          "how to replicate it.";
      errors.push_back(error);

      print_error_report(errors, true, true);

      setenv("STATUS", "6", 1);
      return 6;
    }
  };

  int last_code = 0;

  auto split_ampersand = [&](const std::string& s) -> std::vector<std::string> {
    std::vector<std::string> parts;
    bool in_quotes = false;
    char q = '\0';
    std::string cur;
    for (size_t i = 0; i < s.size(); ++i) {
      char c = s[i];
      if ((c == '"' || c == '\'') && (i == 0 || s[i - 1] != '\\')) {
        if (!in_quotes) {
          in_quotes = true;
          q = c;
        } else if (q == c) {
          in_quotes = false;
          q = '\0';
        }
        cur += c;
      } else if (!in_quotes && c == '&') {
        if (i + 1 < s.size() && s[i + 1] == '&') {
          cur += c;
        } else if (i > 0 && s[i - 1] == '>' && i + 1 < s.size() &&
                   std::isdigit(s[i + 1])) {
          cur += c;
        } else if (i + 1 < s.size() && s[i + 1] == '>') {
          cur += c;
        } else {
          std::string seg = trim(cur);
          if (!seg.empty() && seg.back() != '&')
            seg += " &";
          if (!seg.empty())
            parts.push_back(seg);
          cur.clear();
        }
      } else {
        cur += c;
      }
    }
    std::string tail = trim(cur);
    if (!tail.empty())
      parts.push_back(tail);
    return parts;
  };

  auto handle_if_block = [&](const std::vector<std::string>& src_lines,
                             size_t& idx) -> int {
    std::string first = trim(strip_inline_comment(src_lines[idx]));

    std::string cond_accum;
    if (first.rfind("if ", 0) == 0) {
      cond_accum = first.substr(3);
    } else if (first == "if") {
    } else {
      return 1;
    }

    size_t j = idx;
    bool then_found = false;

    auto pos = first.find("; then");
    if (pos != std::string::npos) {
      cond_accum = trim(first.substr(3, pos - 3));
      then_found = true;
    } else {
      while (!then_found && ++j < src_lines.size()) {
        std::string cur = trim(strip_inline_comment(src_lines[j]));
        if (cur == "then") {
          then_found = true;
          break;
        }
        auto p = cur.rfind("; then");
        if (p != std::string::npos) {
          if (!cond_accum.empty())
            cond_accum += " ";
          cond_accum += cur.substr(0, p);
          then_found = true;
          break;
        }
        if (!cur.empty()) {
          if (!cond_accum.empty())
            cond_accum += " ";
          cond_accum += cur;
        }
      }
    }

    if (!then_found) {
      if (debug_level >= DebugLevel::BASIC)
        std::cerr << "DEBUG: if without matching then" << std::endl;
      idx = j;
      return 1;
    }

    int cond_rc = 1;
    if (!cond_accum.empty()) {
      cond_rc = execute_simple_or_pipeline(cond_accum);
    }

    if (pos != std::string::npos) {
      std::string rem = trim(first.substr(pos + 6));

      if (!rem.empty()) {
        size_t fi_pos = rem.rfind("; fi");
        if (fi_pos == std::string::npos) {
          if (rem.size() >= 2 && rem.substr(rem.size() - 2) == "fi") {
            fi_pos = rem.size() - 2;
          }
        }

        if (rem.find("elif") != std::string::npos) {
        } else {
          if (fi_pos != std::string::npos) {
            std::string body = trim(rem.substr(0, fi_pos));

            std::string then_body = body;
            std::string else_body;
            size_t else_pos = body.find("; else");
            if (else_pos == std::string::npos) {
              size_t alt = body.find(" else ");
              if (alt != std::string::npos)
                else_pos = alt;
            }
            if (else_pos != std::string::npos) {
              then_body = trim(body.substr(0, else_pos));
              else_body = trim(body.substr(else_pos + 6));
            }
            int body_rc = 0;
            if (cond_rc == 0) {
              auto cmds = shell_parser->parse_semicolon_commands(then_body);
              for (const auto& c : cmds) {
                int rc2 = execute_simple_or_pipeline(c);
                body_rc = rc2;
                if (rc2 != 0)
                  break;
              }
            } else if (!else_body.empty()) {
              auto cmds = shell_parser->parse_semicolon_commands(else_body);
              for (const auto& c : cmds) {
                int rc2 = execute_simple_or_pipeline(c);
                body_rc = rc2;
                if (rc2 != 0)
                  break;
              }
            }

            return body_rc;
          }
        }
      }
    }

    size_t k = j + 1;
    int depth = 1;
    bool in_else = false;
    std::vector<std::string> then_lines;
    std::vector<std::string> else_lines;

    if (src_lines.size() == 1 && src_lines[0].find("fi") != std::string::npos) {
      std::string full_line = src_lines[0];

      std::vector<std::string> parts;

      size_t if_pos = full_line.find("if ");
      size_t then_pos = full_line.find("; then");
      if (if_pos != std::string::npos && then_pos != std::string::npos) {
        std::string condition =
            trim(full_line.substr(if_pos + 3, then_pos - (if_pos + 3)));

        int cond_result = execute_simple_or_pipeline(condition);

        std::string remaining = trim(full_line.substr(then_pos + 6));

        std::vector<std::pair<std::string, std::string>> branches;
        std::string current_commands;

        size_t pos = 0;
        bool condition_met = false;

        while (pos < remaining.length()) {
          size_t elif_pos = remaining.find("; elif ", pos);
          size_t else_pos = remaining.find("; else ", pos);

          size_t fi_pos = std::string::npos;
          size_t search_pos = pos;
          while (search_pos < remaining.length()) {
            size_t candidate = remaining.find("fi", search_pos);
            if (candidate == std::string::npos)
              break;

            bool is_word_end = (candidate + 2 >= remaining.length()) ||
                               (remaining[candidate + 2] == ' ') ||
                               (remaining[candidate + 2] == ';');
            bool is_word_start = (candidate == 0) ||
                                 (remaining[candidate - 1] == ' ') ||
                                 (remaining[candidate - 1] == ';');

            if (is_word_start && is_word_end) {
              fi_pos = candidate;
              break;
            }
            search_pos = candidate + 1;
          }

          size_t next_pos = std::min({elif_pos, else_pos, fi_pos});
          if (next_pos == std::string::npos)
            break;

          std::string commands = trim(remaining.substr(pos, next_pos - pos));

          if (elif_pos != std::string::npos && next_pos == elif_pos) {
            if (pos == 0) {
              if (cond_result == 0 && !condition_met) {
                auto cmds = shell_parser->parse_semicolon_commands(commands);
                for (const auto& c : cmds) {
                  execute_simple_or_pipeline(c);
                }
                condition_met = true;
                idx = 0;
                return 0;
              }
            }

            pos = next_pos + 7;
            size_t elif_then = remaining.find("; then", pos);
            if (elif_then != std::string::npos) {
              std::string elif_cond =
                  trim(remaining.substr(pos, elif_then - pos));
              int elif_result = execute_simple_or_pipeline(elif_cond);

              if (elif_result == 0 && !condition_met) {
                size_t elif_body_start = elif_then + 6;

                size_t next_elif = remaining.find("; elif ", elif_body_start);
                size_t next_else = remaining.find("; else ", elif_body_start);
                size_t next_fi = std::string::npos;
                size_t search_fi = elif_body_start;
                while (search_fi < remaining.length()) {
                  size_t candidate = remaining.find("fi", search_fi);
                  if (candidate == std::string::npos)
                    break;

                  bool is_word_end = (candidate + 2 >= remaining.length()) ||
                                     (remaining[candidate + 2] == ' ') ||
                                     (remaining[candidate + 2] == ';');
                  bool is_word_start = (candidate == 0) ||
                                       (remaining[candidate - 1] == ' ') ||
                                       (remaining[candidate - 1] == ';');

                  if (is_word_start && is_word_end) {
                    next_fi = candidate;
                    break;
                  }
                  search_fi = candidate + 1;
                }

                size_t elif_body_end =
                    std::min({next_elif, next_else, next_fi});
                if (elif_body_end != std::string::npos) {
                  std::string elif_commands = trim(remaining.substr(
                      elif_body_start, elif_body_end - elif_body_start));
                  auto cmds =
                      shell_parser->parse_semicolon_commands(elif_commands);
                  for (const auto& c : cmds) {
                    int rc = execute_simple_or_pipeline(c);

                    if (rc == 253 || rc == 254 || rc == 255) {
                      idx = 0;
                      return rc;
                    }
                  }
                  idx = 0;
                  return 0;
                }
              }
              pos = elif_then + 6;
            }
          } else if (else_pos != std::string::npos && next_pos == else_pos) {
            if (!condition_met && cond_result != 0) {
              pos = next_pos + 7;
              size_t fi_end = remaining.find(" fi", pos);
              if (fi_end != std::string::npos) {
                std::string else_commands =
                    trim(remaining.substr(pos, fi_end - pos));
                auto cmds =
                    shell_parser->parse_semicolon_commands(else_commands);
                for (const auto& c : cmds) {
                  execute_simple_or_pipeline(c);
                }
                idx = 0;
                return 0;
              }
            }
            break;
          } else {
            if (commands.length() > 0) {
              auto cmds = shell_parser->parse_semicolon_commands(commands);
              for (const auto& c : cmds) {
                execute_simple_or_pipeline(c);
              }
            }
            break;
          }
        }

        idx = 0;
        return 0;
      }
    }

    while (k < src_lines.size() && depth > 0) {
      std::string cur_raw = src_lines[k];
      std::string cur = trim(strip_inline_comment(cur_raw));
      if (cur == "if" || cur.rfind("if ", 0) == 0 ||
          cur.find("; then") != std::string::npos)
        depth++;
      else if (cur == "fi") {
        depth--;
        if (depth == 0)
          break;
      } else if (depth == 1 && cur == "else") {
        in_else = true;
        k++;
        continue;
      }

      if (depth > 0) {
        if (!in_else)
          then_lines.push_back(cur_raw);
        else
          else_lines.push_back(cur_raw);
      }
      k++;
    }
    if (depth != 0) {
      if (debug_level >= DebugLevel::BASIC)
        std::cerr << "DEBUG: if without matching fi" << std::endl;
      idx = k;
      return 1;
    }

    int body_rc = 0;
    if (cond_rc == 0)
      body_rc = execute_block(then_lines);
    else if (!else_lines.empty())
      body_rc = execute_block(else_lines);

    idx = k;
    return body_rc;
  };

  auto handle_for_block = [&](const std::vector<std::string>& src_lines,
                              size_t& idx) -> int {
    std::string first = trim(strip_inline_comment(src_lines[idx]));
    if (!(first == "for" || first.rfind("for ", 0) == 0))
      return 1;

    std::string var;
    std::vector<std::string> items;

    auto parse_header = [&](const std::string& header) -> bool {
      std::vector<std::string> toks = shell_parser->parse_command(header);
      size_t i = 0;
      if (i < toks.size() && toks[i] == "for")
        ++i;
      if (i >= toks.size())
        return false;
      var = toks[i++];
      if (i < toks.size() && toks[i] == "in") {
        ++i;
        for (; i < toks.size(); ++i) {
          if (toks[i] == ";" || toks[i] == "do")
            break;
          items.push_back(toks[i]);
        }
      }
      return !var.empty();
    };

    if (first.find("; do") != std::string::npos &&
        first.find("done") != std::string::npos) {
      size_t do_pos = first.find("; do");
      std::string header = trim(first.substr(0, do_pos));
      if (!parse_header(header))
        return 1;
      std::string tail = trim(first.substr(do_pos + 4));
      size_t done_pos = tail.rfind("; done");
      if (done_pos == std::string::npos)
        done_pos = tail.rfind("done");
      std::string body =
          done_pos == std::string::npos ? tail : trim(tail.substr(0, done_pos));
      int rc = 0;
      for (const auto& it : items) {
        setenv(var.c_str(), it.c_str(), 1);
        auto cmds = shell_parser->parse_semicolon_commands(body);
        for (const auto& c : cmds) {
          rc = execute_simple_or_pipeline(c);
          if (rc == 255) {
            const char* break_level_str = getenv("CJSH_BREAK_LEVEL");
            int break_level = 1;
            if (break_level_str) {
              break_level = std::stoi(break_level_str);
              unsetenv("CJSH_BREAK_LEVEL");
            }
            if (break_level == 1) {
              rc = 0;
              goto for_loop_break;
            } else {
              setenv("CJSH_BREAK_LEVEL",
                     std::to_string(break_level - 1).c_str(), 1);
              goto for_loop_break;
            }
          } else if (rc == 254) {
            const char* continue_level_str = getenv("CJSH_CONTINUE_LEVEL");
            int continue_level = 1;
            if (continue_level_str) {
              continue_level = std::stoi(continue_level_str);
              unsetenv("CJSH_CONTINUE_LEVEL");
            }
            if (continue_level == 1) {
              rc = 0;
              goto for_loop_continue;
            } else {
              setenv("CJSH_CONTINUE_LEVEL",
                     std::to_string(continue_level - 1).c_str(), 1);
              goto for_loop_break;
            }
          } else if (rc != 0) {
            if (g_shell && g_shell->is_errexit_enabled()) {
              break;
            }

            break;
          }
        }
      for_loop_continue:;
      }
    for_loop_break:;
      return rc;
    }

    std::string header_accum = first;
    size_t j = idx;
    bool have_do = false;

    if (first.find("; do") != std::string::npos &&
        first.rfind("; do") == first.length() - 4) {
      have_do = true;
      header_accum = first.substr(0, first.length() - 4);
    } else {
      while (!have_do && ++j < src_lines.size()) {
        std::string cur = trim(strip_inline_comment(src_lines[j]));
        if (cur == "do") {
          have_do = true;
          break;
        }
        if (cur.find("; do") != std::string::npos) {
          header_accum += " ";
          header_accum += cur;
          have_do = true;
          break;
        }
        if (!cur.empty()) {
          header_accum += " ";
          header_accum += cur;
        }
      }
    }
    if (!parse_header(header_accum)) {
      idx = j;
      return 1;
    }
    if (!have_do) {
      idx = j;
      return 1;
    }

    std::vector<std::string> body_lines;
    size_t k = j + 1;
    int depth = 1;
    while (k < src_lines.size() && depth > 0) {
      std::string cur_raw = src_lines[k];
      std::string cur = trim(strip_inline_comment(cur_raw));
      if (cur == "for" || cur.rfind("for ", 0) == 0)
        depth++;
      else if (cur == "done") {
        depth--;
        if (depth == 0)
          break;
      }
      if (depth > 0)
        body_lines.push_back(cur_raw);
      k++;
    }
    if (depth != 0) {
      idx = k;
      return 1;
    }

    int rc = 0;
    for (const auto& it : items) {
      setenv(var.c_str(), it.c_str(), 1);
      rc = execute_block(body_lines);
      if (rc == 255) {
        const char* break_level_str = getenv("CJSH_BREAK_LEVEL");
        int break_level = 1;
        if (break_level_str) {
          break_level = std::stoi(break_level_str);
          unsetenv("CJSH_BREAK_LEVEL");
        }
        if (break_level == 1) {
          rc = 0;
          break;
        } else {
          setenv("CJSH_BREAK_LEVEL", std::to_string(break_level - 1).c_str(),
                 1);
          break;
        }
      } else if (rc == 254) {
        const char* continue_level_str = getenv("CJSH_CONTINUE_LEVEL");
        int continue_level = 1;
        if (continue_level_str) {
          continue_level = std::stoi(continue_level_str);
          unsetenv("CJSH_CONTINUE_LEVEL");
        }
        if (continue_level == 1) {
          rc = 0;
          continue;
        } else {
          setenv("CJSH_CONTINUE_LEVEL",
                 std::to_string(continue_level - 1).c_str(), 1);
          break;
        }
      } else if (rc != 0) {
        if (g_shell && g_shell->is_errexit_enabled()) {
          break;
        }

        continue;
      }
    }
    idx = k;  // Point to 'done' line, will be incremented in main loop
    return rc;
  };

  auto handle_case_block = [&](const std::vector<std::string>& src_lines,
                               size_t& idx) -> int {
    std::string first = trim(strip_inline_comment(src_lines[idx]));
    if (!(first == "case" || first.rfind("case ", 0) == 0))
      return 1;

    std::string case_value;
    std::string header_accum = first;

    if (first.find(" in ") != std::string::npos &&
        first.find("esac") != std::string::npos) {
      size_t in_pos = first.find(" in ");
      std::string case_part = first.substr(0, in_pos);
      std::string patterns_part = first.substr(in_pos + 4);

      std::vector<std::string> case_toks =
          shell_parser->parse_command(case_part);
      if (case_toks.size() >= 2 && case_toks[0] == "case") {
        case_value = case_toks[1];
      }

      size_t esac_pos = patterns_part.find("esac");
      if (esac_pos != std::string::npos) {
        patterns_part = patterns_part.substr(0, esac_pos);
      }

      std::vector<std::string> pattern_sections;
      size_t start = 0;
      while (start < patterns_part.length()) {
        size_t sep_pos = patterns_part.find(";;", start);
        if (sep_pos == std::string::npos) {
          if (start < patterns_part.length()) {
            pattern_sections.push_back(trim(patterns_part.substr(start)));
          }
          break;
        }
        pattern_sections.push_back(
            trim(patterns_part.substr(start, sep_pos - start)));
        start = sep_pos + 2;
      }

      int matched_exit_code = 0;
      for (const auto& section : pattern_sections) {
        if (section.empty())
          continue;

        size_t paren_pos = section.find(')');
        if (paren_pos != std::string::npos) {
          std::string pattern = trim(section.substr(0, paren_pos));
          std::string command_part = trim(section.substr(paren_pos + 1));

          bool pattern_matches = false;
          if (pattern == "*") {
            pattern_matches = true;
          } else if (pattern == case_value) {
            pattern_matches = true;
          }

          if (pattern_matches) {
            if (!command_part.empty()) {
              matched_exit_code = execute_simple_or_pipeline(command_part);
            }

            return matched_exit_code;
          }
        }
      }

      return 0;
    }

    std::vector<std::string> header_toks =
        shell_parser->parse_command(header_accum);
    size_t tok_idx = 0;
    if (tok_idx < header_toks.size() && header_toks[tok_idx] == "case")
      ++tok_idx;
    if (tok_idx < header_toks.size()) {
      std::string raw_case_value = header_toks[tok_idx++];
      // Expand variables in case value
      shell_parser->expand_env_vars(raw_case_value);
      case_value = raw_case_value;
    }

    bool found_in = false;
    if (tok_idx < header_toks.size() && header_toks[tok_idx] == "in") {
      found_in = true;
    }

    size_t j = idx;
    if (!found_in) {
      while (!found_in && ++j < src_lines.size()) {
        std::string cur = trim(strip_inline_comment(src_lines[j]));
        if (cur == "in") {
          found_in = true;
          break;
        }
        if (!cur.empty()) {
          header_accum += " " + cur;

          header_toks = shell_parser->parse_command(header_accum);
          for (size_t h = 0; h < header_toks.size(); ++h) {
            if (header_toks[h] == "in") {
              found_in = true;
              break;
            }
          }
        }
      }
    }

    if (!found_in || case_value.empty()) {
      if (debug_level >= DebugLevel::BASIC)
        std::cerr << "DEBUG: case without valid syntax" << std::endl;
      idx = j;
      return 1;
    }

    size_t k = j + 1;
    int matched_exit_code = 0;
    bool found_match = false;

    if (k < src_lines.size()) {
      std::string remaining_line = src_lines[k];

      if (remaining_line.find("esac") != std::string::npos) {
        std::string work_line = remaining_line;
        size_t esac_pos = work_line.find("esac");
        if (esac_pos != std::string::npos) {
          work_line = work_line.substr(0, esac_pos);
        }

        std::vector<std::string> pattern_sections;
        size_t start = 0;
        while (start < work_line.length()) {
          size_t sep_pos = work_line.find(";;", start);
          if (sep_pos == std::string::npos) {
            if (start < work_line.length()) {
              pattern_sections.push_back(work_line.substr(start));
            }
            break;
          }
          pattern_sections.push_back(work_line.substr(start, sep_pos - start));
          start = sep_pos + 2;
        }

        for (const auto& section : pattern_sections) {
          std::string pattern_line = trim(section);
          if (pattern_line.empty())
            continue;

          size_t paren_pos = pattern_line.find(')');
          if (paren_pos != std::string::npos) {
            std::string pattern = trim(pattern_line.substr(0, paren_pos));
            std::string command_part = trim(pattern_line.substr(paren_pos + 1));

            bool pattern_matches = false;
            if (pattern == "*") {
              pattern_matches = true;
            } else if (pattern == case_value) {
              pattern_matches = true;
            } else {
              if (pattern.find('*') != std::string::npos) {
                if (pattern.back() == '*') {
                  std::string prefix = pattern.substr(0, pattern.length() - 1);
                  if (case_value.substr(0, prefix.length()) == prefix) {
                    pattern_matches = true;
                  }
                } else if (pattern.front() == '*') {
                  std::string suffix = pattern.substr(1);
                  if (case_value.length() >= suffix.length() &&
                      case_value.substr(case_value.length() -
                                        suffix.length()) == suffix) {
                    pattern_matches = true;
                  }
                }
              }
            }

            if (pattern_matches && !found_match) {
              found_match = true;
              if (!command_part.empty()) {
                matched_exit_code = execute_simple_or_pipeline(command_part);
              }
              break;
            }
          }
        }

        idx = k;
        return matched_exit_code;
      }
    }

    while (k < src_lines.size()) {
      std::string line = trim(strip_inline_comment(src_lines[k]));
      if (line.empty()) {
        k++;
        continue;
      }
      if (line == "esac") {
        break;
      }

      size_t paren_pos = line.find(')');
      if (paren_pos != std::string::npos) {
        std::string pattern = trim(line.substr(0, paren_pos));
        std::string command_part = trim(line.substr(paren_pos + 1));

        // Strip trailing ;; from command_part
        if (command_part.length() >= 2 &&
            command_part.substr(command_part.length() - 2) == ";;") {
          command_part =
              trim(command_part.substr(0, command_part.length() - 2));
        }

        bool pattern_matches = false;
        if (pattern == "*") {
          pattern_matches = true;
        } else if (pattern == case_value) {
          pattern_matches = true;
        } else {
          if (pattern.find('*') != std::string::npos) {
            if (pattern.back() == '*') {
              std::string prefix = pattern.substr(0, pattern.length() - 1);
              if (case_value.substr(0, prefix.length()) == prefix) {
                pattern_matches = true;
              }
            } else if (pattern.front() == '*') {
              std::string suffix = pattern.substr(1);
              if (case_value.length() >= suffix.length() &&
                  case_value.substr(case_value.length() - suffix.length()) ==
                      suffix) {
                pattern_matches = true;
              }
            }
          }
        }

        if (pattern_matches && !found_match) {
          found_match = true;

          std::vector<std::string> case_commands;
          if (!command_part.empty()) {
            case_commands.push_back(command_part);
          }

          // Check if the current line already contains ;; (inline case)
          bool inline_case = line.find(";;") != std::string::npos;

          if (!inline_case) {
            k++;
            while (k < src_lines.size()) {
              std::string cmd_line = trim(strip_inline_comment(src_lines[k]));
              if (cmd_line.empty()) {
                k++;
                continue;
              }
              if (cmd_line == "esac") {
                break;
              }
              if (cmd_line == ";;" ||
                  cmd_line.find(";;") != std::string::npos) {
                if (cmd_line != ";;") {
                  size_t sep_pos = cmd_line.find(";;");
                  std::string before_sep = trim(cmd_line.substr(0, sep_pos));
                  if (!before_sep.empty()) {
                    case_commands.push_back(before_sep);
                  }
                }
                break;
              }

              if (cmd_line.find(')') != std::string::npos) {
                k--;
                break;
              }
              case_commands.push_back(cmd_line);
              k++;
            }
          }

          for (const auto& cmd : case_commands) {
            matched_exit_code = execute_simple_or_pipeline(cmd);
            if (matched_exit_code != 0)
              break;
          }

          // Found a match, now scan forward to esac
          while (k < src_lines.size()) {
            std::string scan_line = trim(strip_inline_comment(src_lines[k]));
            if (scan_line == "esac") {
              break;
            }
            k++;
          }
          idx = k;  // Point to esac line, will be incremented in main loop
          return matched_exit_code;
        }
      }
      k++;
    }

    while (k < src_lines.size()) {
      std::string line = trim(strip_inline_comment(src_lines[k]));
      if (line == "esac") {
        break;
      }
      k++;
    }

    idx = k;  // Point to esac line, will be incremented in main loop
    return matched_exit_code;
  };

  auto handle_while_block = [&](const std::vector<std::string>& src_lines,
                                size_t& idx) -> int {
    std::string first = trim(strip_inline_comment(src_lines[idx]));
    if (!(first == "while" || first.rfind("while ", 0) == 0))
      return 1;

    auto parse_cond_from = [&](const std::string& s, std::string& cond,
                               bool& inline_do, std::string& body_inline) {
      inline_do = false;
      body_inline.clear();
      cond.clear();
      std::string tmp = s;
      if (tmp == "while") {
        return true;
      }
      if (tmp.rfind("while ", 0) == 0)
        tmp = tmp.substr(6);
      size_t do_pos = tmp.find("; do");
      if (do_pos != std::string::npos) {
        cond = trim(tmp.substr(0, do_pos));
        inline_do = true;
        body_inline = trim(tmp.substr(do_pos + 4));
        return true;
      }
      if (tmp == "do") {
        inline_do = true;
        return true;
      }
      cond = trim(tmp);
      return true;
    };

    std::string cond;
    bool inline_do = false;
    std::string body_inline;
    parse_cond_from(first, cond, inline_do, body_inline);
    size_t j = idx;
    if (!inline_do) {
      while (++j < src_lines.size()) {
        std::string cur = trim(strip_inline_comment(src_lines[j]));
        if (cur == "do") {
          inline_do = true;
          break;
        }
        if (cur.find("; do") != std::string::npos) {
          parse_cond_from(cur, cond, inline_do, body_inline);
          break;
        }
        if (!cur.empty()) {
          if (!cond.empty())
            cond += " ";
          cond += cur;
        }
      }
    }
    if (!inline_do) {
      idx = j;
      return 1;
    }

    std::vector<std::string> body_lines;
    if (!body_inline.empty()) {
      std::string bi = body_inline;
      size_t done_pos = bi.rfind("; done");
      if (done_pos != std::string::npos)
        bi = trim(bi.substr(0, done_pos));
      body_lines = shell_parser->parse_into_lines(bi);
    } else {
      size_t k = j + 1;
      int depth = 1;
      while (k < src_lines.size() && depth > 0) {
        std::string cur_raw = src_lines[k];
        std::string cur = trim(strip_inline_comment(cur_raw));
        if (cur == "while" || cur.rfind("while ", 0) == 0)
          depth++;
        else if (cur == "done") {
          depth--;
          if (depth == 0)
            break;
        }
        if (depth > 0)
          body_lines.push_back(cur_raw);
        k++;
      }
      if (depth != 0) {
        idx = k;
        return 1;
      }
      idx = k;  // Point to 'done' line, will be incremented in main loop
    }

    int rc = 0;
    int guard = 0;
    const int GUARD_MAX = 100000;
    while (true) {
      int c = 0;
      if (!cond.empty()) {
        c = execute_simple_or_pipeline(cond);
      }
      if (c != 0)
        break;
      rc = execute_block(body_lines);
      if (rc == 255) {
        const char* break_level_str = getenv("CJSH_BREAK_LEVEL");
        int break_level = 1;
        if (break_level_str) {
          break_level = std::stoi(break_level_str);
          unsetenv("CJSH_BREAK_LEVEL");
        }
        if (break_level == 1) {
          rc = 0;
          break;
        } else {
          setenv("CJSH_BREAK_LEVEL", std::to_string(break_level - 1).c_str(),
                 1);
          break;
        }
      } else if (rc == 254) {
        const char* continue_level_str = getenv("CJSH_CONTINUE_LEVEL");
        int continue_level = 1;
        if (continue_level_str) {
          continue_level = std::stoi(continue_level_str);
          unsetenv("CJSH_CONTINUE_LEVEL");
        }
        if (continue_level == 1) {
          rc = 0;
          continue;
        } else {
          setenv("CJSH_CONTINUE_LEVEL",
                 std::to_string(continue_level - 1).c_str(), 1);
          break;
        }
      } else if (rc != 0) {
        if (g_shell && g_shell->is_errexit_enabled()) {
          break;
        }
      }
      if (++guard > GUARD_MAX) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "",
                     "while loop exceeded max iterations (guard)",
                     {}});
        rc = 1;
        break;
      }
    }
    return rc;
  };

  auto handle_until_block = [&](const std::vector<std::string>& src_lines,
                                size_t& idx) -> int {
    std::string first = trim(strip_inline_comment(src_lines[idx]));
    if (!(first == "until" || first.rfind("until ", 0) == 0))
      return 1;

    auto parse_cond_from = [&](const std::string& s, std::string& cond,
                               bool& inline_do, std::string& body_inline) {
      inline_do = false;
      body_inline.clear();
      cond.clear();
      std::string tmp = s;
      if (tmp == "until") {
        return true;
      }
      if (tmp.rfind("until ", 0) == 0)
        tmp = tmp.substr(6);
      size_t do_pos = tmp.find("; do");
      if (do_pos != std::string::npos) {
        cond = trim(tmp.substr(0, do_pos));
        inline_do = true;
        body_inline = trim(tmp.substr(do_pos + 4));
        return true;
      }
      if (tmp == "do") {
        inline_do = true;
        return true;
      }
      cond = trim(tmp);
      return true;
    };

    std::string cond;
    bool inline_do = false;
    std::string body_inline;
    parse_cond_from(first, cond, inline_do, body_inline);
    size_t j = idx;
    if (!inline_do) {
      while (++j < src_lines.size()) {
        std::string cur = trim(strip_inline_comment(src_lines[j]));
        if (cur == "do") {
          inline_do = true;
          break;
        }
        if (cur.find("; do") != std::string::npos) {
          parse_cond_from(cur, cond, inline_do, body_inline);
          break;
        }
        if (!cur.empty()) {
          if (!cond.empty())
            cond += " ";
          cond += cur;
        }
      }
    }
    if (!inline_do) {
      idx = j;
      return 1;
    }

    std::vector<std::string> body_lines;
    if (!body_inline.empty()) {
      std::string bi = body_inline;
      size_t done_pos = bi.rfind("; done");
      if (done_pos != std::string::npos)
        bi = trim(bi.substr(0, done_pos));
      body_lines = shell_parser->parse_into_lines(bi);
    } else {
      size_t k = j + 1;
      int depth = 1;
      while (k < src_lines.size() && depth > 0) {
        std::string cur_raw = src_lines[k];
        std::string cur = trim(strip_inline_comment(cur_raw));
        if (cur == "until" || cur.rfind("until ", 0) == 0)
          depth++;
        else if (cur == "done") {
          depth--;
          if (depth == 0)
            break;
        }
        if (depth > 0)
          body_lines.push_back(cur_raw);
        k++;
      }
      if (depth != 0) {
        idx = k;
        return 1;
      }
      idx = k;  // Point to 'done' line, will be incremented in main loop
    }

    int rc = 0;
    int guard = 0;
    const int GUARD_MAX = 100000;
    while (true) {
      int c = 0;
      if (!cond.empty()) {
        c = execute_simple_or_pipeline(cond);
      }

      if (c == 0)
        break;
      rc = execute_block(body_lines);
      if (rc == 255) {
        const char* break_level_str = getenv("CJSH_BREAK_LEVEL");
        int break_level = 1;
        if (break_level_str) {
          break_level = std::stoi(break_level_str);
          unsetenv("CJSH_BREAK_LEVEL");
        }
        if (break_level == 1) {
          rc = 0;
          break;
        } else {
          setenv("CJSH_BREAK_LEVEL", std::to_string(break_level - 1).c_str(),
                 1);
          break;
        }
      } else if (rc == 254) {
        const char* continue_level_str = getenv("CJSH_CONTINUE_LEVEL");
        int continue_level = 1;
        if (continue_level_str) {
          continue_level = std::stoi(continue_level_str);
          unsetenv("CJSH_CONTINUE_LEVEL");
        }
        if (continue_level == 1) {
          rc = 0;
          continue;
        } else {
          setenv("CJSH_CONTINUE_LEVEL",
                 std::to_string(continue_level - 1).c_str(), 1);
          break;
        }
      } else if (rc != 0) {
        if (g_shell && g_shell->is_errexit_enabled()) {
          break;
        }
      }
      if (++guard > GUARD_MAX) {
        std::cerr << "cjsh: until loop aborted (guard)" << std::endl;
        rc = 1;
        break;
      }
    }
    return rc;
  };

  for (size_t line_index = 0; line_index < lines.size(); ++line_index) {
    const auto& raw_line = lines[line_index];
    std::string line = trim(strip_inline_comment(raw_line));
    if (line.empty()) {
      if (g_debug_mode)
        std::cerr << "DEBUG: skipping empty/comment line" << std::endl;
      continue;
    }

    if (line == "fi" || line == "then" || line == "else" || line == "done" ||
        line == "esac" || line == "}" || line == ";;") {
      if (g_debug_mode)
        std::cerr << "DEBUG: ignoring orphaned control structure keyword: "
                  << line << std::endl;
      continue;
    }

    if (line == "if" || line.rfind("if ", 0) == 0) {
      int rc = handle_if_block(lines, line_index);
      last_code = rc;
      if (g_debug_mode)
        std::cerr << "DEBUG: if block completed with exit code: " << rc
                  << std::endl;

      continue;
    }

    if (line == "for" || line.rfind("for ", 0) == 0) {
      int rc = handle_for_block(lines, line_index);
      last_code = rc;
      if (g_debug_mode)
        std::cerr << "DEBUG: for block completed with exit code: " << rc
                  << std::endl;
      continue;
    }

    if (line == "while" || line.rfind("while ", 0) == 0) {
      int rc = handle_while_block(lines, line_index);
      last_code = rc;
      if (g_debug_mode)
        std::cerr << "DEBUG: while block completed with exit code: " << rc
                  << std::endl;
      continue;
    }

    if (line == "until" || line.rfind("until ", 0) == 0) {
      int rc = handle_until_block(lines, line_index);
      last_code = rc;
      if (g_debug_mode)
        std::cerr << "DEBUG: until block completed with exit code: " << rc
                  << std::endl;
      continue;
    }

    if (line == "case" || line.rfind("case ", 0) == 0) {
      int rc = handle_case_block(lines, line_index);
      last_code = rc;
      if (g_debug_mode)
        std::cerr << "DEBUG: case block completed with exit code: " << rc
                  << std::endl;
      continue;
    }

    if (line.find("()") != std::string::npos &&
        line.find("{") != std::string::npos) {
      size_t name_end = line.find("()");
      size_t brace_pos = line.find("{");
      if (name_end != std::string::npos && brace_pos != std::string::npos &&
          name_end < brace_pos) {
        std::string func_name = trim(line.substr(0, name_end));
        if (!func_name.empty() && func_name.find(' ') == std::string::npos) {
          std::vector<std::string> body_lines;
          bool handled_single_line = false;
          std::string after_brace = trim(line.substr(brace_pos + 1));
          if (!after_brace.empty()) {
            size_t end_brace = after_brace.find('}');
            if (end_brace != std::string::npos) {
              std::string body_part = trim(after_brace.substr(0, end_brace));
              if (!body_part.empty())
                body_lines.push_back(body_part);

              functions[func_name] = body_lines;
              if (g_debug_mode)
                std::cerr << "DEBUG: Defined function '" << func_name
                          << "' (single-line)" << std::endl;

              std::string remainder = trim(after_brace.substr(end_brace + 1));
              if (!remainder.empty()) {
                line = remainder;
              }
              handled_single_line = true;
            } else if (!after_brace.empty()) {
              body_lines.push_back(after_brace);
            }
          }
          if (!handled_single_line) {
            int depth = 1;
            while (++line_index < lines.size() && depth > 0) {
              std::string func_line_raw = lines[line_index];
              std::string func_line = trim(strip_inline_comment(func_line_raw));
              for (char ch : func_line) {
                if (ch == '{')
                  depth++;
                else if (ch == '}')
                  depth--;
              }
              if (depth <= 0) {
                size_t pos = func_line.find('}');
                if (pos != std::string::npos) {
                  std::string before = trim(func_line.substr(0, pos));
                  if (!before.empty())
                    body_lines.push_back(before);
                }
                break;
              } else if (!func_line.empty()) {
                body_lines.push_back(func_line_raw);
              }
            }
            functions[func_name] = body_lines;
            if (g_debug_mode)
              std::cerr << "DEBUG: Defined function '" << func_name << "' with "
                        << body_lines.size() << " lines" << std::endl;
            continue;
          } else {
          }
        }
      }
    }

    std::vector<LogicalCommand> lcmds =
        shell_parser->parse_logical_commands(line);
    if (lcmds.empty())
      continue;

    last_code = 0;
    for (size_t i = 0; i < lcmds.size(); ++i) {
      const auto& lc = lcmds[i];

      if (i > 0) {
        const std::string& prev_op = lcmds[i - 1].op;

        bool is_control_flow =
            (last_code == 253 || last_code == 254 || last_code == 255);
        if (prev_op == "&&" && last_code != 0 && !is_control_flow) {
          if (g_debug_mode)
            std::cerr << "DEBUG: Skipping due to && short-circuit" << std::endl;
          continue;
        }
        if (prev_op == "||" && last_code == 0) {
          if (g_debug_mode)
            std::cerr << "DEBUG: Skipping due to || short-circuit" << std::endl;
          continue;
        }

        if (is_control_flow) {
          if (g_debug_mode)
            std::cerr << "DEBUG: Breaking logical command sequence due to "
                         "control flow: "
                      << last_code << std::endl;
          break;
        }
      }

      std::string cmd_to_parse = lc.command;
      std::string trimmed_cmd = trim(strip_inline_comment(cmd_to_parse));

      if (!trimmed_cmd.empty() &&
          (trimmed_cmd[0] == '(' || trimmed_cmd[0] == '{')) {
        int code = execute_simple_or_pipeline(cmd_to_parse);
        last_code = code;
        if (code != 0 && debug_level >= DebugLevel::BASIC) {
          std::cerr << "DEBUG: Grouped command failed (" << code << ") -> '"
                    << cmd_to_parse << "'" << std::endl;
        }
        continue;
      }

      if ((trimmed_cmd == "if" || trimmed_cmd.rfind("if ", 0) == 0) &&
          (trimmed_cmd.find("; then") != std::string::npos) &&
          (trimmed_cmd.find(" fi") != std::string::npos ||
           trimmed_cmd.find("; fi") != std::string::npos ||
           trimmed_cmd.rfind("fi") == trimmed_cmd.length() - 2)) {
        size_t local_idx = 0;
        std::vector<std::string> one{trimmed_cmd};
        int code = handle_if_block(one, local_idx);
        last_code = code;
        if (code != 0 && debug_level >= DebugLevel::BASIC) {
          std::cerr << "DEBUG: if block failed (" << code << ") -> '"
                    << trimmed_cmd << "'" << std::endl;
        }
        continue;
      }

      if ((trimmed_cmd == "case" || trimmed_cmd.rfind("case ", 0) == 0) &&
          (trimmed_cmd.find(" in ") != std::string::npos) &&
          (trimmed_cmd.find("esac") != std::string::npos)) {
        if (g_debug_mode) {
          std::cerr << "DEBUG: Detected inline case statement: " << trimmed_cmd
                    << std::endl;
        }
        size_t local_idx = 0;
        std::vector<std::string> one{trimmed_cmd};
        int code = handle_case_block(one, local_idx);
        last_code = code;
        if (code != 0 && debug_level >= DebugLevel::BASIC) {
          std::cerr << "DEBUG: case block failed (" << code << ") -> '"
                    << trimmed_cmd << "'" << std::endl;
        }
        continue;
      }

      auto semis = shell_parser->parse_semicolon_commands(lc.command);
      if (semis.empty()) {
        last_code = 0;
        continue;
      }
      for (size_t k = 0; k < semis.size(); ++k) {
        const std::string& semi = semis[k];
        auto segs = split_ampersand(semi);
        if (segs.empty())
          segs.push_back(semi);
        for (size_t si = 0; si < segs.size(); ++si) {
          const std::string& cmd_text = segs[si];

          std::string t = trim(strip_inline_comment(cmd_text));

          if ((t.rfind("for ", 0) == 0 || t == "for") &&
              t.find("; do") != std::string::npos) {
            size_t local_idx = 0;
            std::vector<std::string> one{t};
            int code = handle_for_block(one, local_idx);
            last_code = code;
            if (code != 0 && debug_level >= DebugLevel::BASIC) {
              std::cerr << "DEBUG: for block failed (" << code << ") -> '" << t
                        << "'" << std::endl;
            }
            continue;
          }
          if ((t.rfind("while ", 0) == 0 || t == "while") &&
              t.find("; do") != std::string::npos) {
            size_t local_idx = 0;
            std::vector<std::string> one{t};
            int code = handle_while_block(one, local_idx);
            last_code = code;
            if (code != 0 && debug_level >= DebugLevel::BASIC) {
              std::cerr << "DEBUG: while block failed (" << code << ") -> '"
                        << t << "'" << std::endl;
            }
            continue;
          }
          if ((t.rfind("until ", 0) == 0 || t == "until") &&
              t.find("; do") != std::string::npos) {
            size_t local_idx = 0;
            std::vector<std::string> one{t};
            int code = handle_until_block(one, local_idx);
            last_code = code;
            if (code != 0 && debug_level >= DebugLevel::BASIC) {
              std::cerr << "DEBUG: until block failed (" << code << ") -> '"
                        << t << "'" << std::endl;
            }
            continue;
          }

          if ((t.rfind("if ", 0) == 0 || t == "if") &&
              t.find("; then") != std::string::npos &&
              t.find(" fi") != std::string::npos) {
            size_t local_idx = 0;
            std::vector<std::string> one{t};
            int code = handle_if_block(one, local_idx);
            last_code = code;
            if (code != 0 && debug_level >= DebugLevel::BASIC) {
              std::cerr << "DEBUG: if block failed (" << code << ") -> '" << t
                        << "'" << std::endl;
            }
            continue;
          }

          if ((t.rfind("for ", 0) == 0 || t == "for")) {
            std::string var;
            std::vector<std::string> items;
            auto parse_for_header = [&](const std::string& header) -> bool {
              std::vector<std::string> toks =
                  shell_parser->parse_command(header);
              size_t i = 0;
              if (i < toks.size() && toks[i] == "for")
                ++i;
              if (i >= toks.size())
                return false;
              var = toks[i++];
              if (i < toks.size() && toks[i] == "in") {
                ++i;
                for (; i < toks.size(); ++i)
                  items.push_back(toks[i]);
              }
              return !var.empty();
            };
            if (!parse_for_header(t)) {
              last_code = 1;
              break;
            }

            std::string body_inline;
            size_t done_index = k + 1;
            if (done_index < semis.size()) {
              std::string next = trim(strip_inline_comment(semis[done_index]));
              if (next.rfind("do ", 0) == 0)
                body_inline = trim(next.substr(3));
              else if (next == "do")
                body_inline = "";
              else {
                last_code = 1;
                break;
              }

              size_t scan = done_index + 1;
              bool found_done = false;
              for (; scan < semis.size(); ++scan) {
                std::string seg = trim(strip_inline_comment(semis[scan]));
                if (seg == "done") {
                  found_done = true;
                  break;
                }

                if (!body_inline.empty())
                  body_inline += "; ";
                body_inline += seg;
              }
              if (!found_done) {
                last_code = 1;
                break;
              }

              int rc2 = 0;
              for (const auto& itv : items) {
                setenv(var.c_str(), itv.c_str(), 1);
                auto cmds2 =
                    shell_parser->parse_semicolon_commands(body_inline);
                for (const auto& c2 : cmds2) {
                  rc2 = execute_simple_or_pipeline(c2);
                  if (rc2 != 0)
                    break;
                }
                if (rc2 != 0)
                  break;
              }
              last_code = rc2;

              k = scan;

              break;
            }
          }
          if ((t.rfind("while ", 0) == 0 || t == "while")) {
            std::string cond =
                (t == "while") ? std::string("") : trim(t.substr(6));

            size_t done_index = k + 1;
            std::string body_inline;
            if (done_index < semis.size()) {
              std::string next = trim(strip_inline_comment(semis[done_index]));
              if (next.rfind("do ", 0) == 0)
                body_inline = trim(next.substr(3));
              else if (next == "do")
                body_inline.clear();
              else {
                last_code = 1;
                break;
              }

              size_t scan = done_index + 1;
              bool found_done = false;
              for (; scan < semis.size(); ++scan) {
                std::string seg = trim(strip_inline_comment(semis[scan]));
                if (seg == "done") {
                  found_done = true;
                  break;
                }
                if (!body_inline.empty())
                  body_inline += "; ";
                body_inline += seg;
              }
              if (!found_done) {
                last_code = 1;
                break;
              }

              int rc2 = 0;
              int guard = 0;
              const int GUARD_MAX = 100000;
              while (true) {
                int cnd = 0;
                if (!cond.empty()) {
                  cnd = execute_simple_or_pipeline(cond);
                }
                if (cnd != 0)
                  break;
                auto cmds2 =
                    shell_parser->parse_semicolon_commands(body_inline);
                for (const auto& c2 : cmds2) {
                  rc2 = execute_simple_or_pipeline(c2);
                  if (rc2 != 0)
                    break;
                }
                if (rc2 != 0)
                  break;
                if (++guard > GUARD_MAX) {
                  std::cerr << "cjsh: while loop aborted (guard)" << std::endl;
                  rc2 = 1;
                  break;
                }
              }
              last_code = rc2;
              k = scan;
              break;
            }
          }

          int code = 0;
          bool is_function_call = false;
          {
            std::vector<std::string> first_toks =
                shell_parser->parse_command(cmd_text);
            if (!first_toks.empty() && functions.count(first_toks[0])) {
              is_function_call = true;

              std::vector<std::string> param_names;
              for (size_t pi = 1; pi < first_toks.size() && pi <= 9; ++pi) {
                std::string name = std::to_string(pi);
                param_names.push_back(name);
                setenv(name.c_str(), first_toks[pi].c_str(), 1);
              }

              code = execute_block(functions[first_toks[0]]);

              if (code == 253) {
                const char* return_code_env = getenv("CJSH_RETURN_CODE");
                if (return_code_env) {
                  try {
                    code = std::stoi(return_code_env);
                    unsetenv("CJSH_RETURN_CODE");
                  } catch (const std::exception&) {
                    code = 0;
                  }
                }
              }

              for (const auto& n : param_names)
                unsetenv(n.c_str());
            } else {
              code = execute_simple_or_pipeline(cmd_text);
            }
          }
          last_code = code;

          setenv("STATUS", std::to_string(last_code).c_str(), 1);

          if (g_shell && g_shell->is_errexit_enabled() && code != 0) {
            if (code != 253 && code != 254 && code != 255) {
              if (g_debug_mode) {
                std::cerr
                    << "DEBUG: errexit triggered, exiting due to error code: "
                    << code << std::endl;
              }
              return code;
            }
          }

          if (!is_function_call &&
              (code == 253 || code == 254 || code == 255)) {
            if (g_debug_mode) {
              std::cerr << "DEBUG: Control flow signal detected: " << code
                        << std::endl;
            }

            goto control_flow_exit;
          }

          if (code != 0 && debug_level >= DebugLevel::BASIC) {
            std::cerr << "DEBUG: Command failed (" << code << ") -> '"
                      << cmd_text << "'" << std::endl;
          }
        }
      }
    }

  control_flow_exit:

    if (last_code == 127) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Stopping script block due to critical error: "
                  << last_code << std::endl;
      return last_code;
    } else if (last_code == 253 || last_code == 254 || last_code == 255) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Passing through control flow code: " << last_code
                  << std::endl;
      return last_code;
    } else if (last_code != 0 && g_debug_mode) {
      std::cerr << "DEBUG: Command failed with exit code " << last_code
                << " but continuing execution" << std::endl;
    }
  }

  return last_code;
}

std::string ShellScriptInterpreter::expand_parameter_expression(
    const std::string& param_expr) {
  if (param_expr.empty()) {
    return "";
  }

  if (param_expr[0] == '!') {
    std::string var_name = param_expr.substr(1);
    std::string indirect_name = get_variable_value(var_name);
    return get_variable_value(indirect_name);
  }

  if (param_expr[0] == '#') {
    std::string var_name = param_expr.substr(1);
    std::string value = get_variable_value(var_name);
    return std::to_string(value.length());
  }

  size_t op_pos = std::string::npos;
  std::string op;

  for (size_t i = 1; i < param_expr.length(); ++i) {
    if (param_expr[i] == '/') {
      if (i + 1 < param_expr.length() && param_expr[i + 1] == '/') {
        op_pos = i;
        op = "//";
        break;
      } else {
        op_pos = i;
        op = "/";
        break;
      }
    }
  }

  if (op_pos == std::string::npos) {
    for (size_t i = 1; i < param_expr.length(); ++i) {
      if (param_expr[i] == '^') {
        if (i + 1 < param_expr.length() && param_expr[i + 1] == '^') {
          op_pos = i;
          op = "^^";
          break;
        } else {
          op_pos = i;
          op = "^";
          break;
        }
      } else if (param_expr[i] == ',') {
        if (i + 1 < param_expr.length() && param_expr[i + 1] == ',') {
          op_pos = i;
          op = ",,";
          break;
        } else {
          op_pos = i;
          op = ",";
          break;
        }
      }
    }
  }

  if (op_pos == std::string::npos) {
    for (size_t i = 1; i < param_expr.length(); ++i) {
      if (param_expr[i] == ':' && i + 1 < param_expr.length()) {
        char next = param_expr[i + 1];
        if (next == '-' || next == '=' || next == '?' || next == '+') {
          op_pos = i;
          op = param_expr.substr(i, 2);
          break;
        }
      } else if (param_expr[i] == '#' || param_expr[i] == '%') {
        if (i + 1 < param_expr.length() && param_expr[i + 1] == param_expr[i]) {
          op_pos = i;
          op = param_expr.substr(i, 2);
          break;
        } else {
          op_pos = i;
          op = param_expr.substr(i, 1);
          break;
        }
      } else if (param_expr[i] == '-' || param_expr[i] == '=' ||
                 param_expr[i] == '?' || param_expr[i] == '+') {
        op_pos = i;
        op = param_expr.substr(i, 1);
        break;
      }
    }
  }

  std::string var_name = param_expr.substr(0, op_pos);
  std::string var_value = get_variable_value(var_name);
  bool is_set = variable_is_set(var_name);

  if (op_pos == std::string::npos) {
    return var_value;
  }

  std::string operand = param_expr.substr(op_pos + op.length());

  if (op == ":-") {
    return (is_set && !var_value.empty()) ? var_value : operand;
  } else if (op == "-") {
    return is_set ? var_value : operand;
  } else if (op == ":=") {
    if (!is_set || var_value.empty()) {
      if (ReadonlyManager::instance().is_readonly(var_name)) {
        std::cerr << "cjsh: " << var_name << ": readonly variable" << std::endl;
        return var_value;
      }
      setenv(var_name.c_str(), operand.c_str(), 1);
      return operand;
    }
    return var_value;
  } else if (op == "=") {
    if (!is_set) {
      if (ReadonlyManager::instance().is_readonly(var_name)) {
        std::cerr << "cjsh: " << var_name << ": readonly variable" << std::endl;
        return var_value;
      }
      setenv(var_name.c_str(), operand.c_str(), 1);
      return operand;
    }
    return var_value;
  } else if (op == ":?") {
    if (!is_set || var_value.empty()) {
      std::cerr << "cjsh: " << var_name << ": "
                << (operand.empty() ? "parameter null or not set" : operand)
                << std::endl;
      exit(1);
    }
    return var_value;
  } else if (op == "?") {
    if (!is_set) {
      std::cerr << "cjsh: " << var_name << ": "
                << (operand.empty() ? "parameter not set" : operand)
                << std::endl;
      exit(1);
    }
    return var_value;
  } else if (op == ":+") {
    return (is_set && !var_value.empty()) ? operand : "";
  } else if (op == "+") {
    return is_set ? operand : "";
  } else if (op == "#") {
    return pattern_match_prefix(var_value, operand, false);
  } else if (op == "##") {
    return pattern_match_prefix(var_value, operand, true);
  } else if (op == "%") {
    return pattern_match_suffix(var_value, operand, false);
  } else if (op == "%%") {
    return pattern_match_suffix(var_value, operand, true);
  } else if (op == "/") {
    return pattern_substitute(var_value, operand, false);
  } else if (op == "//") {
    return pattern_substitute(var_value, operand, true);
  } else if (op == "^") {
    return case_convert(var_value, operand, true, false);
  } else if (op == "^^") {
    return case_convert(var_value, operand, true, true);
  } else if (op == ",") {
    return case_convert(var_value, operand, false, false);
  } else if (op == ",,") {
    return case_convert(var_value, operand, false, true);
  }

  return var_value;
}

std::string ShellScriptInterpreter::get_variable_value(
    const std::string& var_name) {
  if (var_name == "?") {
    const char* status_env = getenv("STATUS");
    return status_env ? status_env : "0";
  } else if (var_name == "$") {
    return std::to_string(getpid());
  } else if (var_name == "#") {
    if (g_shell) {
      return std::to_string(g_shell->get_positional_parameter_count());
    }
    return "0";
  } else if (var_name == "*" || var_name == "@") {
    if (g_shell) {
      auto params = g_shell->get_positional_parameters();
      std::string result;
      for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0)
          result += " ";
        result += params[i];
      }
      return result;
    }
    return "";
  } else if (var_name == "!") {
    const char* last_bg_pid = getenv("!");
    if (last_bg_pid) {
      return last_bg_pid;
    } else {
      pid_t last_pid = JobManager::instance().get_last_background_pid();
      if (last_pid > 0) {
        return std::to_string(last_pid);
      } else {
        return "";
      }
    }
  } else if (var_name.length() == 1 && isdigit(var_name[0])) {
    const char* env_val = getenv(var_name.c_str());
    if (env_val) {
      return env_val;
    }

    int param_num = var_name[0] - '0';
    if (g_shell && param_num > 0) {
      auto params = g_shell->get_positional_parameters();
      if (static_cast<size_t>(param_num - 1) < params.size()) {
        return params[param_num - 1];
      }
    }
    return "";
  }

  const char* env_val = getenv(var_name.c_str());
  return env_val ? env_val : "";
}

bool ShellScriptInterpreter::variable_is_set(const std::string& var_name) {
  if (var_name == "?" || var_name == "$" || var_name == "#" ||
      var_name == "*" || var_name == "@" || var_name == "!") {
    return true;
  } else if (var_name.length() == 1 && isdigit(var_name[0])) {
    if (getenv(var_name.c_str()) != nullptr) {
      return true;
    }

    int param_num = var_name[0] - '0';
    if (g_shell && param_num > 0) {
      auto params = g_shell->get_positional_parameters();
      return static_cast<size_t>(param_num - 1) < params.size();
    }
    return false;
  }

  return getenv(var_name.c_str()) != nullptr;
}

std::string ShellScriptInterpreter::pattern_match_prefix(
    const std::string& value, const std::string& pattern, bool longest) {
  if (value.empty() || pattern.empty()) {
    return value;
  }

  size_t best_match = 0;

  for (size_t i = 0; i <= value.length(); ++i) {
    std::string prefix = value.substr(0, i);
    if (matches_pattern(prefix, pattern)) {
      if (longest) {
        best_match = i;
      } else {
        return value.substr(i);
      }
    }
  }

  return value.substr(best_match);
}

std::string ShellScriptInterpreter::pattern_match_suffix(
    const std::string& value, const std::string& pattern, bool longest) {
  if (value.empty() || pattern.empty()) {
    return value;
  }

  size_t best_match = value.length();

  for (size_t i = 0; i <= value.length(); ++i) {
    std::string suffix = value.substr(value.length() - i);
    if (matches_pattern(suffix, pattern)) {
      if (longest) {
        best_match = value.length() - i;
      } else {
        return value.substr(0, value.length() - i);
      }
    }
  }

  return value.substr(0, best_match);
}

bool ShellScriptInterpreter::matches_pattern(const std::string& text,
                                             const std::string& pattern) {
  size_t ti = 0, pi = 0;
  size_t star_idx = std::string::npos, match_idx = 0;

  while (ti < text.length()) {
    if (pi < pattern.length() &&
        (pattern[pi] == '?' || pattern[pi] == text[ti])) {
      ti++;
      pi++;
    } else if (pi < pattern.length() && pattern[pi] == '*') {
      star_idx = pi++;
      match_idx = ti;
    } else if (star_idx != std::string::npos) {
      pi = star_idx + 1;
      ti = ++match_idx;
    } else {
      return false;
    }
  }

  while (pi < pattern.length() && pattern[pi] == '*') {
    pi++;
  }

  return pi == pattern.length();
}

std::string ShellScriptInterpreter::pattern_substitute(
    const std::string& value, const std::string& replacement_expr,
    bool global) {
  if (value.empty() || replacement_expr.empty()) {
    return value;
  }

  size_t slash_pos = replacement_expr.find('/');
  if (slash_pos == std::string::npos) {
    return value;
  }

  std::string pattern = replacement_expr.substr(0, slash_pos);
  std::string replacement = replacement_expr.substr(slash_pos + 1);

  if (pattern.empty()) {
    return value;
  }

  std::string result = value;

  if (pattern.find('*') == std::string::npos &&
      pattern.find('?') == std::string::npos) {
    if (global) {
      size_t pos = 0;
      while ((pos = result.find(pattern, pos)) != std::string::npos) {
        result.replace(pos, pattern.length(), replacement);
        pos += replacement.length();
      }
    } else {
      size_t pos = result.find(pattern);
      if (pos != std::string::npos) {
        result.replace(pos, pattern.length(), replacement);
      }
    }
  } else {
    if (!global && matches_pattern(result, pattern)) {
      result = replacement;
    }
  }

  return result;
}

std::string ShellScriptInterpreter::case_convert(const std::string& value,
                                                 const std::string& pattern,
                                                 bool uppercase,
                                                 bool all_chars) {
  if (value.empty()) {
    return value;
  }

  std::string result = value;

  if (pattern.empty()) {
    if (all_chars) {
      for (char& c : result) {
        if (uppercase) {
          c = std::toupper(c);
        } else {
          c = std::tolower(c);
        }
      }
    } else {
      if (!result.empty()) {
        if (uppercase) {
          result[0] = std::toupper(result[0]);
        } else {
          result[0] = std::tolower(result[0]);
        }
      }
    }
  } else {
    if (all_chars) {
      for (char& c : result) {
        if (uppercase) {
          c = std::toupper(c);
        } else {
          c = std::tolower(c);
        }
      }
    } else {
      if (!result.empty()) {
        if (uppercase) {
          result[0] = std::toupper(result[0]);
        } else {
          result[0] = std::tolower(result[0]);
        }
      }
    }
  }

  return result;
}

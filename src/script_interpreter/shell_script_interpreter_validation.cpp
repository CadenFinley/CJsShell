#include "shell_script_interpreter.h"
#include "shell_script_interpreter_error_reporter.h"
#include "shell_script_interpreter_utils.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using shell_script_interpreter::detail::process_line_for_validation;
using shell_script_interpreter::detail::strip_inline_comment;
using shell_script_interpreter::detail::trim;

namespace {

using SyntaxError = ShellScriptInterpreter::SyntaxError;
using ErrorSeverity = ShellScriptInterpreter::ErrorSeverity;
using ErrorCategory = ShellScriptInterpreter::ErrorCategory;

bool has_inline_terminator(const std::string& text,
                           const std::string& terminator) {
  return text.find(" " + terminator) != std::string::npos ||
         text.find(";" + terminator) != std::string::npos ||
         text.find(terminator + ";") != std::string::npos;
}

bool handle_inline_loop_header(
    const std::string& line, const std::string& keyword, size_t display_line,
    std::vector<std::pair<std::string, size_t>>& control_stack) {
  if (line.rfind(keyword + " ", 0) == 0 &&
      line.find("; do") != std::string::npos) {
    if (!has_inline_terminator(line, "done")) {
      control_stack.push_back({"do", display_line});
    }
    return true;
  }
  return false;
}

struct QuoteState {
  bool in_quotes = false;
  char quote_char = '\0';
  bool escaped = false;
};

bool should_skip_line(const std::string& line) {
  size_t first_non_space = line.find_first_not_of(" \t");
  return first_non_space == std::string::npos || line[first_non_space] == '#';
}

bool should_process_char(QuoteState& state, char c, bool ignore_single_quotes,
                         bool process_escaped_chars = true) {
  if (state.escaped) {
    state.escaped = false;
    return process_escaped_chars;
  }

  if (c == '\\' && (!state.in_quotes || state.quote_char != '\'')) {
    state.escaped = true;
    return false;
  }

  if (!state.in_quotes && (c == '"' || c == '\'')) {
    state.in_quotes = true;
    state.quote_char = c;
    return false;
  }

  if (state.in_quotes && c == state.quote_char) {
    state.in_quotes = false;
    state.quote_char = '\0';
    return false;
  }

  if (state.in_quotes && state.quote_char == '\'' && ignore_single_quotes)
    return false;

  return true;
}

bool extract_trimmed_line(const std::string& line, std::string& trimmed_line,
                          size_t& first_non_space) {
  first_non_space = line.find_first_not_of(" \t");
  if (first_non_space == std::string::npos)
    return false;

  if (line[first_non_space] == '#')
    return false;

  trimmed_line = line.substr(first_non_space);
  return true;
}

template <typename ProcessFunc>
std::vector<SyntaxError> process_lines_for_validation(
    const std::vector<std::string>& lines, ProcessFunc process_line_func) {
  std::vector<SyntaxError> errors;

  for (size_t line_num = 0; line_num < lines.size(); ++line_num) {
    const std::string& line = lines[line_num];
    size_t display_line = line_num + 1;

    std::string trimmed_line;
    size_t first_non_space = 0;
    if (!extract_trimmed_line(line, trimmed_line, first_non_space)) {
      continue;
    }

    auto line_errors =
        process_line_func(line, trimmed_line, display_line, first_non_space);
    errors.insert(errors.end(), line_errors.begin(), line_errors.end());
  }

  return errors;
}

std::vector<std::string> tokenize_whitespace(const std::string& input) {
  std::vector<std::string> tokens;
  std::stringstream ss(input);
  std::string token;
  while (ss >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

size_t adjust_display_line(const std::string& text, size_t base_line,
                           size_t offset) {
  size_t limit = std::min(offset, text.size());
  return base_line + static_cast<size_t>(
                         std::count(text.begin(), text.begin() + limit, '\n'));
}

struct ForLoopCheckResult {
  bool incomplete = false;
  bool missing_in_keyword = false;
  bool missing_do_keyword = false;
};

ForLoopCheckResult analyze_for_loop_syntax(
    const std::vector<std::string>& tokens, const std::string& trimmed_line) {
  ForLoopCheckResult result;

  if (tokens.size() < 3) {
    result.incomplete = true;
    return result;
  }

  bool has_in_clause =
      std::find(tokens.begin(), tokens.end(), "in") != tokens.end();
  if (!has_in_clause) {
    result.missing_in_keyword = true;
    return result;
  }

  bool has_do = std::find(tokens.begin(), tokens.end(), "do") != tokens.end();
  bool has_semicolon = trimmed_line.find(';') != std::string::npos;
  if (!has_do && !has_semicolon) {
    result.missing_do_keyword = true;
  }

  return result;
}

struct WhileUntilCheckResult {
  bool missing_do_keyword = false;
  bool missing_condition = false;
  bool unclosed_test = false;
};

WhileUntilCheckResult analyze_while_until_syntax(
    const std::string& first_token, const std::string& trimmed_line,
    const std::vector<std::string>& tokens) {
  WhileUntilCheckResult result;

  bool has_do = std::find(tokens.begin(), tokens.end(), "do") != tokens.end();
  bool has_semicolon = trimmed_line.find(';') != std::string::npos;
  if (!has_do && !has_semicolon) {
    result.missing_do_keyword = true;
  }

  size_t kw_pos = trimmed_line.find(first_token);
  std::string after_kw = kw_pos != std::string::npos
                             ? trimmed_line.substr(kw_pos + first_token.size())
                             : "";
  size_t non = after_kw.find_first_not_of(" \t");
  if (non != std::string::npos)
    after_kw = after_kw.substr(non);
  else
    after_kw.clear();

  bool immediate_do = (after_kw == "do" || after_kw.find("do ") == 0 ||
                       after_kw.find("do\t") == 0);

  size_t semi = after_kw.find(';');
  if (semi != std::string::npos)
    after_kw = after_kw.substr(0, semi);

  size_t do_pos = after_kw.rfind(" do");
  if (do_pos != std::string::npos && do_pos == after_kw.size() - 3)
    after_kw = after_kw.substr(0, do_pos);
  do_pos = after_kw.rfind("\tdo");
  if (do_pos != std::string::npos && do_pos == after_kw.size() - 3)
    after_kw = after_kw.substr(0, do_pos);

  std::string cond = after_kw;
  while (!cond.empty() && isspace(static_cast<unsigned char>(cond.back())))
    cond.pop_back();

  if (cond.empty() || immediate_do) {
    result.missing_condition = true;
  } else {
    if ((cond.find('[') != std::string::npos &&
         cond.find(']') == std::string::npos) ||
        (cond.find("[[") != std::string::npos &&
         cond.find("]]") == std::string::npos)) {
      result.unclosed_test = true;
    }
  }

  return result;
}

struct IfCheckResult {
  bool missing_then_keyword = false;
  bool missing_condition = false;
};

IfCheckResult analyze_if_syntax(const std::vector<std::string>& tokens,
                                const std::string& trimmed_line) {
  IfCheckResult result;

  bool has_then_on_line =
      std::find(tokens.begin(), tokens.end(), "then") != tokens.end();
  bool has_semicolon = trimmed_line.find(';') != std::string::npos;

  if (!has_then_on_line && !has_semicolon) {
    result.missing_then_keyword = true;
  }

  if (tokens.size() == 1 || (tokens.size() == 2 && tokens[1] == "then")) {
    result.missing_condition = true;
  }

  return result;
}

struct CaseCheckResult {
  bool incomplete = false;
  bool missing_in_keyword = false;
};

CaseCheckResult analyze_case_syntax(const std::vector<std::string>& tokens) {
  CaseCheckResult result;

  if (tokens.size() < 3) {
    result.incomplete = true;
    return result;
  }

  bool has_in_keyword =
      std::find(tokens.begin(), tokens.end(), "in") != tokens.end();
  if (!has_in_keyword) {
    result.missing_in_keyword = true;
  }

  return result;
}

bool is_valid_identifier_start(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool is_valid_identifier_char(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool is_valid_identifier(const std::string& text) {
  if (text.empty() || !is_valid_identifier_start(text.front()))
    return false;
  for (size_t i = 1; i < text.size(); ++i) {
    if (!is_valid_identifier_char(text[i]))
      return false;
  }
  return true;
}

bool is_allowed_array_index_char(char c) {
  if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
    return true;
  switch (c) {
    case '+':
    case '-':
    case '*':
    case '/':
    case '%':
    case '(':
    case ')':
      return true;
    default:
      return false;
  }
}

bool validate_array_index_expression(const std::string& index_text,
                                     std::string& issue) {
  if (index_text.empty()) {
    issue = "Empty array index";
    return false;
  }
  if (index_text.find_first_of(" \t") != std::string::npos) {
    issue = "Array index cannot contain whitespace";
    return false;
  }
  for (char c : index_text) {
    if (!is_allowed_array_index_char(c)) {
      issue = "Invalid characters in array index";
      return false;
    }
  }
  return true;
}

}  // namespace

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::validate_script_syntax(
    const std::vector<std::string>& lines) {
  std::vector<SyntaxError> errors;

  std::vector<std::pair<std::string, size_t>> control_stack;

  for (size_t line_num = 0; line_num < lines.size(); ++line_num) {
    const std::string& line = lines[line_num];
    size_t display_line = line_num + 1;

    std::string trimmed;
    size_t first_non_space = 0;
    if (!extract_trimmed_line(line, trimmed, first_non_space)) {
      continue;
    }

    std::string line_without_comments = strip_inline_comment(line);
    QuoteState quote_state;

    for (char c : line_without_comments) {
      should_process_char(quote_state, c, false, false);
    }

    if (quote_state.in_quotes) {
      errors.push_back({display_line,
                        "Unclosed quote: missing closing " +
                            std::string(1, quote_state.quote_char),
                        line});
    }

    int paren_balance = 0;
    quote_state = QuoteState{};
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

        if (!should_process_char(quote_state, c, false, false)) {
          continue;
        }

        if (!quote_state.in_quotes) {
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

    std::string trimmed_for_parsing = process_line_for_validation(trimmed);

    if (!trimmed_for_parsing.empty() && trimmed_for_parsing.back() == ';') {
      trimmed_for_parsing.pop_back();
      trimmed_for_parsing = trim(trimmed_for_parsing);
    }

    if (trimmed_for_parsing.rfind("if ", 0) == 0 &&
        trimmed_for_parsing.find("; then") != std::string::npos) {
      if (!has_inline_terminator(trimmed_for_parsing, "fi")) {
        control_stack.push_back({"if", display_line});

        control_stack.back().first = "then";
      }
    } else if (handle_inline_loop_header(trimmed_for_parsing, "while",
                                         display_line, control_stack) ||
               handle_inline_loop_header(trimmed_for_parsing, "until",
                                         display_line, control_stack) ||
               handle_inline_loop_header(trimmed_for_parsing, "for",
                                         display_line, control_stack)) {
      // Inline loop handled above.
    } else {
      auto tokens = tokenize_whitespace(trimmed_for_parsing);

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
          auto for_check = analyze_for_loop_syntax(tokens, trimmed_for_parsing);
          if (for_check.missing_in_keyword) {
            errors.push_back(
                {display_line, "'for' statement missing 'in' clause", line});
          }
          control_stack.push_back({"for", display_line});
        }

        else if (first_token == "case") {
          auto case_check = analyze_case_syntax(tokens);
          if (case_check.missing_in_keyword) {
            errors.push_back(
                {display_line, "'case' statement missing 'in' clause", line});
          }

          if (!has_inline_terminator(trimmed_for_parsing, "esac")) {
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

    {
      std::string msg = "Unclosed '" + unclosed.first + "' - missing '" +
                        expected_close + "'";
      SyntaxError syn_err(unclosed.second, msg, "");
      if (unclosed.first == "{" || unclosed.first == "function") {
        syn_err.error_code = "SYN007";
        syn_err.suggestion = "Add closing '}'";
      } else {
        syn_err.error_code = "SYN001";
        syn_err.suggestion = "Close the control structure";
      }
      syn_err.category = ErrorCategory::CONTROL_FLOW;
      syn_err.severity = ErrorSeverity::ERROR;
      errors.push_back(syn_err);
    }
    control_stack.pop_back();
  }

  return errors;
}

bool ShellScriptInterpreter::has_syntax_errors(
    const std::vector<std::string>& lines, bool print_errors) {
  std::vector<SyntaxError> errors =
      validate_comprehensive_syntax(lines, true, false, false);

  bool has_critical_errors = false;
  for (const auto& error : errors) {
    if (error.severity == ErrorSeverity::CRITICAL) {
      has_critical_errors = true;
      break;
    }
  }

  if (has_critical_errors && print_errors) {
    std::vector<SyntaxError> critical_errors;
    for (const auto& error : errors) {
      if (error.severity == ErrorSeverity::CRITICAL) {
        critical_errors.push_back(error);
      }
    }
    if (!critical_errors.empty()) {
      shell_script_interpreter::ErrorReporter::print_error_report(
          critical_errors, true, true);
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

  auto add_errors = [&all_errors](const std::vector<SyntaxError>& new_errors) {
    all_errors.insert(all_errors.end(), new_errors.begin(), new_errors.end());
  };

  add_errors(validate_script_syntax(lines));
  add_errors(validate_variable_usage(lines));
  add_errors(validate_redirection_syntax(lines));
  add_errors(validate_arithmetic_expressions(lines));
  add_errors(validate_parameter_expansions(lines));
  add_errors(analyze_control_flow(lines));
  add_errors(validate_pipeline_syntax(lines));
  add_errors(validate_function_syntax(lines));
  add_errors(validate_loop_syntax(lines));
  add_errors(validate_conditional_syntax(lines));
  add_errors(validate_array_syntax(lines));
  add_errors(validate_heredoc_syntax(lines));

  if (check_semantics) {
    add_errors(validate_command_existence(lines));
  }

  if (check_style) {
    add_errors(check_style_guidelines(lines));
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

    if (should_skip_line(line)) {
      continue;
    }

    size_t eq_pos = line.find('=');
    if (eq_pos != std::string::npos) {
      std::string before_eq = line.substr(0, eq_pos);

      size_t start = before_eq.find_first_not_of(" \t");
      if (start != std::string::npos) {
        before_eq = before_eq.substr(start);

        before_eq = trim(before_eq);

        if (is_valid_identifier(before_eq)) {
          defined_vars[before_eq].push_back(
              adjust_display_line(line, display_line, eq_pos));
        }
      }
    }

    QuoteState quote_state;

    for (size_t i = 0; i < line.length(); ++i) {
      char c = line[i];

      if (!should_process_char(quote_state, c, true)) {
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
          used_vars[var_name].push_back(
              adjust_display_line(line, display_line, i));
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
  return process_lines_for_validation(
      lines,
      [](const std::string& line, const std::string& /* trimmed_line */,
         size_t display_line,
         size_t /* first_non_space */) -> std::vector<SyntaxError> {
        std::vector<SyntaxError> line_errors;

        QuoteState quote_state;

        for (size_t i = 0; i < line.length(); ++i) {
          char c = line[i];

          if (!should_process_char(quote_state, c, false)) {
            continue;
          }

          if (!quote_state.in_quotes) {
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

              size_t check_pos = i + 1;
              while (check_pos < line.length() &&
                     std::isspace(line[check_pos])) {
                check_pos++;
              }

              if (check_pos < line.length()) {
                char next_char = line[check_pos];
                if ((redir_op == ">" && next_char == '>') ||
                    (redir_op == "<" && next_char == '<') ||
                    (redir_op == ">>" && next_char == '>') ||
                    (redir_op == "<<" && next_char == '<')) {
                  line_errors.push_back(
                      SyntaxError({display_line, redir_start, check_pos + 1, 0},
                                  ErrorSeverity::ERROR,
                                  ErrorCategory::REDIRECTION, "RED005",
                                  "Invalid redirection syntax '" + redir_op +
                                      " " + next_char + "'",
                                  line, "Use single redirection operator"));
                  continue;
                }
              }

              size_t target_start = i + 1;
              while (target_start < line.length() &&
                     std::isspace(line[target_start])) {
                target_start++;
              }

              if (target_start >= line.length()) {
                line_errors.push_back(SyntaxError(
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
                if (target.empty() ||
                    (!std::isdigit(static_cast<unsigned char>(target[0])) &&
                     target != "-")) {
                  line_errors.push_back(SyntaxError(
                      {display_line, target_start, target_end, 0},
                      ErrorSeverity::ERROR, ErrorCategory::REDIRECTION,
                      "RED002",
                      "File descriptor redirection requires digit or '-'", line,
                      "Use format like 2>&1 or 2>&-"));
                }
              } else if (redir_op == "<<") {
                if (target.empty()) {
                  line_errors.push_back(SyntaxError(
                      {display_line, target_start, target_end, 0},
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
                  line_errors.push_back(SyntaxError(
                      {display_line, pipe_pos, pipe_pos + 1, 0},
                      ErrorSeverity::ERROR, ErrorCategory::REDIRECTION,
                      "RED004", "Pipe missing command after '|'", line,
                      "Add command after pipe"));
                }
              }
            }
          }
        }

        return line_errors;
      });
}

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::validate_arithmetic_expressions(
    const std::vector<std::string>& lines) {
  return process_lines_for_validation(
      lines,
      [](const std::string& line, const std::string& /* trimmed_line */,
         size_t display_line,
         size_t /* first_non_space */) -> std::vector<SyntaxError> {
        std::vector<SyntaxError> line_errors;

        QuoteState quote_state;

        for (size_t i = 0; i < line.length(); ++i) {
          char c = line[i];

          if (!should_process_char(quote_state, c, true)) {
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

            const size_t adjusted_line =
                adjust_display_line(line, display_line, start);

            if (paren_count > 0) {
              line_errors.push_back(
                  SyntaxError({adjusted_line, start, j, 0},
                              ErrorSeverity::ERROR, ErrorCategory::SYNTAX,
                              "ARITH001", "Unclosed arithmetic expansion $(()",
                              line, "Add closing ))"));
            } else {
              if (expr.empty()) {
                line_errors.push_back(
                    SyntaxError({adjusted_line, start, j, 0},
                                ErrorSeverity::ERROR, ErrorCategory::SYNTAX,
                                "ARITH002", "Empty arithmetic expression", line,
                                "Provide expression inside $(( ))"));
              } else {
                std::string trimmed_expr = expr;

                trimmed_expr.erase(0, trimmed_expr.find_first_not_of(" \t"));
                trimmed_expr.erase(trimmed_expr.find_last_not_of(" \t") + 1);

                if (!trimmed_expr.empty()) {
                  char last_char = trimmed_expr.back();
                  if (last_char == '+' || last_char == '-' ||
                      last_char == '*' || last_char == '/' ||
                      last_char == '%' || last_char == '&' ||
                      last_char == '|' || last_char == '^') {
                    line_errors.push_back(SyntaxError(
                        {adjusted_line, start, j, 0}, ErrorSeverity::ERROR,
                        ErrorCategory::SYNTAX, "ARITH003",
                        "Incomplete arithmetic expression - missing operand",
                        line,
                        "Add operand after '" + std::string(1, last_char) +
                            "'"));
                  }
                }

                if (expr.find("/0") != std::string::npos ||
                    expr.find("% 0") != std::string::npos) {
                  line_errors.push_back(SyntaxError(
                      {adjusted_line, start, j, 0}, ErrorSeverity::WARNING,
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
                  line_errors.push_back(SyntaxError(
                      {display_line, start, j, 0}, ErrorSeverity::ERROR,
                      ErrorCategory::SYNTAX, "ARITH005",
                      "Unbalanced parentheses in arithmetic expression", line,
                      "Check parentheses balance in expression"));
                }
              }
            }

            i = j - 1;
          }

          else if (c == '$' && i + 1 < line.length() && line[i + 1] == '[') {
            line_errors.push_back(
                SyntaxError({display_line, i, i + 2, 0}, ErrorSeverity::WARNING,
                            ErrorCategory::STYLE, "ARITH006",
                            "Deprecated arithmetic syntax $[...], use $((...))",
                            line, "Replace $[expr] with $((expr))"));
          }
        }

        return line_errors;
      });
}

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::validate_parameter_expansions(
    const std::vector<std::string>& lines) {
  return process_lines_for_validation(
      lines,
      [](const std::string& line, const std::string& /* trimmed_line */,
         size_t display_line,
         size_t /* first_non_space */) -> std::vector<SyntaxError> {
        std::vector<SyntaxError> line_errors;

        QuoteState quote_state;

        for (size_t i = 0; i < line.length(); ++i) {
          char c = line[i];

          if (!should_process_char(quote_state, c, true)) {
            continue;
          }

          if (c == '$' && i + 1 < line.length() && line[i + 1] == '(') {
            size_t start = i;
            size_t paren_count = 1;
            size_t j = i + 2;
            const bool inside_quotes = quote_state.in_quotes;

            while (j < line.length() && paren_count > 0) {
              if (line[j] == '(' && !inside_quotes) {
                paren_count++;
              } else if (line[j] == ')' && !inside_quotes) {
                paren_count--;
              } else if ((line[j] == '"' || line[j] == '\'') &&
                         !inside_quotes) {
                char nested_quote = line[j];
                j++;
                while (j < line.length() && line[j] != nested_quote) {
                  if (line[j] == '\\')
                    j++;
                  j++;
                }
              }
              j++;
            }

            if (paren_count > 0) {
              line_errors.push_back(
                  SyntaxError({display_line, start, j, 0}, ErrorSeverity::ERROR,
                              ErrorCategory::SYNTAX, "SYN005",
                              "Unclosed command substitution $() - missing ')'",
                              line, "Add closing parenthesis"));
            }

            i = j - 1;
          }

          else if (c == '`' && !quote_state.in_quotes) {
            size_t start = i;
            size_t j = i + 1;
            bool found_closing = false;

            while (j < line.length()) {
              if (line[j] == '`') {
                found_closing = true;
                j++;
                break;
              }
              if (line[j] == '\\')
                j++;
              j++;
            }

            if (!found_closing) {
              line_errors.push_back(SyntaxError(
                  {display_line, start, j, 0}, ErrorSeverity::ERROR,
                  ErrorCategory::SYNTAX, "SYN006",
                  "Unclosed backtick command substitution - missing '`'", line,
                  "Add closing backtick"));
            }

            i = j - 1;
          }

          if (!quote_state.in_quotes && c == '=' && i > 0) {
            size_t var_start = i;

            if (i > 0 && line[i - 1] == ']') {
              size_t pos = i - 1;
              int bracket_depth = 0;
              bool found_open = false;
              while (pos > 0) {
                char bc = line[pos];
                if (bc == ']')
                  bracket_depth++;
                else if (bc == '[') {
                  if (bracket_depth == 0) {
                    found_open = true;
                    break;
                  } else
                    bracket_depth--;
                }
                pos--;
              }
              if (found_open) {
                size_t name_end = pos;
                size_t name_start = name_end;
                while (name_start > 0 && (std::isalnum(line[name_start - 1]) ||
                                          line[name_start - 1] == '_')) {
                  name_start--;
                }
                if (name_start < name_end) {
                  size_t index_start = pos + 1;
                  size_t index_end = i - 2;
                  std::string index_text;
                  if (index_end >= index_start)
                    index_text =
                        line.substr(index_start, index_end - index_start + 1);
                  std::string var_name_only =
                      line.substr(name_start, name_end - name_start);

                  std::string index_issue;
                  if (!validate_array_index_expression(index_text,
                                                       index_issue)) {
                    line_errors.push_back(SyntaxError(
                        {display_line, name_start, i, 0}, ErrorSeverity::ERROR,
                        ErrorCategory::VARIABLES, "VAR005",
                        index_issue + " for array '" + var_name_only + "'",
                        line,
                        "Use a valid numeric or arithmetic expression index"));
                  }

                  var_start = name_start;
                }
              }
            }

            while (var_start > 0 && (std::isalnum(line[var_start - 1]) ||
                                     line[var_start - 1] == '_')) {
              var_start--;
            }

            if (var_start < i) {
              std::string var_name = line.substr(var_start, i - var_start);

              if (!var_name.empty()) {
          if (!is_valid_identifier_start(var_name[0])) {
                  line_errors.push_back(SyntaxError(
                      {display_line, var_start, i, 0}, ErrorSeverity::ERROR,
                      ErrorCategory::VARIABLES, "VAR004",
            "Invalid variable name '" + var_name +
              "' - must start with letter or underscore",
            line,
            "Use variable name starting with letter or underscore"));
                }

                if (var_start > 0 && std::isspace(line[var_start - 1])) {
                  line_errors.push_back(SyntaxError(
                      {display_line, var_start - 1, i + 1, 0},
                      ErrorSeverity::ERROR, ErrorCategory::VARIABLES, "VAR005",
                      "Variable assignment cannot have spaces around '='", line,
                      "Remove spaces: " + var_name + "=value"));
                }
                if (i + 1 < line.length() && std::isspace(line[i + 1])) {
                  line_errors.push_back(SyntaxError(
                      {display_line, var_start, i + 2, 0}, ErrorSeverity::ERROR,
                      ErrorCategory::VARIABLES, "VAR005",
                      "Variable assignment cannot have spaces around '='", line,
                      "Remove spaces: " + var_name + "=value"));
                }
              }
            }
          }
        }

        return line_errors;
      });
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
  std::vector<SyntaxError> errors;

  for (size_t line_num = 0; line_num < lines.size(); ++line_num) {
    const std::string& line = lines[line_num];
    size_t display_line = line_num + 1;

    std::string trimmed_line;
    size_t first_non_space = 0;
    if (!extract_trimmed_line(line, trimmed_line, first_non_space)) {
      continue;
    }

    if (trimmed_line.rfind("if ", 0) == 0 ||
        trimmed_line.rfind("while ", 0) == 0 ||
        trimmed_line.rfind("until ", 0) == 0) {
      int logical_ops = 0;
      int bracket_depth = 0;
      int max_bracket_depth = 0;
      QuoteState quote_state;

      for (size_t i = 0; i < line.length() - 1; ++i) {
        char c = line[i];

        if (!should_process_char(quote_state, c, false, false)) {
          continue;
        }

        if (!quote_state.in_quotes) {
          if ((c == '&' && line[i + 1] == '&') ||
              (c == '|' && line[i + 1] == '|')) {
            logical_ops++;
            i++;
          } else if (c == '[') {
            bracket_depth++;
            max_bracket_depth = std::max(max_bracket_depth, bracket_depth);
          } else if (c == ']') {
            bracket_depth--;
          }
        }
      }

      if (logical_ops > 3) {
        errors.push_back(
            SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::INFO,
                        ErrorCategory::STYLE, "STYLE001",
                        "Complex condition with " +
                            std::to_string(logical_ops) + " logical operators",
                        line,
                        "Consider breaking into multiple if statements or "
                        "using a function"));
      }

      if (max_bracket_depth > 2) {
        errors.push_back(
            SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::INFO,
                        ErrorCategory::STYLE, "STYLE002",
                        "Deeply nested test conditions (depth: " +
                            std::to_string(max_bracket_depth) + ")",
                        line, "Consider simplifying the condition logic"));
      }
    }

    if (line.length() > 100) {
      errors.push_back(SyntaxError(
          {display_line, 100, line.length(), 0}, ErrorSeverity::INFO,
          ErrorCategory::STYLE, "STYLE003",
          "Line length (" + std::to_string(line.length()) +
              " chars) exceeds recommended 100 characters",
          line, "Consider breaking long lines for better readability"));
    }

    if (line.find('\t') != std::string::npos &&
        line.find(' ') != std::string::npos) {
      size_t first_tab = line.find('\t');
      size_t first_space = line.find(' ');
      if (first_tab < 20 && first_space < 20) {
        errors.push_back(SyntaxError(
            {display_line, 0, std::min(first_tab, first_space), 0},
            ErrorSeverity::INFO, ErrorCategory::STYLE, "STYLE004",
            "Mixed tabs and spaces for indentation", line,
            "Use consistent indentation (either all tabs or all spaces)"));
      }
    }

    if (trimmed_line.find("eval ") != std::string::npos ||
        trimmed_line.find("$(") != std::string::npos) {
      std::string warning_type = trimmed_line.find("eval ") != std::string::npos
                                     ? "eval"
                                     : "command substitution";
      errors.push_back(SyntaxError(
          {display_line, 0, 0, 0}, ErrorSeverity::WARNING, ErrorCategory::STYLE,
          "STYLE005", "Use of " + warning_type + " - potential security risk",
          line, "Validate input carefully or consider safer alternatives"));
    }
  }

  return errors;
}

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::validate_pipeline_syntax(
    const std::vector<std::string>& lines) {
  return process_lines_for_validation(
      lines,
      [](const std::string& line, const std::string& trimmed_line,
         size_t display_line,
         size_t first_non_space) -> std::vector<SyntaxError> {
        std::vector<SyntaxError> line_errors;

        {
          std::string work = trimmed_line;

          size_t eq = work.find('=');
          if (eq != std::string::npos) {
            std::string lhs = work.substr(0, eq);

            while (!lhs.empty() &&
                   isspace(static_cast<unsigned char>(lhs.back())))
              lhs.pop_back();

            size_t lb = lhs.find('[');
            size_t rb = lhs.rfind(']');
            if (lb != std::string::npos && rb != std::string::npos && rb > lb &&
                rb == lhs.size() - 1) {
              std::string name = lhs.substr(0, lb);
              bool name_ok = is_valid_identifier(name);
              std::string index_text = lhs.substr(lb + 1, rb - lb - 1);
              std::string issue;
              if (name_ok &&
                  !validate_array_index_expression(index_text, issue)) {
                line_errors.push_back(SyntaxError(
                    {display_line, first_non_space + lb,
                     first_non_space + rb + 1, 0},
                    ErrorSeverity::ERROR, ErrorCategory::VARIABLES, "VAR005",
                    issue + " for array '" + name + "'", line,
                    "Use a valid numeric or arithmetic expression index"));
              }
            }
          }
        }

        if (!trimmed_line.empty() && trimmed_line[0] == '|' &&
            !(trimmed_line.size() > 1 && trimmed_line[1] == '|')) {
          line_errors.push_back(SyntaxError(
              {display_line, first_non_space, first_non_space + 1, 0},
              ErrorSeverity::ERROR, ErrorCategory::REDIRECTION, "PIPE002",
              "Pipeline cannot start with pipe operator", line,
              "Remove leading pipe or add command before pipe"));
        }

        QuoteState quote_state;

        for (size_t i = 0; i < line.length(); ++i) {
          char c = line[i];

          if (!should_process_char(quote_state, c, false, false)) {
            continue;
          }

          if (!quote_state.in_quotes && c == '|' && i + 1 < line.length()) {
            if (line[i + 1] == '|' &&
                !(i + 2 < line.length() && line[i + 2] == '|')) {
              size_t after_logical = i + 2;
              while (after_logical < line.length() &&
                     std::isspace(line[after_logical])) {
                after_logical++;
              }

              if (after_logical < line.length() && line[after_logical] == '|') {
                line_errors.push_back(SyntaxError(
                    {display_line, i, after_logical + 1, 0},
                    ErrorSeverity::ERROR, ErrorCategory::REDIRECTION, "PIPE001",
                    "Invalid pipeline syntax", line,
                    "Check pipe operator usage"));
              }
              i++;
            } else if (line[i + 1] != '|') {
              size_t after_pipe = i + 1;
              while (after_pipe < line.length() &&
                     std::isspace(line[after_pipe])) {
                after_pipe++;
              }

              if (after_pipe >= line.length() || line[after_pipe] == '|' ||
                  line[after_pipe] == '&') {
                line_errors.push_back(SyntaxError(
                    {display_line, i, i + 1, 0}, ErrorSeverity::ERROR,
                    ErrorCategory::REDIRECTION, "PIPE001",
                    "Pipe missing command after '|'", line,
                    "Add command after pipe"));
              }
            }
          }
        }

        return line_errors;
      });
}

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::validate_function_syntax(
    const std::vector<std::string>& lines) {
  return process_lines_for_validation(
      lines,
      [](const std::string& line, const std::string& trimmed_line,
         size_t display_line,
         size_t /* first_non_space */) -> std::vector<SyntaxError> {
        std::vector<SyntaxError> line_errors;

        if (trimmed_line.rfind("function", 0) == 0) {
          auto tokens = tokenize_whitespace(trimmed_line);

          if (tokens.size() < 2) {
            line_errors.push_back(
                SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR,
                            ErrorCategory::SYNTAX, "FUNC001",
                            "Function declaration missing name", line,
                            "Add function name: function name() { ... }"));
          } else {
            const std::string& func_name = tokens[1];

            if (func_name == "()") {
              line_errors.push_back(
                  SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR,
                              ErrorCategory::SYNTAX, "FUNC001",
                              "Function declaration missing name", line,
                              "Add function name before parentheses"));
            } else if (!is_valid_identifier_start(func_name[0])) {
              line_errors.push_back(
                  SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR,
                              ErrorCategory::SYNTAX, "FUNC002",
                              "Invalid function name '" + func_name +
                                  "' - must start with letter or underscore",
                              line,
                              "Use valid function name starting with letter or "
                              "underscore"));
            } else {
              for (char c : func_name) {
                if (!is_valid_identifier_char(c)) {
                  line_errors.push_back(SyntaxError(
                      {display_line, 0, 0, 0}, ErrorSeverity::ERROR,
                      ErrorCategory::SYNTAX, "FUNC002",
                      "Invalid function name '" + func_name +
                          "' - contains invalid character '" + c + "'",
                      line,
                      "Use only letters, numbers, and underscores in "
                      "function names"));
                  break;
                }
              }
            }
          }
        }

        size_t paren_pos = trimmed_line.find("()");
        if (paren_pos != std::string::npos && paren_pos > 0) {
          std::string potential_func = trimmed_line.substr(0, paren_pos);

          if (trimmed_line.find("{", paren_pos) != std::string::npos) {
            if (potential_func.empty()) {
              line_errors.push_back(
                  SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR,
                              ErrorCategory::SYNTAX, "FUNC001",
                              "Function declaration missing name", line,
                              "Add function name before parentheses"));
            } else {
              if (!is_valid_identifier_start(potential_func[0])) {
                line_errors.push_back(
                    SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR,
                                ErrorCategory::SYNTAX, "FUNC002",
                                "Invalid function name '" + potential_func +
                                    "' - must start with letter or underscore",
                                line,
                                "Use valid function name starting with letter "
                                "or underscore"));
              }
            }
          }
        }

        return line_errors;
      });
}

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::validate_loop_syntax(
    const std::vector<std::string>& lines) {
  return process_lines_for_validation(
      lines,
      [](const std::string& line, const std::string& trimmed_line,
         size_t display_line,
         size_t /* first_non_space */) -> std::vector<SyntaxError> {
        std::vector<SyntaxError> line_errors;

        auto tokens = tokenize_whitespace(trimmed_line);

        if (!tokens.empty()) {
          const std::string& first_token = tokens[0];

          if (first_token == "for") {
            auto loop_check = analyze_for_loop_syntax(tokens, trimmed_line);
            if (loop_check.incomplete) {
              line_errors.push_back(
                  SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR,
                              ErrorCategory::CONTROL_FLOW, "SYN002",
                              "'for' statement incomplete", line,
                              "Complete for statement: for var in list; do"));
            } else if (!loop_check.missing_in_keyword &&
                       loop_check.missing_do_keyword) {
              line_errors.push_back(
                  SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR,
                              ErrorCategory::CONTROL_FLOW, "SYN002",
                              "'for' statement missing 'do' keyword", line,
                              "Add 'do' keyword: for var in list; do"));
            }
          }

          else if (first_token == "while" || first_token == "until") {
            auto loop_check =
                analyze_while_until_syntax(first_token, trimmed_line, tokens);

            if (loop_check.missing_condition) {
              line_errors.push_back(SyntaxError(
                  {display_line, 0, 0, 0}, ErrorSeverity::ERROR,
                  ErrorCategory::CONTROL_FLOW, "SYN003",
                  "'" + first_token + "' loop missing condition expression",
                  line, "Add a condition expression before 'do'"));
            } else if (loop_check.unclosed_test) {
              line_errors.push_back(SyntaxError(
                  {display_line, 0, 0, 0}, ErrorSeverity::ERROR,
                  ErrorCategory::CONTROL_FLOW, "SYN003",
                  "Unclosed test expression in '" + first_token + "' condition",
                  line, "Close the '[' with ']' or use '[[ ... ]]'"));
            }

            if (loop_check.missing_do_keyword) {
              line_errors.push_back(SyntaxError(
                  {display_line, 0, 0, 0}, ErrorSeverity::ERROR,
                  ErrorCategory::CONTROL_FLOW, "SYN002",
                  "'" + first_token + "' statement missing 'do' keyword", line,
                  "Add 'do' keyword: " + first_token + " condition; do"));
            }
          }
        }

        return line_errors;
      });
}

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::validate_conditional_syntax(
    const std::vector<std::string>& lines) {
  return process_lines_for_validation(
      lines,
      [](const std::string& line, const std::string& trimmed_line,
         size_t display_line,
         size_t /* first_non_space */) -> std::vector<SyntaxError> {
        std::vector<SyntaxError> line_errors;

        auto tokens = tokenize_whitespace(trimmed_line);

        if (!tokens.empty()) {
          const std::string& first_token = tokens[0];

          if (first_token == "if") {
            auto if_check = analyze_if_syntax(tokens, trimmed_line);
            if (if_check.missing_then_keyword) {
              line_errors.push_back(
                  SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR,
                              ErrorCategory::CONTROL_FLOW, "SYN004",
                              "'if' statement missing 'then' keyword", line,
                              "Add 'then' keyword: if condition; then"));
            }

            if (if_check.missing_condition) {
              line_errors.push_back(
                  SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR,
                              ErrorCategory::CONTROL_FLOW, "SYN004",
                              "'if' statement missing condition", line,
                              "Add condition: if [ condition ]; then"));
            }
          }

          else if (first_token == "case") {
            auto case_check = analyze_case_syntax(tokens);
            if (case_check.incomplete) {
              line_errors.push_back(
                  SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR,
                              ErrorCategory::CONTROL_FLOW, "SYN008",
                              "'case' statement incomplete", line,
                              "Complete case statement: case variable in"));
            } else if (case_check.missing_in_keyword) {
              line_errors.push_back(
                  SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR,
                              ErrorCategory::CONTROL_FLOW, "SYN008",
                              "'case' statement missing 'in' keyword", line,
                              "Add 'in' keyword: case variable in"));
            }
          }
        }

        return line_errors;
      });
}

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::validate_array_syntax(
    const std::vector<std::string>& lines) {
  return process_lines_for_validation(
      lines,
      [](const std::string& line, const std::string& /* trimmed_line */,
         size_t display_line,
         size_t /* first_non_space */) -> std::vector<SyntaxError> {
        std::vector<SyntaxError> line_errors;

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

          if (!in_quotes && c == '(' && i > 0) {
            size_t var_end = i;
            while (var_end > 0 && std::isspace(line[var_end - 1])) {
              var_end--;
            }

            if (var_end > 0 && line[var_end - 1] == '=') {
              size_t paren_count = 1;
              size_t j = i + 1;

              while (j < line.length() && paren_count > 0) {
                if (line[j] == '(' && !in_quotes) {
                  paren_count++;
                } else if (line[j] == ')' && !in_quotes) {
                  paren_count--;
                } else if ((line[j] == '"' || line[j] == '\'') && !in_quotes) {
                  in_quotes = true;
                  quote_char = line[j];
                } else if (line[j] == quote_char && in_quotes) {
                  in_quotes = false;
                  quote_char = '\0';
                }
                j++;
              }

              if (paren_count > 0) {
                line_errors.push_back(
                    SyntaxError({display_line, i, j, 0}, ErrorSeverity::ERROR,
                                ErrorCategory::SYNTAX, "SYN009",
                                "Unclosed array declaration - missing ')'",
                                line, "Add closing parenthesis"));
              }

              i = j - 1;
            }
          }
        }

        return line_errors;
      });
}

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::validate_heredoc_syntax(
    const std::vector<std::string>& lines) {
  std::vector<SyntaxError> errors;

  std::vector<std::pair<std::string, size_t>> heredoc_stack;

  for (size_t line_num = 0; line_num < lines.size(); ++line_num) {
    const std::string& line = lines[line_num];
    size_t display_line = line_num + 1;

    if (!heredoc_stack.empty()) {
      std::string trimmed_line = line;
      trimmed_line.erase(0, trimmed_line.find_first_not_of(" \t"));
      trimmed_line.erase(trimmed_line.find_last_not_of(" \t\r\n") + 1);

      if (trimmed_line == heredoc_stack.back().first) {
        heredoc_stack.pop_back();
        continue;
      }
    }

    size_t heredoc_pos = line.find("<<");
    if (heredoc_pos != std::string::npos) {
      bool in_quotes = false;
      char quote_char = '\0';

      for (size_t i = 0; i < heredoc_pos; ++i) {
        if ((line[i] == '"' || line[i] == '\'') && !in_quotes) {
          in_quotes = true;
          quote_char = line[i];
        } else if (line[i] == quote_char && in_quotes) {
          in_quotes = false;
          quote_char = '\0';
        }
      }

      if (!in_quotes) {
        size_t delim_start = heredoc_pos + 2;
        while (delim_start < line.length() && std::isspace(line[delim_start])) {
          delim_start++;
        }

        if (delim_start < line.length()) {
          size_t delim_end = delim_start;
          while (delim_end < line.length() && !std::isspace(line[delim_end]) &&
                 line[delim_end] != ';' && line[delim_end] != '&' &&
                 line[delim_end] != '|') {
            delim_end++;
          }

          if (delim_start < delim_end) {
            std::string delimiter =
                line.substr(delim_start, delim_end - delim_start);

            if ((delimiter.front() == '"' && delimiter.back() == '"') ||
                (delimiter.front() == '\'' && delimiter.back() == '\'')) {
              delimiter = delimiter.substr(1, delimiter.length() - 2);
            }

            if (!heredoc_stack.empty()) {
              errors.push_back(SyntaxError(
                  {display_line, heredoc_pos, delim_end, 0},
                  ErrorSeverity::WARNING, ErrorCategory::SYNTAX, "SYN011",
                  "Nested heredoc detected - may cause parsing issues", line,
                  "Consider closing previous heredoc '" +
                      heredoc_stack.back().first +
                      "' before starting new one"));
            }

            heredoc_stack.push_back({delimiter, display_line});
          }
        }
      }
    }
  }

  while (!heredoc_stack.empty()) {
    auto& unclosed = heredoc_stack.back();
    errors.push_back(SyntaxError(
        {unclosed.second, 0, 0, 0}, ErrorSeverity::ERROR, ErrorCategory::SYNTAX,
        "SYN010", "Unclosed here document - missing '" + unclosed.first + "'",
        "", "Add closing delimiter: " + unclosed.first));
    heredoc_stack.pop_back();
  }

  return errors;
}

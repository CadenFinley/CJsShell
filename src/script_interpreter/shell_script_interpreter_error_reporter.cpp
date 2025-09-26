#include "shell_script_interpreter_error_reporter.h"

#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <sstream>

namespace shell_script_interpreter {

size_t ErrorReporter::get_terminal_width() {
  struct winsize w;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
    return w.ws_col;
  }
  return 80;
}

void ErrorReporter::print_error_report(
    const std::vector<ShellScriptInterpreter::SyntaxError>& errors,
    bool show_suggestions, bool show_context) {
  using ErrorSeverity = ShellScriptInterpreter::ErrorSeverity;
  using SyntaxError = ShellScriptInterpreter::SyntaxError;

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
    std::cout << std::endl;
  }

  if (!sorted_errors.empty()) {
    std::cout << std::endl;
  }
}

void ErrorReporter::print_runtime_error(const std::string& error_message,
                                        const std::string& context,
                                        size_t line_number) {
  using ErrorSeverity = ShellScriptInterpreter::ErrorSeverity;
  using ErrorCategory = ShellScriptInterpreter::ErrorCategory;
  using SyntaxError = ShellScriptInterpreter::SyntaxError;

  SyntaxError runtime_error({line_number, 0, 0, 0}, ErrorSeverity::ERROR,
                            ErrorCategory::COMMANDS, "RUN001", error_message,
                            context, "");

  std::vector<SyntaxError> errors = {runtime_error};
  print_error_report(errors, false, !context.empty());
}

}  // namespace shell_script_interpreter
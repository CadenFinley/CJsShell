#include "shell_script_interpreter_error_reporter.h"

#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>

namespace shell_script_interpreter {

namespace {

std::string strip_internal_placeholders(const std::string& input,
                                        size_t* column_start = nullptr,
                                        size_t* column_end = nullptr) {
    static const std::string markers[] = {
        "\x1E__NOENV_START__\x1E", "\x1E__NOENV_END__\x1E",
        "\x1E__SUBST_LITERAL_START__\x1E", "\x1E__SUBST_LITERAL_END__\x1E"};

    if (input.empty()) {
        if (column_start)
            *column_start = 0;
        if (column_end)
            *column_end = 0;
        return input;
    }

    std::vector<size_t> index_map(input.size() + 1, 0);
    std::string output;
    output.reserve(input.size());

    size_t i = 0;
    size_t sanitized_index = 0;

    while (i < input.size()) {
        index_map[i] = sanitized_index;

        bool matched_marker = false;
        for (const auto& marker : markers) {
            size_t len = marker.size();
            if (input.compare(i, len, marker) == 0) {
                for (size_t k = 0; k < len && i + k + 1 <= input.size(); ++k) {
                    index_map[i + k + 1] = sanitized_index;
                }
                i += len;
                matched_marker = true;
                break;
            }
        }

        if (matched_marker)
            continue;

        if (input[i] == '\x1E') {
            index_map[i + 1] = sanitized_index;
            ++i;
            continue;
        }

        output.push_back(input[i]);
        ++sanitized_index;
        ++i;
    }

    index_map[input.size()] = sanitized_index;

    if (column_start) {
        size_t original = std::min(*column_start, input.size());
        *column_start = index_map[original];
    }
    if (column_end) {
        size_t original = std::min(*column_end, input.size());
        *column_end = index_map[original];
    }

    return output;
}

}  // namespace

size_t ErrorReporter::get_terminal_width() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        return w.ws_col;
    }
    return 80;
}

void ErrorReporter::print_error_report(
    const std::vector<ShellScriptInterpreter::SyntaxError>& errors,
    bool show_suggestions, bool show_context, int start_error_number) {
    using ErrorSeverity = ShellScriptInterpreter::ErrorSeverity;
    using SyntaxError = ShellScriptInterpreter::SyntaxError;

    static thread_local int global_error_count = 0;

    int actual_start_number;
    if (start_error_number == -1) {
        global_error_count++;
        actual_start_number = global_error_count;
    } else {
        actual_start_number = start_error_number;

        if (start_error_number == 1) {
            global_error_count = 0;
        }
    }

    static thread_local bool error_reporting_in_progress = false;
    if (error_reporting_in_progress) {
        std::cerr << "cjsh: error: recursive error reporting detected, "
                     "aborting to prevent infinite loop"
                  << std::endl;
        return;
    }
    error_reporting_in_progress = true;

    try {
        if (errors.empty()) {
            std::cout << "\033[32m✓ No syntax errors found.\033[0m"
                      << std::endl;
            error_reporting_in_progress = false;
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
                          return a.position.line_number <
                                 b.position.line_number;
                      }
                      return a.position.column_start < b.position.column_start;
                  });

        int error_count = actual_start_number - 1;
        for (const auto& error : sorted_errors) {
            error_count++;

            std::string severity_color;
            std::string severity_icon;
            std::string severity_prefix;

            size_t column_start = error.position.column_start;
            size_t column_end = error.position.column_end;
            std::string sanitized_message =
                strip_internal_placeholders(error.message);
            std::string sanitized_line_content = strip_internal_placeholders(
                error.line_content, &column_start, &column_end);
            std::string sanitized_suggestion =
                strip_internal_placeholders(error.suggestion);

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

            std::cout << BOLD << "┌─ " << error_count << ". " << severity_icon
                      << " " << severity_color << severity_prefix << RESET
                      << BOLD << " [" << BLUE << error.error_code << RESET
                      << BOLD << "]" << RESET << std::endl;

            std::cout << "│  " << DIM << "at line " << BOLD
                      << error.position.line_number << RESET;
            if (column_start > 0) {
                std::cout << DIM << ", column " << BOLD << column_start
                          << RESET;
            }
            std::cout << std::endl;

            std::cout << "│  " << severity_color << sanitized_message << RESET
                      << std::endl;

            if (show_context && !sanitized_line_content.empty()) {
                std::cout << "│" << std::endl;

                std::string line_num_str =
                    std::to_string(error.position.line_number);
                std::cout << "│  " << DIM << line_num_str << " │ " << RESET;

                size_t terminal_width = get_terminal_width();
                size_t line_prefix_width = 6 + line_num_str.length();
                size_t available_width =
                    terminal_width > line_prefix_width + 10
                        ? terminal_width - line_prefix_width - 5
                        : 60;

                std::string display_line = sanitized_line_content;

                size_t adjusted_column_start = column_start;
                size_t adjusted_column_end = column_end;

                if (display_line.find('\n') != std::string::npos) {
                    std::vector<std::string> lines;
                    std::stringstream ss(display_line);
                    std::string line;

                    while (std::getline(ss, line)) {
                        lines.push_back(line);
                    }

                    if (column_start > 0 && !lines.empty()) {
                        size_t cumulative_length = 0;
                        size_t target_line_index = 0;

                        for (size_t i = 0; i < lines.size(); ++i) {
                            if (column_start <=
                                cumulative_length + lines[i].length()) {
                                target_line_index = i;
                                adjusted_column_start =
                                    column_start - cumulative_length;
                                size_t effective_column_end =
                                    column_end > cumulative_length
                                        ? column_end - cumulative_length
                                        : 0;
                                adjusted_column_end = std::min(
                                    effective_column_end, lines[i].length());
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
                                std::min(column_end, display_line.length());
                        }
                    } else {
                        display_line = lines.empty() ? "" : lines[0];
                        adjusted_column_start = 0;
                        adjusted_column_end = display_line.length();
                    }
                }

                size_t pos = 0;
                while ((pos = display_line.find("\\x0a", pos)) !=
                       std::string::npos) {
                    display_line.replace(pos, 4, "\n");
                    pos += 1;
                }
                pos = 0;
                while ((pos = display_line.find("\\x09", pos)) !=
                       std::string::npos) {
                    display_line.replace(pos, 4, "\t");
                    pos += 1;
                }
                pos = 0;
                while ((pos = display_line.find("\\x0d", pos)) !=
                       std::string::npos) {
                    display_line.replace(pos, 4, "\r");
                    pos += 1;
                }

                for (size_t i = 0; i < display_line.length(); ++i) {
                    if (display_line[i] == '\t') {
                        display_line.replace(i, 1, "    ");
                        i += 3;
                    } else if (display_line[i] == '\n' ||
                               display_line[i] == '\r') {
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
                        size_t end_pos = std::min(display_line.length(),
                                                  available_width - 3);
                        display_line = display_line.substr(0, end_pos) + "...";
                    } else if (error_col + suffix_context >=
                               display_line.length()) {
                        display_start_col =
                            display_line.length() > available_width - 3
                                ? display_line.length() - (available_width - 3)
                                : 0;
                        display_line =
                            "..." + display_line.substr(display_start_col);
                        adjusted_start = adjusted_start - display_start_col + 3;
                        adjusted_end = adjusted_end - display_start_col + 3;
                    } else {
                        size_t ideal_start = error_col > prefix_context
                                                 ? error_col - prefix_context
                                                 : 0;

                        display_start_col = ideal_start;
                        for (size_t i = ideal_start;
                             i > 0 && i > ideal_start - 10 &&
                             i < display_line.length();
                             --i) {
                            if (display_line[i] == ' ' ||
                                display_line[i] == '\t' ||
                                display_line[i] == '(' ||
                                display_line[i] == '[' ||
                                display_line[i] == '{' ||
                                display_line[i] == '"' ||
                                display_line[i] == '\'') {
                                display_start_col = i + 1;
                                break;
                            }
                        }

                        size_t display_end =
                            std::min(display_start_col + available_width - 6,
                                     display_line.length());

                        for (size_t i = display_end;
                             i < display_line.length() && i < display_end + 10;
                             ++i) {
                            if (display_line[i] == ' ' ||
                                display_line[i] == '\t' ||
                                display_line[i] == ')' ||
                                display_line[i] == ']' ||
                                display_line[i] == '}' ||
                                display_line[i] == '"' ||
                                display_line[i] == '\'') {
                                display_end = i;
                                break;
                            }
                        }

                        display_line = "..." +
                                       display_line.substr(
                                           display_start_col,
                                           display_end - display_start_col) +
                                       "...";
                        adjusted_start = adjusted_start - display_start_col + 3;
                        adjusted_end = adjusted_end - display_start_col + 3;
                    }
                }

                if (column_start > 0 && column_end > column_start &&
                    adjusted_start < display_line.length()) {
                    size_t start = adjusted_start;
                    size_t end = std::min(adjusted_end, display_line.length());

                    if (start < display_line.length()) {
                        std::cout << display_line.substr(0, start);
                        std::cout << BG_RED << WHITE;
                        if (end <= display_line.length()) {
                            std::cout
                                << display_line.substr(start, end - start);
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

                if (column_start > 0 &&
                    adjusted_start < display_line.length()) {
                    std::cout << "│  " << DIM
                              << std::string(line_num_str.length(), ' ')
                              << " │ " << RESET;
                    std::cout << std::string(adjusted_start, ' ');
                    std::cout << severity_color << "^";
                    if (adjusted_end > adjusted_start + 1 &&
                        adjusted_end <= display_line.length()) {
                        size_t tilde_count = std::min(
                            adjusted_end - adjusted_start - 1,
                            display_line.length() - adjusted_start - 1);
                        std::cout << std::string(tilde_count, '~');
                    }
                    std::cout << RESET << std::endl;
                }
            }

            if (show_suggestions && !sanitized_suggestion.empty()) {
                std::cout << "│" << std::endl;
                std::cout << "│  " << GREEN << "Suggestion: " << RESET
                          << sanitized_suggestion << std::endl;
            }

            size_t terminal_width = get_terminal_width();
            size_t content_width = 0;

            if (!sanitized_line_content.empty()) {
                std::string line_num_str =
                    std::to_string(error.position.line_number);
                size_t line_prefix_width = 6 + line_num_str.length();
                size_t max_line_display_width =
                    terminal_width > line_prefix_width + 10
                        ? terminal_width - line_prefix_width - 5
                        : 60;

                size_t actual_line_width = std::min(
                    sanitized_line_content.length(), max_line_display_width);
                content_width = std::max(content_width,
                                         line_prefix_width + actual_line_width);
            }

            content_width =
                std::max(content_width, 3 + sanitized_message.length());

            if (!sanitized_suggestion.empty()) {
                content_width =
                    std::max(content_width, 15 + sanitized_suggestion.length());
            }

            size_t footer_width = std::min(
                content_width, terminal_width > 10 ? terminal_width - 2 : 50);
            footer_width = std::max(footer_width, static_cast<size_t>(50));
            footer_width = std::min(footer_width, terminal_width - 2);
            footer_width = std::min(footer_width, static_cast<size_t>(120));

            std::cout << "└";
            for (size_t i = 0; i < footer_width; i++) {
                std::cout << "—";
            }
            std::cout << std::endl;
        }

    } catch (...) {
        std::cerr << "cjsh: error: exception during error reporting"
                  << std::endl;
    }

    error_reporting_in_progress = false;
}

void ErrorReporter::print_runtime_error(const std::string& error_message,
                                        const std::string& context,
                                        size_t line_number) {
    using ErrorSeverity = ShellScriptInterpreter::ErrorSeverity;
    using ErrorCategory = ShellScriptInterpreter::ErrorCategory;
    using SyntaxError = ShellScriptInterpreter::SyntaxError;

    std::string suggestion = "";
    if (error_message.find("command not found") != std::string::npos) {
        suggestion = "Try 'help' to see available commands.";
    } else if (error_message.find("Unclosed quote") != std::string::npos) {
        suggestion = "Make sure all quotes are properly closed";
    }

    SyntaxError runtime_error({line_number, 0, 0, 0}, ErrorSeverity::ERROR,
                              ErrorCategory::COMMANDS, "RUN001", error_message,
                              context, suggestion);

    std::vector<SyntaxError> errors = {runtime_error};
    print_error_report(errors, true, !context.empty());
}

void ErrorReporter::reset_error_count() {
}

}  // namespace shell_script_interpreter
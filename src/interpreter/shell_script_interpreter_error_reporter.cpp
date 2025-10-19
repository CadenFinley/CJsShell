#include "shell_script_interpreter_error_reporter.h"

#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>

#include "suggestion_utils.h"

namespace shell_script_interpreter {

using ErrorSeverity = ShellScriptInterpreter::ErrorSeverity;
using ErrorCategory = ShellScriptInterpreter::ErrorCategory;
using SyntaxError = ShellScriptInterpreter::SyntaxError;

namespace {

std::string strip_internal_placeholders(const std::string& input, size_t* column_start = nullptr,
                                        size_t* column_end = nullptr) {
    static const std::string markers[] = {"\x1E__NOENV_START__\x1E", "\x1E__NOENV_END__\x1E",
                                          "\x1E__SUBST_LITERAL_START__\x1E",
                                          "\x1E__SUBST_LITERAL_END__\x1E"};

    if (input.empty()) {
        if (column_start != nullptr)
            *column_start = 0;
        if (column_end != nullptr)
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

    if (column_start != nullptr) {
        size_t original = std::min(*column_start, input.size());
        *column_start = index_map[original];
    }
    if (column_end != nullptr) {
        size_t original = std::min(*column_end, input.size());
        *column_end = index_map[original];
    }

    return output;
}

size_t get_terminal_width() {
    struct winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        return w.ws_col;
    }
    return 80;
}

}  // namespace

void print_error_report(const std::vector<ShellScriptInterpreter::SyntaxError>& errors,
                        bool show_suggestions, bool show_context, int start_error_number) {
    using ErrorSeverity = ShellScriptInterpreter::ErrorSeverity;
    using SyntaxError = ShellScriptInterpreter::SyntaxError;

    static thread_local int global_error_count = 0;

    int actual_start_number = 0;
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
                  << '\n';
        return;
    }
    error_reporting_in_progress = true;

    try {
        if (errors.empty()) {
            std::cout << "\033[32m✓ No syntax errors found.\033[0m" << '\n';
            error_reporting_in_progress = false;
            return;
        }

        const std::string reset_color = "\033[0m";
        const std::string bold_style = "\033[1m";
        const std::string dim_style = "\033[2m";
        const std::string red_color = "\033[31m";
        const std::string green_color = "\033[32m";
        const std::string yellow_color = "\033[33m";
        const std::string blue_color = "\033[34m";
        const std::string cyan_color = "\033[36m";
        const std::string white_color = "\033[37m";
        const std::string bg_red_color = "\033[41m";

        std::vector<SyntaxError> sorted_errors = errors;
        std::sort(sorted_errors.begin(), sorted_errors.end(),
                  [](const SyntaxError& a, const SyntaxError& b) {
                      if (a.position.line_number != b.position.line_number) {
                          return a.position.line_number < b.position.line_number;
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
            std::string sanitized_message = strip_internal_placeholders(error.message);
            std::string sanitized_line_content =
                strip_internal_placeholders(error.line_content, &column_start, &column_end);
            std::string sanitized_suggestion = strip_internal_placeholders(error.suggestion);

            switch (error.severity) {
                case ErrorSeverity::CRITICAL:
                    severity_color = bold_style + red_color;
                    severity_icon = "";
                    severity_prefix = "CRITICAL";
                    break;
                case ErrorSeverity::ERROR:
                    severity_color = red_color;
                    severity_icon = "";
                    severity_prefix = "ERROR";
                    break;
                case ErrorSeverity::WARNING:
                    severity_color = yellow_color;
                    severity_icon = "";
                    severity_prefix = "WARNING";
                    break;
                case ErrorSeverity::INFO:
                    severity_color = cyan_color;
                    severity_icon = "";
                    severity_prefix = "INFO";
                    break;
            }

            std::cout << bold_style << "┌─ " << error_count << ". " << severity_icon << " "
                      << severity_color << severity_prefix << reset_color << bold_style << " ["
                      << blue_color << error.error_code << reset_color << bold_style << "]"
                      << reset_color << '\n';

            std::cout << "│  " << dim_style << "at line " << bold_style
                      << error.position.line_number << reset_color;
            if (column_start > 0) {
                std::cout << dim_style << ", column " << bold_style << column_start << reset_color;
            }
            std::cout << '\n';

            std::cout << "│  " << severity_color << sanitized_message << reset_color << '\n';

            if (show_context && !sanitized_line_content.empty()) {
                std::cout << "│" << '\n';

                std::string line_num_str = std::to_string(error.position.line_number);
                std::cout << "│  " << dim_style << line_num_str << " │ " << reset_color;

                size_t terminal_width = get_terminal_width();
                size_t line_prefix_width = 6 + line_num_str.length();
                size_t available_width = terminal_width > line_prefix_width + 10
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
                            if (column_start <= cumulative_length + lines[i].length()) {
                                target_line_index = i;
                                adjusted_column_start = column_start - cumulative_length;
                                size_t effective_column_end = column_end > cumulative_length
                                                                  ? column_end - cumulative_length
                                                                  : 0;
                                adjusted_column_end =
                                    std::min(effective_column_end, lines[i].length());
                                break;
                            }
                            cumulative_length += lines[i].length() + 1;
                        }

                        if (target_line_index < lines.size()) {
                            display_line = lines[target_line_index];
                        } else {
                            display_line = lines[0];
                            adjusted_column_start = 0;
                            adjusted_column_end = std::min(column_end, display_line.length());
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
                    } else if (display_line[i] < 32) {
                        char hex_repr[5];
                        (void)snprintf(hex_repr, sizeof(hex_repr), "\\x%02x",
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
                    size_t suffix_context = available_width - prefix_context - error_context;

                    if (error_col <= prefix_context) {
                        display_start_col = 0;
                        size_t end_pos = std::min(display_line.length(), available_width - 3);
                        display_line = display_line.substr(0, end_pos) + "...";
                    } else if (error_col + suffix_context >= display_line.length()) {
                        display_start_col = display_line.length() > available_width - 3
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
                             i > 0 && i > ideal_start - 10 && i < display_line.length(); --i) {
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

                if (column_start > 0 && column_end > column_start &&
                    adjusted_start < display_line.length()) {
                    size_t start = adjusted_start;
                    size_t end = std::min(adjusted_end, display_line.length());

                    if (start < display_line.length()) {
                        std::cout << display_line.substr(0, start);
                        std::cout << bg_red_color << white_color;
                        if (end <= display_line.length()) {
                            std::cout << display_line.substr(start, end - start);
                            std::cout << reset_color;
                            if (end < display_line.length()) {
                                std::cout << display_line.substr(end);
                            }
                        } else {
                            std::cout << display_line.substr(start) << reset_color;
                        }
                    } else {
                        std::cout << display_line;
                    }
                } else {
                    std::cout << display_line;
                }
                std::cout << '\n';

                if (column_start > 0 && adjusted_start < display_line.length()) {
                    std::cout << "│  " << dim_style << std::string(line_num_str.length(), ' ')
                              << " │ " << reset_color;
                    std::cout << std::string(adjusted_start, ' ');
                    std::cout << severity_color << "^";
                    if (adjusted_end > adjusted_start + 1 &&
                        adjusted_end <= display_line.length()) {
                        size_t tilde_count = std::min(adjusted_end - adjusted_start - 1,
                                                      display_line.length() - adjusted_start - 1);
                        std::cout << std::string(tilde_count, '~');
                    }
                    std::cout << reset_color << '\n';
                }
            }

            if (show_suggestions && !sanitized_suggestion.empty()) {
                std::cout << "│" << '\n';
                std::cout << "│  " << green_color << "Suggestion: " << reset_color
                          << sanitized_suggestion << '\n';
            }

            size_t terminal_width = get_terminal_width();
            size_t content_width = 0;

            if (!sanitized_line_content.empty()) {
                std::string line_num_str = std::to_string(error.position.line_number);
                size_t line_prefix_width = 6 + line_num_str.length();
                size_t max_line_display_width = terminal_width > line_prefix_width + 10
                                                    ? terminal_width - line_prefix_width - 5
                                                    : 60;

                size_t actual_line_width =
                    std::min(sanitized_line_content.length(), max_line_display_width);
                content_width = std::max(content_width, line_prefix_width + actual_line_width);
            }

            content_width = std::max(content_width, 3 + sanitized_message.length());

            if (!sanitized_suggestion.empty()) {
                content_width = std::max(content_width, 15 + sanitized_suggestion.length());
            }

            size_t footer_width =
                std::min(content_width, terminal_width > 10 ? terminal_width - 2 : 50);
            footer_width = std::max(footer_width, static_cast<size_t>(50));
            footer_width = std::min(footer_width, terminal_width - 2);
            footer_width = std::min(footer_width, static_cast<size_t>(120));

            std::cout << "└";
            for (size_t i = 0; i < footer_width; i++) {
                std::cout << "—";
            }
            std::cout << '\n';
        }

    } catch (...) {
        std::cerr << "cjsh: error: exception during error reporting" << '\n';
    }

    error_reporting_in_progress = false;
}

void print_runtime_error(const std::string& error_message, const std::string& context,
                         size_t line_number) {
    using ErrorSeverity = ShellScriptInterpreter::ErrorSeverity;
    using ErrorCategory = ShellScriptInterpreter::ErrorCategory;
    using SyntaxError = ShellScriptInterpreter::SyntaxError;

    std::string suggestion;
    if (error_message.find("command not found") != std::string::npos) {
        suggestion = "Try 'help' to see available commands.";
    } else if (error_message.find("Unclosed quote") != std::string::npos) {
        suggestion = "Make sure all quotes are properly closed";
    }

    SyntaxError runtime_error({line_number, 0, 0, 0}, ErrorSeverity::ERROR, ErrorCategory::COMMANDS,
                              "RUN001", error_message, context, suggestion);

    std::vector<SyntaxError> errors = {runtime_error};
    print_error_report(errors, true, !context.empty());
}

void reset_error_count() {
}

int handle_memory_allocation_error(const std::string& text) {
    std::vector<SyntaxError> errors;
    SyntaxError error(1, "Memory allocation failed", text);
    error.severity = ErrorSeverity::ERROR;
    error.category = ErrorCategory::COMMANDS;
    error.error_code = "MEM001";
    error.suggestion = "Command may be too complex or system is low on memory";
    errors.push_back(error);

    print_error_report(errors, true, true);
    setenv("?", "3", 1);
    return 3;
}

int handle_system_error(const std::string& text, const std::system_error& e) {
    std::vector<SyntaxError> errors;
    SyntaxError error(1, "System error: " + std::string(e.what()), text);
    error.severity = ErrorSeverity::ERROR;
    error.category = ErrorCategory::COMMANDS;
    error.error_code = "SYS001";
    error.suggestion = "Check system resources and permissions";
    errors.push_back(error);

    print_error_report(errors, true, true);
    setenv("?", "4", 1);
    return 4;
}

int handle_runtime_error(const std::string& text, const std::runtime_error& e, size_t line_number) {
    std::vector<SyntaxError> errors;
    size_t normalized_line = line_number == 0 ? 1 : line_number;
    SyntaxError error(normalized_line, e.what(), text);
    std::string error_msg = e.what();

    if (error_msg.find("command not found: ") != std::string::npos) {
        size_t pos = error_msg.find("command not found: ");
        if (pos != std::string::npos) {
            std::string command_name = error_msg.substr(pos + 19);
            auto suggestions = suggestion_utils::generate_command_suggestions(command_name);

            error.message = "cjsh: command not found: " + command_name;
            error.severity = ErrorSeverity::ERROR;
            error.category = ErrorCategory::COMMANDS;
            error.error_code = "RUN001";

            if (!suggestions.empty()) {
                std::string suggestion_text;
                std::vector<std::string> commands;

                for (const auto& suggestion : suggestions) {
                    if (suggestion.find("Did you mean") != std::string::npos) {
                        size_t start = suggestion.find("'");
                        if (start != std::string::npos) {
                            start++;
                            size_t end = suggestion.find("'", start);
                            if (end != std::string::npos) {
                                commands.push_back(suggestion.substr(start, end - start));
                            }
                        }
                    }
                }

                if (!commands.empty()) {
                    suggestion_text = "Did you mean: ";
                    for (size_t i = 0; i < commands.size(); ++i) {
                        suggestion_text += commands[i];
                        if (i < commands.size() - 1) {
                            suggestion_text += ", ";
                        }
                    }
                    suggestion_text += "?";
                } else {
                    suggestion_text = suggestions.empty()
                                          ? "Check command syntax and system resources"
                                          : suggestions[0];
                }

                error.suggestion = suggestion_text;
            } else {
                error.suggestion = "Check command syntax and system resources";
            }
        } else {
            error.severity = ErrorSeverity::ERROR;
            error.category = ErrorCategory::COMMANDS;
            error.error_code = "RUN001";
            error.suggestion = "Check command syntax and system resources";
        }

        error.position.line_number = normalized_line;
        errors.push_back(error);
        print_error_report(errors, true, true);
        setenv("?", "127", 1);
        return 127;
    }

    if (error_msg.find("Unclosed quote") != std::string::npos ||
        error_msg.find("missing closing") != std::string::npos ||
        error_msg.find("syntax error near unexpected token") != std::string::npos) {
        error.severity = ErrorSeverity::ERROR;
        error.category = ErrorCategory::SYNTAX;
        error.error_code = "SYN001";
        if (error_msg.find("syntax error near unexpected token") != std::string::npos) {
            error.suggestion = "Check for incomplete redirections or missing command arguments";
        } else {
            error.suggestion = "Make sure all quotes are properly closed";
        }
    } else if (error_msg.find("Failed to open") != std::string::npos ||
               error_msg.find("Failed to redirect") != std::string::npos ||
               error_msg.find("Failed to write") != std::string::npos) {
        error.severity = ErrorSeverity::ERROR;
        error.category = ErrorCategory::REDIRECTION;
        error.error_code = "IO001";
        error.suggestion = "Check file permissions and paths";
    } else {
        error.severity = ErrorSeverity::ERROR;
        error.category = ErrorCategory::COMMANDS;
        error.error_code = "RUN001";
        error.suggestion = "Check command syntax and system resources";
    }

    error.position.line_number = normalized_line;
    errors.push_back(error);
    print_error_report(errors, true, true);
    setenv("?", "2", 1);
    return 2;
}

int handle_generic_exception(const std::string& text, const std::exception& e) {
    std::vector<SyntaxError> errors;
    SyntaxError error(1, "Unexpected error: " + std::string(e.what()), text);
    error.severity = ErrorSeverity::ERROR;
    error.category = ErrorCategory::COMMANDS;
    error.error_code = "UNK001";
    error.suggestion =
        "An unexpected error occurred, please report this as an issue, and how to replicate it.";
    errors.push_back(error);

    print_error_report(errors, true, true);
    setenv("?", "5", 1);
    return 5;
}

int handle_unknown_error(const std::string& text) {
    std::vector<SyntaxError> errors;
    SyntaxError error(1, "Unknown error occurred", text);
    error.severity = ErrorSeverity::ERROR;
    error.category = ErrorCategory::COMMANDS;
    error.error_code = "UNK002";
    error.suggestion =
        "An unexpected error occurred, please report this as an issue, and how to replicate it.";
    errors.push_back(error);

    print_error_report(errors, true, true);
    setenv("?", "6", 1);
    return 6;
}

}  // namespace shell_script_interpreter
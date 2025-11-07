#include "shell_script_interpreter_error_reporter.h"

#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>

#include "error_out.h"
#include "suggestion_utils.h"

namespace shell_script_interpreter {

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

ErrorType map_category_to_error_type(ErrorCategory category) {
    switch (category) {
        case ErrorCategory::SYNTAX:
            return ErrorType::SYNTAX_ERROR;
        case ErrorCategory::COMMANDS:
        case ErrorCategory::CONTROL_FLOW:
        case ErrorCategory::REDIRECTION:
        case ErrorCategory::VARIABLES:
        case ErrorCategory::SEMANTICS:
        case ErrorCategory::PERFORMANCE:
            return ErrorType::RUNTIME_ERROR;
        case ErrorCategory::STYLE:
            return ErrorType::INVALID_ARGUMENT;
        default:
            return ErrorType::UNKNOWN_ERROR;
    }
}

ShellScriptInterpreter::ErrorCategory map_error_type_to_category(ErrorType type) {
    using Category = ShellScriptInterpreter::ErrorCategory;
    switch (type) {
        case ErrorType::SYNTAX_ERROR:
            return Category::SYNTAX;
        case ErrorType::COMMAND_NOT_FOUND:
            return Category::COMMANDS;
        case ErrorType::PERMISSION_DENIED:
            return Category::REDIRECTION;
        case ErrorType::FILE_NOT_FOUND:
            return Category::REDIRECTION;
        case ErrorType::INVALID_ARGUMENT:
            return Category::SEMANTICS;
        case ErrorType::RUNTIME_ERROR:
        case ErrorType::UNKNOWN_ERROR:
        default:
            return Category::COMMANDS;
    }
}

std::string error_code_from_type(ErrorType type) {
    switch (type) {
        case ErrorType::SYNTAX_ERROR:
            return "SYN001";
        case ErrorType::COMMAND_NOT_FOUND:
            return "CMD404";
        case ErrorType::PERMISSION_DENIED:
            return "PER001";
        case ErrorType::FILE_NOT_FOUND:
            return "FS001";
        case ErrorType::INVALID_ARGUMENT:
            return "ARG001";
        case ErrorType::RUNTIME_ERROR:
            return "RUN001";
        case ErrorType::UNKNOWN_ERROR:
        default:
            return "UNK001";
    }
}

std::string describe_error_type(ErrorType type) {
    switch (type) {
        case ErrorType::COMMAND_NOT_FOUND:
            return "command not found";
        case ErrorType::SYNTAX_ERROR:
            return "syntax error";
        case ErrorType::PERMISSION_DENIED:
            return "permission denied";
        case ErrorType::FILE_NOT_FOUND:
            return "file not found";
        case ErrorType::INVALID_ARGUMENT:
            return "invalid argument";
        case ErrorType::RUNTIME_ERROR:
            return "runtime error";
        case ErrorType::UNKNOWN_ERROR:
        default:
            return "unknown error";
    }
}

std::string build_basic_error_message(const ErrorInfo& error) {
    std::string message = "cjsh: ";
    if (!error.command_used.empty()) {
        message += error.command_used + ": ";
    }
    message += describe_error_type(error.type);
    if (!error.message.empty()) {
        message += ": " + error.message;
    }
    return message;
}

}  // namespace

void print_error_report(const std::vector<ShellScriptInterpreter::SyntaxError>& errors,
                        bool show_suggestions, bool show_context) {
    using SyntaxError = ShellScriptInterpreter::SyntaxError;

    static thread_local bool error_reporting_in_progress = false;
    if (error_reporting_in_progress) {
        print_error_fallback(
            {ErrorType::UNKNOWN_ERROR,
             ErrorSeverity::ERROR,
             "error-reporter",
             "recursive error reporting detected, aborting to prevent infinite loop",
             {}});
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

        const bool use_compact_error_output = !isatty(STDERR_FILENO);

        for (const auto& error : sorted_errors) {
            std::string severity_color;
            std::string severity_icon;
            std::string severity_prefix;
            bool has_line_number = error.position.line_number > 0;

            size_t column_start = error.position.column_start;
            size_t column_end = error.position.column_end;
            std::string sanitized_message = strip_internal_placeholders(error.message);
            std::string sanitized_line_content =
                strip_internal_placeholders(error.line_content, &column_start, &column_end);
            std::string sanitized_suggestion = strip_internal_placeholders(error.suggestion);

            if (use_compact_error_output) {
                std::ostringstream message_stream;
                message_stream << "[" << error.error_code << "] " << sanitized_message;
                if (error.position.line_number > 0) {
                    message_stream << " (line " << error.position.line_number;
                    if (column_start > 0) {
                        message_stream << ", column " << column_start;
                    }
                    message_stream << ")";
                }

                std::vector<std::string> suggestions;
                if (show_context && !sanitized_line_content.empty()) {
                    std::string context_line = sanitized_line_content;
                    std::replace(context_line.begin(), context_line.end(), '\n', ' ');
                    std::replace(context_line.begin(), context_line.end(), '\r', ' ');
                    if (context_line.size() > 120) {
                        context_line = context_line.substr(0, 117);
                        context_line.append("...");
                    }
                    if (error.position.line_number > 0) {
                        std::string prefix = "at line ";
                        prefix.append(std::to_string(error.position.line_number));
                        prefix.append(": ");
                        context_line.insert(0, prefix);
                    }
                }
                if (show_suggestions && !sanitized_suggestion.empty()) {
                    suggestions.push_back(sanitized_suggestion);
                }
                if (!error.documentation_url.empty()) {
                    suggestions.push_back("More info: " + error.documentation_url);
                }
                for (const auto& info : error.related_info) {
                    if (!info.empty()) {
                        suggestions.push_back(info);
                    }
                }

                ErrorInfo compact_error;
                compact_error.type = map_category_to_error_type(error.category);
                compact_error.severity = error.severity;
                compact_error.command_used.clear();
                compact_error.message = message_stream.str();
                compact_error.suggestions = suggestions;

                print_error_fallback(compact_error);
                continue;
            }

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

            std::cerr << bold_style << "┌─ ";
            if (!severity_icon.empty()) {
                std::cerr << severity_icon << ' ';
            }
            std::cerr << severity_color << severity_prefix << reset_color << bold_style << " ["
                      << blue_color << error.error_code << reset_color << bold_style << "]"
                      << reset_color << '\n';

            if (has_line_number) {
                std::cerr << "│  " << dim_style << "at line " << bold_style
                          << error.position.line_number << reset_color;
                if (column_start > 0) {
                    std::cerr << dim_style << ", column " << bold_style << column_start
                              << reset_color;
                }
                std::cerr << '\n';
            }

            std::cerr << "│  " << severity_color << sanitized_message << reset_color << '\n';

            if (show_context && !sanitized_line_content.empty()) {
                std::cerr << "│" << '\n';

                std::string line_num_str =
                    has_line_number ? std::to_string(error.position.line_number) : std::string();
                std::cerr << "│  " << dim_style << line_num_str << " │ " << reset_color;

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
                        std::cerr << display_line.substr(0, start);
                        std::cerr << bg_red_color << white_color;
                        if (end <= display_line.length()) {
                            std::cerr << display_line.substr(start, end - start);
                            std::cerr << reset_color;
                            if (end < display_line.length()) {
                                std::cerr << display_line.substr(end);
                            }
                        } else {
                            std::cerr << display_line.substr(start) << reset_color;
                        }
                    } else {
                        std::cerr << display_line;
                    }
                } else {
                    std::cerr << display_line;
                }
                std::cerr << '\n';

                if (column_start > 0 && adjusted_start < display_line.length()) {
                    std::cerr << "│  " << dim_style << std::string(line_num_str.length(), ' ')
                              << " │ " << reset_color;
                    std::cerr << std::string(adjusted_start, ' ');
                    std::cerr << severity_color << "^";
                    if (adjusted_end > adjusted_start + 1 &&
                        adjusted_end <= display_line.length()) {
                        size_t tilde_count = std::min(adjusted_end - adjusted_start - 1,
                                                      display_line.length() - adjusted_start - 1);
                        std::cerr << std::string(tilde_count, '~');
                    }
                    std::cerr << reset_color << '\n';
                }
            }

            if (show_suggestions && !sanitized_suggestion.empty()) {
                std::cerr << "│" << '\n';
                std::cerr << "│  " << green_color << "Suggestion: " << reset_color
                          << sanitized_suggestion << '\n';
            }

            size_t terminal_width = get_terminal_width();
            size_t content_width = 0;

            if (!sanitized_line_content.empty()) {
                std::string line_num_str =
                    has_line_number ? std::to_string(error.position.line_number) : std::string();
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

            std::cerr << "└";
            for (size_t i = 0; i < footer_width; i++) {
                std::cerr << "—";
            }
            std::cerr << '\n';
        }

    } catch (const std::exception& e) {
        print_error_fallback({ErrorType::UNKNOWN_ERROR,
                              ErrorSeverity::ERROR,
                              "error-reporter",
                              std::string("exception during error reporting: ") + e.what(),
                              {}});
    } catch (...) {
        print_error_fallback({ErrorType::UNKNOWN_ERROR,
                              ErrorSeverity::ERROR,
                              "error-reporter",
                              "unknown exception during error reporting",
                              {}});
    }

    error_reporting_in_progress = false;
}

void print_runtime_error(const std::string& error_message, const std::string& context,
                         size_t line_number) {
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

bool report_error(const ErrorInfo& error) {
    static thread_local bool basic_error_reporting_in_progress = false;
    if (basic_error_reporting_in_progress) {
        return false;
    }

    basic_error_reporting_in_progress = true;
    struct ReportingGuard {
        bool* flag;
        ~ReportingGuard() {
            if (flag) {
                *flag = false;
            }
        }
    } guard{&basic_error_reporting_in_progress};

    try {
        SyntaxError converted({0, 0, 0, 0}, error.severity, map_error_type_to_category(error.type),
                              error_code_from_type(error.type), build_basic_error_message(error));

        if (!error.suggestions.empty()) {
            converted.suggestion = error.suggestions.front();
            for (size_t i = 1; i < error.suggestions.size(); ++i) {
                converted.related_info.push_back(error.suggestions[i]);
            }
        }

        std::vector<SyntaxError> errors;
        errors.emplace_back(std::move(converted));

        bool show_suggestions = !error.suggestions.empty();
        print_error_report(errors, show_suggestions, false);

        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace shell_script_interpreter
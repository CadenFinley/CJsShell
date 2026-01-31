#include "status_line.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <string>
#include <vector>

#include "cjsh.h"
#include "error_out.h"
#include "interpreter.h"
#include "shell.h"
#include "shell_env.h"

namespace status_line {
namespace {

std::string sanitize_for_status(const std::string& text) {
    if (text.empty()) {
        return {};
    }

    std::string sanitized;
    sanitized.reserve(text.size());
    bool previous_space = false;

    for (char ch : text) {
        unsigned char uch = static_cast<unsigned char>(ch);
        char normalized = ch;

        if (normalized == '\n' || normalized == '\r' || normalized == '\t') {
            normalized = ' ';
        }

        if (std::iscntrl(uch) && normalized != ' ') {
            continue;
        }

        if (normalized == ' ') {
            if (sanitized.empty() || previous_space) {
                previous_space = true;
                continue;
            }
            previous_space = true;
        } else {
            previous_space = false;
        }

        sanitized.push_back(normalized);
    }

    size_t start = sanitized.find_first_not_of(' ');
    if (start == std::string::npos) {
        return {};
    }
    if (start > 0) {
        sanitized.erase(0, start);
    }

    size_t end = sanitized.find_last_not_of(' ');
    if (end != std::string::npos && end + 1 < sanitized.size()) {
        sanitized.erase(end + 1);
    }

    return sanitized;
}

const char* severity_to_label(ErrorSeverity severity) {
    switch (severity) {
        case ErrorSeverity::CRITICAL:
            return "critical";
        case ErrorSeverity::ERROR:
            return "error";
        case ErrorSeverity::WARNING:
            return "warning";
        case ErrorSeverity::INFO:
        default:
            return "info";
    }
}

constexpr const char* kAnsiReset = "\x1b[0m";

const char* severity_to_underline_style(ErrorSeverity severity) {
    switch (severity) {
        case ErrorSeverity::CRITICAL:
            return "\x1b[4m\x1b[58;5;196m";
        case ErrorSeverity::ERROR:
            return "\x1b[4m\x1b[58;5;160m";
        case ErrorSeverity::WARNING:
            return "\x1b[4m\x1b[58;5;214m";
        case ErrorSeverity::INFO:
        default:
            return "\x1b[4m\x1b[58;5;51m";
    }
}

std::string format_error_location(const ShellScriptInterpreter::SyntaxError& error) {
    const auto& pos = error.position;
    if (pos.line_number == 0) {
        return {};
    }

    std::string location = "line ";
    location += std::to_string(pos.line_number);
    if (pos.column_start > 0) {
        location += ", col ";
        location += std::to_string(pos.column_start + 1);
    }
    return location;
}

std::string build_validation_status_message(
    const std::vector<ShellScriptInterpreter::SyntaxError>& errors) {
    if (errors.empty()) {
        return {};
    }

    struct SeverityBuckets {
        size_t info = 0;
        size_t warning = 0;
        size_t error = 0;
        size_t critical = 0;
    } buckets;

    std::vector<const ShellScriptInterpreter::SyntaxError*> sorted_errors;
    sorted_errors.reserve(errors.size());

    for (const auto& err : errors) {
        switch (err.severity) {
            case ErrorSeverity::CRITICAL:
                ++buckets.critical;
                break;
            case ErrorSeverity::ERROR:
                ++buckets.error;
                break;
            case ErrorSeverity::WARNING:
                ++buckets.warning;
                break;
            case ErrorSeverity::INFO:
            default:
                ++buckets.info;
                break;
        }

        sorted_errors.push_back(&err);
    }

    auto severity_rank = [](ErrorSeverity severity) {
        switch (severity) {
            case ErrorSeverity::CRITICAL:
                return 3;
            case ErrorSeverity::ERROR:
                return 2;
            case ErrorSeverity::WARNING:
                return 1;
            case ErrorSeverity::INFO:
            default:
                return 0;
        }
    };

    std::sort(sorted_errors.begin(), sorted_errors.end(), [&](const auto* lhs, const auto* rhs) {
        int lhs_rank = severity_rank(lhs->severity);
        int rhs_rank = severity_rank(rhs->severity);
        if (lhs_rank != rhs_rank) {
            return lhs_rank > rhs_rank;
        }
        if (lhs->position.line_number != rhs->position.line_number) {
            return lhs->position.line_number < rhs->position.line_number;
        }
        if (lhs->position.column_start != rhs->position.column_start) {
            return lhs->position.column_start < rhs->position.column_start;
        }
        return lhs->message < rhs->message;
    });

    if (sorted_errors.empty()) {
        return {};
    }

    std::vector<std::string> counter_parts;
    counter_parts.reserve(4);

    auto append_part = [&counter_parts](size_t count, const char* label) {
        if (count == 0) {
            return;
        }
        std::string part = std::to_string(count);
        part.push_back(' ');
        part.append(label);
        if (count > 1) {
            part.push_back('s');
        }
        counter_parts.push_back(std::move(part));
    };

    append_part(buckets.critical, "critical");
    append_part(buckets.error, "error");
    append_part(buckets.warning, "warning");
    append_part(buckets.info, "info");

    std::string message;
    message.reserve(256);

    for (size_t i = 0; i < sorted_errors.size(); ++i) {
        const auto* error = sorted_errors[i];

        if (i != 0) {
            message.push_back('\n');
        }

        std::string line;
        line.reserve(128);
        line.push_back('[');
        line.append(severity_to_label(error->severity));
        line.append("]");

        std::string location = format_error_location(*error);
        std::string sanitized_text = sanitize_for_status(error->message);
        std::string sanitized_suggestion = sanitize_for_status(error->suggestion);
        std::string detail_text;

        if (!location.empty()) {
            detail_text.append(location);
        }
        if (!sanitized_text.empty()) {
            if (!detail_text.empty()) {
                detail_text.append(" - ");
            }
            detail_text.append(sanitized_text);
        }
        if (!sanitized_suggestion.empty()) {
            if (!detail_text.empty()) {
                detail_text.append(" | ");
            }
            detail_text.append(sanitized_suggestion);
        }

        if (!detail_text.empty()) {
            line.push_back(' ');
            line.append(detail_text);
        }

        const char* style_prefix = severity_to_underline_style(error->severity);
        if (style_prefix != nullptr && style_prefix[0] != '\0') {
            message.append(style_prefix);
            message.append(line);
            message.append(kAnsiReset);
        } else {
            message.append(line);
        }
    }

    return message;
}

std::string previous_passed_buffer;

}  // namespace

const char* create_below_syntax_message(const char* input_buffer, void*) {
    static thread_local std::string status_message;

    if (!config::status_line_enabled) {
        status_message.clear();
        previous_passed_buffer.clear();
        return nullptr;
    }

    if (!config::status_reporting_enabled) {
        status_message.clear();
        previous_passed_buffer.clear();
        return nullptr;
    }

    const std::string current_input = (input_buffer != nullptr) ? input_buffer : "";

    if (previous_passed_buffer == current_input) {
        return status_message.empty() ? nullptr : status_message.c_str();
    }

    previous_passed_buffer = current_input;

    if (current_input.empty()) {
        status_message.clear();
        return nullptr;
    }

    bool has_visible_content = std::any_of(current_input.begin(), current_input.end(), [](char ch) {
        return std::isspace(static_cast<unsigned char>(ch)) == 0;
    });

    if (!has_visible_content) {
        status_message.clear();
        return nullptr;
    }

    Shell* shell = g_shell.get();
    if (shell == nullptr) {
        status_message.clear();
        return nullptr;
    }

    ShellScriptInterpreter* interpreter = shell->get_shell_script_interpreter();
    if (interpreter == nullptr) {
        status_message.clear();
        return nullptr;
    }

    std::vector<std::string> lines = interpreter->parse_into_lines(current_input);
    if (lines.empty()) {
        lines.emplace_back(current_input);
    }

    std::vector<ShellScriptInterpreter::SyntaxError> errors;
    try {
        errors = interpreter->validate_comprehensive_syntax(lines);
    } catch (const std::exception& ex) {
        status_message.assign("Validation failed: ");
        status_message.append(sanitize_for_status(ex.what()));
        return status_message.c_str();
    } catch (...) {
        status_message.assign("Validation failed: unknown error.");
        return status_message.c_str();
    }

    status_message = build_validation_status_message(errors);
    if (status_message.empty()) {
        return nullptr;
    }

    return status_message.c_str();
}

}  // namespace status_line

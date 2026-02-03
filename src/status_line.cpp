/*
  status_line.cpp

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "status_line.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "completions/suggestion_utils.h"
#include "error_out.h"
#include "highlighter/token_classifier.h"
#include "interpreter.h"
#include "shell.h"
#include "shell_env.h"
#include "utils/quote_state.h"

namespace status_line {
namespace {

struct UnknownCommandInfo {
    std::string command;
    std::vector<std::string> suggestions;
};

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

bool extract_next_token(const std::string& cmd, size_t& cursor, size_t& token_start,
                        size_t& token_end) {
    const size_t len = cmd.length();

    while (cursor < len && (std::isspace(static_cast<unsigned char>(cmd[cursor])) != 0)) {
        ++cursor;
    }

    if (cursor >= len) {
        return false;
    }

    size_t start = cursor;
    utils::QuoteState quote_state;

    while (cursor < len) {
        char ch = cmd[cursor];
        auto action = quote_state.consume_forward(ch);
        if (action == utils::QuoteAdvanceResult::Process && !quote_state.inside_quotes() &&
            (std::isspace(static_cast<unsigned char>(ch)) != 0)) {
            break;
        }
        ++cursor;
    }

    token_start = start;
    token_end = cursor;
    return true;
}

bool token_has_explicit_path_hint(const std::string& token) {
    if (token.empty()) {
        return false;
    }

    if (token[0] == '/') {
        return true;
    }

    return token.rfind("./", 0) == 0 || token.rfind("../", 0) == 0 || token.rfind("~/", 0) == 0 ||
           token.rfind("-/", 0) == 0 || token.find('/') != std::string::npos;
}

std::string resolve_token_path(const std::string& token, Shell* shell) {
    std::string path_to_check = token;

    if (token.rfind("~/", 0) == 0) {
        path_to_check = cjsh_filesystem::g_user_home_path().string() + token.substr(1);
    } else if (token.rfind("-/", 0) == 0) {
        if (shell != nullptr) {
            std::string prev_dir = shell->get_previous_directory();
            if (!prev_dir.empty()) {
                path_to_check = prev_dir + token.substr(1);
            }
        }
    } else if (token[0] != '/' && token.rfind("./", 0) != 0 && token.rfind("../", 0) != 0 &&
               token.rfind("~/", 0) != 0 && token.rfind("-/", 0) != 0) {
        path_to_check = cjsh_filesystem::safe_current_directory() + "/" + token;
    }

    return path_to_check;
}

bool token_is_history_expansion(const std::string& token, size_t absolute_cmd_start) {
    if (!config::history_expansion_enabled || token.empty()) {
        return false;
    }

    if (token[0] == '!') {
        return true;
    }

    if (token[0] == '^' && absolute_cmd_start == 0) {
        return true;
    }

    return false;
}

bool is_known_command_token(const std::string& token, size_t absolute_cmd_start, Shell* shell,
                            const std::unordered_set<std::string>& available_commands) {
    using namespace token_classifier;

    if (token.empty()) {
        return true;
    }

    if (is_variable_reference(token)) {
        return true;
    }

    if (token_is_history_expansion(token, absolute_cmd_start)) {
        return true;
    }

    if (token_has_explicit_path_hint(token)) {
        std::string path_to_check = resolve_token_path(token, shell);
        std::error_code ec;
        return std::filesystem::exists(path_to_check, ec);
    }

    if (shell != nullptr && shell->get_interactive_mode()) {
        const auto& abbreviations = shell->get_abbreviations();
        if (abbreviations.find(token) != abbreviations.end()) {
            return true;
        }
    }

    if (is_shell_keyword(token) || is_shell_builtin(token)) {
        return true;
    }

    if (available_commands.find(token) != available_commands.end()) {
        return true;
    }

    if (is_external_command(token)) {
        return true;
    }

    return false;
}

bool has_exited_token_context(const std::string& input, size_t absolute_token_end) {
    if (absolute_token_end >= input.size()) {
        return false;
    }

    char next_char = input[absolute_token_end];
    if ((std::isspace(static_cast<unsigned char>(next_char)) != 0)) {
        return true;
    }

    return next_char == '|' || next_char == '&' || next_char == ';';
}

std::vector<std::string> extract_candidate_commands(const std::vector<std::string>& suggestions) {
    std::vector<std::string> commands;
    commands.reserve(std::min<size_t>(suggestions.size(), 3));

    for (const auto& suggestion : suggestions) {
        size_t first_quote = suggestion.find('\'');
        if (first_quote == std::string::npos) {
            continue;
        }
        size_t second_quote = suggestion.find('\'', first_quote + 1);
        if (second_quote == std::string::npos || second_quote <= first_quote + 1) {
            continue;
        }

        commands.emplace_back(suggestion.substr(first_quote + 1, second_quote - first_quote - 1));
        if (commands.size() == 3) {
            break;
        }
    }

    return commands;
}

UnknownCommandInfo build_unknown_command_info(const std::string& token) {
    UnknownCommandInfo info;
    info.command = token;
    auto suggestions = suggestion_utils::generate_command_suggestions(token);
    info.suggestions = extract_candidate_commands(suggestions);
    return info;
}

std::string sanitize_input_for_analysis(const std::string& input) {
    std::string sanitized = input;
    size_t len = input.size();
    bool in_quotes = false;
    char quote_char = '\0';
    bool escaped = false;

    size_t i = 0;
    while (i < len) {
        char c = input[i];

        if (escaped) {
            escaped = false;
            ++i;
            continue;
        }

        if (c == '\\' && (!in_quotes || quote_char != '\'')) {
            escaped = true;
            ++i;
            continue;
        }

        if ((c == '\"' || c == '\'') && !in_quotes) {
            in_quotes = true;
            quote_char = c;
            ++i;
            continue;
        }

        if (c == quote_char && in_quotes) {
            in_quotes = false;
            quote_char = '\0';
            ++i;
            continue;
        }

        if (!in_quotes && c == '#') {
            size_t comment_end = i;
            while (comment_end < len && input[comment_end] != '\n' && input[comment_end] != '\r') {
                sanitized[comment_end] = ' ';
                comment_end++;
            }
            i = comment_end;
            continue;
        }

        ++i;
    }

    return sanitized;
}

std::optional<UnknownCommandInfo> analyze_command_range(
    Shell* shell, const std::string& original_input, const std::string& analysis,
    const std::unordered_set<std::string>& available_commands, size_t cmd_start, size_t cmd_end) {
    std::string cmd_str = analysis.substr(cmd_start, cmd_end - cmd_start);

    size_t token_cursor = 0;
    size_t first_token_start = 0;
    size_t first_token_end = 0;
    if (!extract_next_token(cmd_str, token_cursor, first_token_start, first_token_end)) {
        return std::nullopt;
    }

    std::string token = cmd_str.substr(first_token_start, first_token_end - first_token_start);
    size_t absolute_token_end = cmd_start + first_token_end;

    bool token_unknown =
        !is_known_command_token(token, cmd_start, shell, available_commands) && !token.empty();
    if (token_unknown && has_exited_token_context(original_input, absolute_token_end)) {
        return build_unknown_command_info(token);
    }

    if (token == "sudo") {
        size_t arg_cursor = token_cursor;
        size_t arg_start = 0;
        size_t arg_end = 0;
        if (extract_next_token(cmd_str, arg_cursor, arg_start, arg_end)) {
            std::string arg = cmd_str.substr(arg_start, arg_end - arg_start);
            size_t absolute_arg_end = cmd_start + arg_end;
            bool arg_unknown =
                !is_known_command_token(arg, cmd_start + arg_start, shell, available_commands) &&
                !arg.empty();
            if (arg_unknown && has_exited_token_context(original_input, absolute_arg_end)) {
                return build_unknown_command_info(arg);
            }
        }
    }

    return std::nullopt;
}

std::optional<UnknownCommandInfo> detect_unknown_command(Shell* shell,
                                                         const std::string& original_input) {
    if (shell == nullptr || original_input.empty()) {
        return std::nullopt;
    }

    std::string analysis = sanitize_input_for_analysis(original_input);
    if (analysis.empty()) {
        return std::nullopt;
    }

    std::unordered_set<std::string> available_commands = shell->get_available_commands();

    size_t len = analysis.length();
    size_t pos = 0;
    while (pos < len) {
        size_t cmd_end = pos;
        utils::QuoteState cmd_quote_state;
        while (cmd_end < len) {
            char current = analysis[cmd_end];
            auto action = cmd_quote_state.consume_forward(current);
            if (action == utils::QuoteAdvanceResult::Process && !cmd_quote_state.inside_quotes()) {
                if ((cmd_end + 1 < len && analysis[cmd_end] == '&' &&
                     analysis[cmd_end + 1] == '&') ||
                    (cmd_end + 1 < len && analysis[cmd_end] == '|' &&
                     analysis[cmd_end + 1] == '|') ||
                    analysis[cmd_end] == '|' || analysis[cmd_end] == ';' ||
                    analysis[cmd_end] == '\n' || analysis[cmd_end] == '\r') {
                    break;
                }
            }
            cmd_end++;
        }

        size_t cmd_start = pos;
        while (cmd_start < cmd_end &&
               (std::isspace(static_cast<unsigned char>(analysis[cmd_start])) != 0)) {
            cmd_start++;
        }

        if (cmd_start < cmd_end) {
            auto unknown_info = analyze_command_range(shell, original_input, analysis,
                                                      available_commands, cmd_start, cmd_end);
            if (unknown_info.has_value()) {
                return unknown_info;
            }
        }

        pos = cmd_end;
        if (pos < len) {
            if (pos + 1 < len && ((analysis[pos] == '&' && analysis[pos + 1] == '&') ||
                                  (analysis[pos] == '|' && analysis[pos + 1] == '|') ||
                                  (analysis[pos] == '>' && analysis[pos + 1] == '>') ||
                                  (analysis[pos] == '<' && analysis[pos + 1] == '<') ||
                                  (analysis[pos] == '&' && analysis[pos + 1] == '>'))) {
                pos += 2;
            } else if (analysis[pos] == '|' || analysis[pos] == ';' || analysis[pos] == '>' ||
                       analysis[pos] == '<' ||
                       (analysis[pos] == '&' && (pos == len - 1 || analysis[pos + 1] != '&'))) {
                pos += 1;
            } else {
                if (analysis[pos] == '\r' && pos + 1 < len && analysis[pos + 1] == '\n') {
                    pos += 2;
                } else {
                    pos += 1;
                }
            }
        }
    }

    return std::nullopt;
}

std::string format_suggestion_list(const std::vector<std::string>& suggestions) {
    if (suggestions.empty()) {
        return {};
    }

    std::vector<std::string> sanitized;
    sanitized.reserve(suggestions.size());
    for (const auto& entry : suggestions) {
        std::string cleaned = sanitize_for_status(entry);
        if (!cleaned.empty()) {
            sanitized.push_back(cleaned);
        }
    }

    if (sanitized.empty()) {
        return {};
    }

    if (sanitized.size() == 1) {
        return sanitized.front();
    }

    if (sanitized.size() == 2) {
        return sanitized[0] + " or " + sanitized[1];
    }

    std::string result = sanitized[0];
    result.append(", ");
    result.append(sanitized[1]);
    result.append(", or ");
    result.append(sanitized[2]);
    return result;
}

std::string format_unknown_command_message(const UnknownCommandInfo& info) {
    std::string sanitized_command = sanitize_for_status(info.command);
    if (sanitized_command.empty()) {
        sanitized_command = info.command;
    }

    std::string message = "Unknown command: ";
    message.append(sanitized_command);

    if (!info.suggestions.empty()) {
        std::string suggestion_text = format_suggestion_list(info.suggestions);
        if (!suggestion_text.empty()) {
            message.append(" | Did you mean: ");
            message.append(suggestion_text);
            message.push_back('?');
        }
    }

    return message;
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

    std::optional<UnknownCommandInfo> unknown_info = detect_unknown_command(shell, current_input);
    std::string validation_message = build_validation_status_message(errors);

    std::string combined_message;
    if (unknown_info.has_value()) {
        combined_message = format_unknown_command_message(*unknown_info);
    }

    if (!validation_message.empty()) {
        if (!combined_message.empty()) {
            combined_message.push_back('\n');
        }
        combined_message.append(validation_message);
    }

    status_message = std::move(combined_message);
    if (status_message.empty()) {
        return nullptr;
    }

    return status_message.c_str();
}

}  // namespace status_line

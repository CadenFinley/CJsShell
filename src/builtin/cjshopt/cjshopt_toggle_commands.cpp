/*
  cjshopt_toggle_commands.cpp

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

#include "cjshopt_command.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "cjsh.h"
#include "cjsh_completions.h"
#include "error_out.h"
#include "isocline.h"
#include "shell_env.h"

namespace {
enum class ToggleRequest : std::uint8_t {
    Enable,
    Disable,
    Status
};

enum class StatusQuery : std::uint8_t {
    Status,
    Value
};

struct ToggleCommandConfig {
    std::string command_name;
    std::vector<std::string> usage_lines;
    std::function<bool()> get_current;
    std::function<void(bool)> set_state;
    std::string status_label;
    bool status_label_is_plural = false;
    std::optional<std::string> persist_template;
    std::vector<std::string> true_synonyms;
    std::vector<std::string> false_synonyms;
};

std::string normalize_option(const std::string& option) {
    std::string normalized = option;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return normalized;
}

bool matches_token(const std::string& value, std::initializer_list<const char*> tokens) {
    return std::any_of(tokens.begin(), tokens.end(),
                       [&](const char* token) { return value == token; });
}

std::optional<ToggleRequest> parse_toggle_request(const ToggleCommandConfig& config,
                                                  const std::string& normalized) {
    if (matches_token(normalized, {"status", "--status"})) {
        return ToggleRequest::Status;
    }
    if (matches_token(normalized, {"on", "enable", "enabled", "true", "1", "--enable"})) {
        return ToggleRequest::Enable;
    }
    if (matches_token(normalized, {"off", "disable", "disabled", "false", "0", "--disable"})) {
        return ToggleRequest::Disable;
    }

    if (std::any_of(config.true_synonyms.begin(), config.true_synonyms.end(),
                    [&](const std::string& token) { return normalized == token; })) {
        return ToggleRequest::Enable;
    }
    if (std::any_of(config.false_synonyms.begin(), config.false_synonyms.end(),
                    [&](const std::string& token) { return normalized == token; })) {
        return ToggleRequest::Disable;
    }

    return std::nullopt;
}

StatusQuery parse_status_query(const std::string& normalized) {
    if (matches_token(normalized, {"status", "--status"})) {
        return StatusQuery::Status;
    }
    return StatusQuery::Value;
}

std::string format_persist_message(const ToggleCommandConfig& config, bool enable) {
    if (!config.persist_template) {
        return {};
    }

    std::string result = *config.persist_template;
    const std::string state_word = enable ? "on" : "off";

    auto replace_all = [](std::string* target, const std::string& from, const std::string& to) {
        size_t position = 0;
        while ((position = target->find(from, position)) != std::string::npos) {
            target->replace(position, from.size(), to);
            position += to.size();
        }
    };

    replace_all(&result, "{command}", config.command_name);
    replace_all(&result, "{state}", state_word);

    return result;
}

std::string describe_status_hint_mode(ic_status_hint_mode_t mode) {
    switch (mode) {
        case IC_STATUS_HINT_OFF:
            return "hidden (never shown)";
        case IC_STATUS_HINT_NORMAL:
            return "normal (default: only when input and status are empty)";
        case IC_STATUS_HINT_TRANSIENT:
            return "transient (show when the status line is empty)";
        case IC_STATUS_HINT_PERSISTENT:
            return "persistent (always prepended above status lines)";
        default:
            return "unknown";
    }
}

const char* canonical_status_hint_token(ic_status_hint_mode_t mode) {
    switch (mode) {
        case IC_STATUS_HINT_OFF:
            return "off";
        case IC_STATUS_HINT_NORMAL:
            return "normal";
        case IC_STATUS_HINT_TRANSIENT:
            return "transient";
        case IC_STATUS_HINT_PERSISTENT:
            return "persistent";
        default:
            return "normal";
    }
}

bool g_status_hint_preference_initialized = false;
ic_status_hint_mode_t g_status_hint_preference = IC_STATUS_HINT_NORMAL;

void ensure_status_hint_preference_initialized() {
    if (!g_status_hint_preference_initialized) {
        g_status_hint_preference = ic_get_status_hint_mode();
        g_status_hint_preference_initialized = true;
    }
}

void apply_effective_status_hint_mode() {
    ensure_status_hint_preference_initialized();
    if (config::status_line_enabled) {
        ic_set_status_hint_mode(g_status_hint_preference);
    } else {
        ic_set_status_hint_mode(IC_STATUS_HINT_OFF);
    }
}

int handle_toggle_command(const ToggleCommandConfig& config, const std::vector<std::string>& args) {
    if (args.size() == 1) {
        print_error({ErrorType::INVALID_ARGUMENT, config.command_name, "Missing option argument",
                     config.usage_lines});
        return 1;
    }

    if (args.size() == 2 && (args[1] == "--help" || args[1] == "-h")) {
        if (!g_startup_active) {
            for (const auto& line : config.usage_lines) {
                std::cout << line << '\n';
            }
            std::cout << "Current: " << (config.get_current() ? "enabled" : "disabled") << '\n';
        }
        return 0;
    }

    if (args.size() != 2) {
        print_error({ErrorType::INVALID_ARGUMENT, config.command_name,
                     "Too many arguments provided", config.usage_lines});
        return 1;
    }

    const std::string& option = args[1];
    const std::string normalized = normalize_option(option);

    auto request = parse_toggle_request(config, normalized);
    if (!request.has_value()) {
        print_error({ErrorType::INVALID_ARGUMENT, config.command_name,
                     "Unknown option '" + option + "'", config.usage_lines});
        return 1;
    }

    if (*request == ToggleRequest::Status) {
        if (!g_startup_active) {
            const char* verb = config.status_label_is_plural ? "are" : "is";
            std::cout << config.status_label << ' ' << verb << " currently "
                      << (config.get_current() ? "enabled" : "disabled") << ".\n";
        }
        return 0;
    }

    bool enable = (*request == ToggleRequest::Enable);

    const bool previously_enabled = config.get_current();
    if (previously_enabled == enable) {
        return 0;
    }

    config.set_state(enable);

    if (!g_startup_active) {
        std::cout << config.status_label << ' ' << (enable ? "enabled" : "disabled") << ".\n";
        const std::string extra = format_persist_message(config, enable);
        if (!extra.empty()) {
            std::cout << extra;
        }
    }

    return 0;
}
}  // namespace

int current_line_number_highlight_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: current-line-number-highlight <on|off|status>", "Examples:",
        "  current-line-number-highlight on      Enable highlighting of the current line number",
        "  current-line-number-highlight off     Disable highlighting of the current line number",
        "  current-line-number-highlight status  Show the current setting"};

    static const ToggleCommandConfig config{
        "current-line-number-highlight",
        usage_lines,
        []() { return ic_current_line_number_highlight_is_enabled(); },
        [](bool enable) { ic_enable_current_line_number_highlight(enable); },
        "Current line number highlighting",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

int completion_case_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: completion-case <on|off|status>",
        "Examples:", "  completion-case on       Enable case sensitive completions",
        "  completion-case off      Use case insensitive completions",
        "  completion-case status   Show the current setting"};

    static const ToggleCommandConfig config{
        "completion-case",
        usage_lines,
        []() { return is_completion_case_sensitive(); },
        [](bool enable) { set_completion_case_sensitive(enable); },
        "Completion case sensitivity",
        false,
        std::nullopt,
        {"case-sensitive", "--case-sensitive"},
        {"case-insensitive", "--case-insensitive"}};

    return handle_toggle_command(config, args);
}

int history_search_case_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: history-search-case <on|off|status>", "Examples:",
        "  history-search-case on       Require exact case matches in fuzzy history search",
        "  history-search-case off      Match history entries case insensitively",
        "  history-search-case status   Show the current setting"};

    static const ToggleCommandConfig config{
        "history-search-case",
        usage_lines,
        []() { return ic_history_fuzzy_search_is_case_sensitive(); },
        [](bool enable) { ic_enable_history_fuzzy_case_sensitive(enable); },
        "History search case sensitivity",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {"case-sensitive", "--case-sensitive"},
        {"case-insensitive", "--case-insensitive"}};

    return handle_toggle_command(config, args);
}

int completion_spell_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: completion-spell <on|off|status>",
        "Examples:", "  completion-spell on      Enable spell correction in completions",
        "  completion-spell off     Disable spell correction in completions",
        "  completion-spell status  Show the current setting"};

    static const ToggleCommandConfig config{
        "completion-spell",
        usage_lines,
        []() { return is_completion_spell_correction_enabled(); },
        [](bool enable) { set_completion_spell_correction_enabled(enable); },
        "Completion spell correction",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {"spell", "--spell"},
        {}};

    return handle_toggle_command(config, args);
}

int completion_learning_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: completion-learning <on|off|status>", "Examples:",
        "  completion-learning on      Allow cjsh to learn completions as you use commands",
        "  completion-learning off     Only use cached completions (run generate-completions)",
        "  completion-learning status  Show the current setting"};

    static const ToggleCommandConfig config{
        "completion-learning",
        usage_lines,
        []() { return config::completion_learning_enabled; },
        [](bool enable) { config::completion_learning_enabled = enable; },
        "Completion learning",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

int smart_cd_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: smart-cd <on|off|status>",
        "Examples:", "  smart-cd on      Enable smart cd auto-jumps",
        "  smart-cd off     Disable smart cd auto-jumps",
        "  smart-cd status  Show the current setting"};

    static const ToggleCommandConfig config{
        "smart-cd",
        usage_lines,
        []() { return config::smart_cd_enabled; },
        [](bool enable) { config::smart_cd_enabled = enable; },
        "Smart cd",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

int script_extension_interpreter_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: script-extension-interpreter <on|off|status>",
        "Examples:", "  script-extension-interpreter on      Enable extension-based script runners",
        "  script-extension-interpreter off     Disable extension-based script runners",
        "  script-extension-interpreter status  Show the current setting"};

    static const ToggleCommandConfig config{
        "script-extension-interpreter",
        usage_lines,
        []() { return config::script_extension_interpreter_enabled; },
        [](bool enable) { config::script_extension_interpreter_enabled = enable; },
        "Script extension interpreter",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

int line_numbers_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: line-numbers <on|off|relative|absolute|status>",
        "Examples:",
        "  line-numbers on        Enable absolute line numbers in multiline input",
        "  line-numbers relative  Enable relative line numbers in multiline input",
        "  line-numbers off       Disable line numbers in multiline input",
        "  line-numbers status    Show the current setting"};

    const auto describe_status = []() {
        if (!ic_line_numbers_are_enabled()) {
            return std::string("Line numbers are currently disabled.");
        }
        if (ic_line_numbers_are_relative()) {
            return std::string("Line numbers are currently enabled (relative numbering).");
        }
        return std::string("Line numbers are currently enabled (absolute numbering).");
    };

    if (args.size() == 1) {
        print_error(
            {ErrorType::INVALID_ARGUMENT, "line-numbers", "Missing option argument", usage_lines});
        return 1;
    }

    if (args.size() == 2 && (args[1] == "--help" || args[1] == "-h")) {
        if (!g_startup_active) {
            for (const auto& line : usage_lines) {
                std::cout << line << '\n';
            }
        }
        return 0;
    }

    if (args.size() != 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "line-numbers", "Too many arguments provided",
                     usage_lines});
        return 1;
    }

    const std::string& option = args[1];
    const std::string normalized = normalize_option(option);

    enum class LineNumbersMode : std::uint8_t {
        Status,
        Off,
        Relative,
        Absolute
    };

    auto parse_line_numbers_mode = [&](const std::string& value) -> std::optional<LineNumbersMode> {
        if (matches_token(value, {"status", "--status"})) {
            return LineNumbersMode::Status;
        }
        if (matches_token(value, {"off", "disable", "disabled", "false", "0", "--disable"})) {
            return LineNumbersMode::Off;
        }
        if (matches_token(value, {"relative", "rel", "--relative"})) {
            return LineNumbersMode::Relative;
        }
        if (matches_token(value, {"absolute", "abs", "--absolute"}) ||
            matches_token(value, {"on", "enable", "enabled", "true", "1", "--enable"})) {
            return LineNumbersMode::Absolute;
        }
        return std::nullopt;
    };

    auto mode = parse_line_numbers_mode(normalized);
    if (!mode.has_value()) {
        print_error({ErrorType::INVALID_ARGUMENT, "line-numbers", "Unknown option '" + option + "'",
                     usage_lines});
        return 1;
    }

    if (*mode == LineNumbersMode::Status) {
        if (!g_startup_active) {
            std::cout << describe_status() << '\n';
        }
        return 0;
    }

    const bool was_enabled = ic_line_numbers_are_enabled();
    const bool was_relative = ic_line_numbers_are_relative();
    bool changed = false;

    switch (*mode) {
        case LineNumbersMode::Off:
            ic_enable_line_numbers(false);
            changed = (was_enabled || was_relative);
            break;
        case LineNumbersMode::Relative:
            ic_enable_relative_line_numbers(true);
            changed = (!was_enabled || !was_relative);
            break;
        case LineNumbersMode::Absolute:
            ic_enable_line_numbers(true);
            ic_enable_relative_line_numbers(false);
            changed = (!was_enabled || was_relative);
            break;
        case LineNumbersMode::Status:
            break;
    }

    if (!g_startup_active && changed) {
        std::cout << describe_status() << '\n';
        std::string persist_token;
        if (!ic_line_numbers_are_enabled()) {
            persist_token = "off";
        } else if (ic_line_numbers_are_relative()) {
            persist_token = "relative";
        } else {
            persist_token = "absolute";
        }
        std::cout << "Add `cjshopt line-numbers " << persist_token
                  << "` to your ~/.cjshrc to persist this change.\n";
    }

    return 0;
}

int line_numbers_continuation_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: line-numbers-continuation <on|off|status>", "Examples:",
        "  line-numbers-continuation on       Keep line numbers when a continuation prompt is set",
        "  line-numbers-continuation off      Hide line numbers whenever a continuation prompt is "
        "set",
        "  line-numbers-continuation status   Show the current setting"};

    static const ToggleCommandConfig config{
        "line-numbers-continuation",
        usage_lines,
        []() { return ic_line_numbers_with_continuation_prompt_are_enabled(); },
        [](bool enable) { ic_enable_line_numbers_with_continuation_prompt(enable); },
        "Line numbers with continuation prompts",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

int line_numbers_replace_prompt_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: line-numbers-replace-prompt <on|off|status>", "Examples:",
        "  line-numbers-replace-prompt on      Replace the final prompt line with line numbers",
        "  line-numbers-replace-prompt off     Keep the final prompt line visible",
        "  line-numbers-replace-prompt status  Show the current setting"};

    static const ToggleCommandConfig config{
        "line-numbers-replace-prompt",
        usage_lines,
        []() { return ic_line_number_prompt_replacement_is_enabled(); },
        [](bool enable) { ic_enable_line_number_prompt_replacement(enable); },
        "Line number prompt replacement",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

int hint_delay_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: hint-delay <milliseconds>",
        "Examples:", "  hint-delay 100    Set hint delay to 100 milliseconds",
        "  hint-delay 0      Show hints immediately",
        "  hint-delay status Show the current delay setting"};

    if (args.size() == 1) {
        print_error(
            {ErrorType::INVALID_ARGUMENT, "hint-delay", "Missing delay value", usage_lines});
        return 1;
    }

    if (args.size() == 2 && (args[1] == "--help" || args[1] == "-h")) {
        if (!g_startup_active) {
            for (const auto& line : usage_lines) {
                std::cout << line << '\n';
            }
        }
        return 0;
    }

    if (args.size() != 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "hint-delay", "Too many arguments provided",
                     usage_lines});
        return 1;
    }

    const std::string& option = args[1];
    std::string normalized = option;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (parse_status_query(normalized) == StatusQuery::Status) {
        if (!g_startup_active) {
            std::cout << "To check or modify hint delay, use: hint-delay <milliseconds>\n";
        }
        return 0;
    }

    try {
        long delay_ms = std::stol(option);
        if (delay_ms < 0) {
            print_error({ErrorType::INVALID_ARGUMENT, "hint-delay", "Delay must be non-negative",
                         usage_lines});
            return 1;
        }

        ic_set_hint_delay(delay_ms);

        if (!g_startup_active) {
            std::cout << "Hint delay set to " << delay_ms << " milliseconds.\n";
            std::cout << "Add `cjshopt hint-delay " << delay_ms
                      << "` to your ~/.cjshrc to persist this change.\n";
        }
        return 0;
    } catch (...) {
        print_error({ErrorType::INVALID_ARGUMENT, "hint-delay",
                     "Invalid delay value '" + option + "' (expected a number)", usage_lines});
        return 1;
    }
}

int multiline_start_lines_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: multiline-start-lines <count>",
        "Examples:", "  multiline-start-lines 1    Start editing on the first prompt line",
        "  multiline-start-lines 2    Start with two prompt lines (cursor on line 2)",
        "  multiline-start-lines status   Show the current setting"};

    if (args.size() == 1) {
        print_error({ErrorType::INVALID_ARGUMENT, "multiline-start-lines", "Missing line count",
                     usage_lines});
        return 1;
    }

    if (args.size() == 2 && (args[1] == "--help" || args[1] == "-h")) {
        if (!g_startup_active) {
            for (const auto& line : usage_lines) {
                std::cout << line << '\n';
            }
        }
        return 0;
    }

    if (args.size() != 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "multiline-start-lines",
                     "Too many arguments provided", usage_lines});
        return 1;
    }

    const std::string& option = args[1];
    std::string normalized = option;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (parse_status_query(normalized) == StatusQuery::Status) {
        if (!g_startup_active) {
            const size_t current = ic_get_multiline_start_line_count();
            std::cout << "Multiline prompts currently start with " << current << " line"
                      << (current == 1 ? "" : "s") << ".\n";
        }
        return 0;
    }

    size_t requested = 0;
    try {
        unsigned long parsed = std::stoul(option);
        requested = static_cast<size_t>(parsed);
    } catch (...) {
        print_error({ErrorType::INVALID_ARGUMENT, "multiline-start-lines",
                     "Invalid line count '" + option + "' (expected a positive integer)",
                     usage_lines});
        return 1;
    }

    if (requested == 0) {
        print_error({ErrorType::INVALID_ARGUMENT, "multiline-start-lines",
                     "Line count must be at least 1", usage_lines});
        return 1;
    }

    ic_set_multiline_start_line_count(requested);
    const size_t applied = ic_get_multiline_start_line_count();

    if (!g_startup_active) {
        if (applied != requested) {
            std::cout << "Line count exceeds the supported maximum; using " << applied
                      << " instead.\n";
        }
        std::cout << "Multiline prompts will now start with " << applied << " line"
                  << (applied == 1 ? "" : "s") << ".\n";
        std::cout << "Add `cjshopt multiline-start-lines " << applied
                  << "` to your ~/.cjshrc to persist this change.\n";
    }

    return 0;
}

int completion_preview_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: completion-preview <on|off|status>",
        "Examples:", "  completion-preview on      Enable completion preview",
        "  completion-preview off     Disable completion preview",
        "  completion-preview status  Show the current setting"};

    static const ToggleCommandConfig config{
        "completion-preview",
        usage_lines,
        []() {
            bool current_status = ic_enable_completion_preview(true);
            ic_enable_completion_preview(current_status);
            return current_status;
        },
        [](bool enable) { ic_enable_completion_preview(enable); },
        "Completion preview",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

int visible_whitespace_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: visible-whitespace <on|off|status>",
        "Examples:", "  visible-whitespace on      Show whitespace characters while editing",
        "  visible-whitespace off     Hide whitespace characters while editing",
        "  visible-whitespace status  Show the current setting"};

    static const ToggleCommandConfig config{
        "visible-whitespace",
        usage_lines,
        []() {
            bool current_status = ic_enable_visible_whitespace(true);
            ic_enable_visible_whitespace(current_status);
            return current_status;
        },
        [](bool enable) { ic_enable_visible_whitespace(enable); },
        "Visible whitespace characters",
        true,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

int hint_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: hint <on|off|status>", "Examples:", "  hint on      Enable inline hints",
        "  hint off     Disable inline hints", "  hint status  Show the current setting"};

    static const ToggleCommandConfig config{
        "hint",
        usage_lines,
        []() {
            bool current_status = ic_enable_hint(true);
            ic_enable_hint(current_status);
            return current_status;
        },
        [](bool enable) { ic_enable_hint(enable); },
        "Inline hints",
        true,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

int multiline_indent_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: multiline-indent <on|off|status>",
        "Examples:", "  multiline-indent on      Enable automatic indentation in multiline",
        "  multiline-indent off     Disable automatic indentation in multiline",
        "  multiline-indent status  Show the current setting"};

    static const ToggleCommandConfig config{
        "multiline-indent",
        usage_lines,
        []() {
            bool current_status = ic_enable_multiline_indent(true);
            ic_enable_multiline_indent(current_status);
            return current_status;
        },
        [](bool enable) { ic_enable_multiline_indent(enable); },
        "Multiline auto-indent",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

int multiline_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: multiline <on|off|status>",
        "Examples:", "  multiline on      Enable multiline input",
        "  multiline off     Disable multiline input",
        "  multiline status  Show the current setting"};

    static const ToggleCommandConfig config{
        "multiline",
        usage_lines,
        []() {
            bool current_status = ic_enable_multiline(true);
            ic_enable_multiline(current_status);
            return current_status;
        },
        [](bool enable) { ic_enable_multiline(enable); },
        "Multiline input",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

int inline_help_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: inline-help <on|off|status>",
        "Examples:", "  inline-help on      Enable inline help messages",
        "  inline-help off     Disable inline help messages",
        "  inline-help status  Show the current setting"};

    static const ToggleCommandConfig config{
        "inline-help",
        usage_lines,
        []() {
            bool current_status = ic_enable_inline_help(true);
            ic_enable_inline_help(current_status);
            return current_status;
        },
        [](bool enable) { ic_enable_inline_help(enable); },
        "Inline help messages",
        true,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

int status_hints_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: status-hints <off|normal|transient|persistent|status>",
        "Examples:",
        "  status-hints off          Never display the underlined status hints",
        "  status-hints normal       Only show hints when the buffer and status are blank "
        "(default)",
        "  status-hints transient    Show hints when the status line is empty",
        "  status-hints persistent   Always prepend hints above other status messages",
        "  status-hints status       Show the current mode"};

    ensure_status_hint_preference_initialized();

    if (args.size() == 1) {
        print_error(
            {ErrorType::INVALID_ARGUMENT, "status-hints", "Missing option argument", usage_lines});
        return 1;
    }

    if (args.size() == 2 && (args[1] == "--help" || args[1] == "-h")) {
        if (!g_startup_active) {
            for (const auto& line : usage_lines) {
                std::cout << line << '\n';
            }
            std::cout << "Current: " << describe_status_hint_mode(ic_get_status_hint_mode())
                      << '\n';
        }
        return 0;
    }

    if (args.size() != 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "status-hints", "Too many arguments provided",
                     usage_lines});
        return 1;
    }

    const std::string& option = args[1];
    const std::string normalized = normalize_option(option);

    enum class StatusHintsMode : std::uint8_t {
        Status,
        Off,
        Normal,
        Transient,
        Persistent
    };

    auto parse_status_hints_mode = [&](const std::string& value) -> std::optional<StatusHintsMode> {
        if (matches_token(value, {"status", "--status"})) {
            return StatusHintsMode::Status;
        }
        if (matches_token(value, {"off", "disable", "disabled", "never", "hidden", "--disable"})) {
            return StatusHintsMode::Off;
        }
        if (matches_token(value, {"normal", "minimal", "empty-only", "default"})) {
            return StatusHintsMode::Normal;
        }
        if (matches_token(value, {"transient", "auto"})) {
            return StatusHintsMode::Transient;
        }
        if (matches_token(value, {"persistent", "always", "always-on", "on"})) {
            return StatusHintsMode::Persistent;
        }
        return std::nullopt;
    };

    auto mode = parse_status_hints_mode(normalized);
    if (!mode.has_value()) {
        print_error({ErrorType::INVALID_ARGUMENT, "status-hints", "Unknown option '" + option + "'",
                     usage_lines});
        return 1;
    }

    if (*mode == StatusHintsMode::Status) {
        if (!g_startup_active) {
            if (config::status_line_enabled) {
                std::cout << "Status hints are currently "
                          << describe_status_hint_mode(g_status_hint_preference) << ".\n";
            } else {
                std::cout << "Status hints preference is "
                          << describe_status_hint_mode(g_status_hint_preference)
                          << ", but the status line toggle is off so the banner stays hidden.\n";
            }
        }
        return 0;
    }

    ic_status_hint_mode_t target = IC_STATUS_HINT_NORMAL;
    switch (*mode) {
        case StatusHintsMode::Off:
            target = IC_STATUS_HINT_OFF;
            break;
        case StatusHintsMode::Normal:
            target = IC_STATUS_HINT_NORMAL;
            break;
        case StatusHintsMode::Transient:
            target = IC_STATUS_HINT_TRANSIENT;
            break;
        case StatusHintsMode::Persistent:
            target = IC_STATUS_HINT_PERSISTENT;
            break;
        case StatusHintsMode::Status:
            break;
    }

    const bool preference_changed = (g_status_hint_preference != target);
    g_status_hint_preference = target;

    if (!preference_changed) {
        if (!g_startup_active && !config::status_line_enabled) {
            std::cout << "Status hints stay hidden because the status line toggle is off.\n";
        }
        return 0;
    }

    apply_effective_status_hint_mode();

    if (!g_startup_active) {
        if (config::status_line_enabled) {
            std::cout << "Status hints set to " << describe_status_hint_mode(target) << ".\n";
            std::cout << "Add `cjshopt status-hints " << canonical_status_hint_token(target)
                      << "` to your ~/.cjshrc to persist this change.\n";
        } else {
            std::cout << "Stored status hint mode set to " << describe_status_hint_mode(target)
                      << ", but the status line toggle is off so nothing is shown.\n";
            std::cout << "Re-enable it with `cjshopt status-line on` to display the banner.\n";
        }
    }

    return 0;
}

int status_line_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: status-line <on|off|status>",
        "Examples:", "  status-line on      Show the status area below the prompt",
        "  status-line off     Hide the status area entirely",
        "  status-line status  Show the current setting"};

    static const ToggleCommandConfig config{
        "status-line",
        usage_lines,
        []() { return config::status_line_enabled; },
        [](bool enable) {
            config::status_line_enabled = enable;
            apply_effective_status_hint_mode();
        },
        "Status line",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

int status_reporting_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: status-reporting <on|off|status>",
        "Examples:", "  status-reporting on      Show cjsh validation output in the status row",
        "  status-reporting off     Hide validation and error reporting",
        "  status-reporting status  Show the current setting"};

    static const ToggleCommandConfig command_config{
        "status-reporting",
        usage_lines,
        []() { return config::status_reporting_enabled; },
        [](bool enable) { config::status_reporting_enabled = enable; },
        "Status reporting",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(command_config, args);
}

int auto_tab_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: auto-tab <on|off|status>",
        "Examples:", "  auto-tab on      Enable automatic tab completion",
        "  auto-tab off     Disable automatic tab completion",
        "  auto-tab status  Show the current setting"};

    static const ToggleCommandConfig config{
        "auto-tab",
        usage_lines,
        []() {
            bool current_status = ic_enable_auto_tab(true);
            ic_enable_auto_tab(current_status);
            return current_status;
        },
        [](bool enable) { ic_enable_auto_tab(enable); },
        "Automatic tab completion",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

int prompt_newline_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: prompt-newline <on|off|status>",
        "Examples:", "  prompt-newline on      Add a newline after each command",
        "  prompt-newline off     Disable newlines after commands",
        "  prompt-newline status  Show the current setting"};

    static const ToggleCommandConfig config{
        "prompt-newline",
        usage_lines,
        []() { return config::newline_after_execution; },
        [](bool enable) { config::newline_after_execution = enable; },
        "Post-execution newline",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

int prompt_cleanup_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: prompt-cleanup <on|off|status>",
        "Examples:", "  prompt-cleanup on      Enable prompt cleanup",
        "  prompt-cleanup off     Disable prompt cleanup",
        "  prompt-cleanup status  Show the current setting"};

    static const ToggleCommandConfig config{
        "prompt-cleanup",
        usage_lines,
        []() { return ic_prompt_cleanup_is_enabled(); },
        [](bool enable) {
            size_t extra_lines = ic_prompt_cleanup_extra_lines();
            ic_enable_prompt_cleanup(enable, extra_lines);
        },
        "Prompt cleanup",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

int prompt_cleanup_newline_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: prompt-cleanup-newline <on|off|status>",
        "Examples:", "  prompt-cleanup-newline on      Add cleanup newline before execution",
        "  prompt-cleanup-newline off     Disable cleanup newline",
        "  prompt-cleanup-newline status  Show the current setting"};

    static const ToggleCommandConfig config{
        "prompt-cleanup-newline",
        usage_lines,
        []() { return ic_prompt_cleanup_newline_is_enabled(); },
        [](bool enable) { ic_enable_prompt_cleanup_newline(enable); },
        "Prompt cleanup newline",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

int prompt_cleanup_empty_line_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: prompt-cleanup-empty-line <on|off|status>",
        "Examples:", "  prompt-cleanup-empty-line on      Insert an empty line after cleanup",
        "  prompt-cleanup-empty-line off     Keep prompt cleanup compact",
        "  prompt-cleanup-empty-line status  Show the current setting"};

    static const ToggleCommandConfig config{
        "prompt-cleanup-empty-line",
        usage_lines,
        []() { return ic_prompt_cleanup_empty_line_is_enabled(); },
        [](bool enable) { ic_enable_prompt_cleanup_empty_line(enable); },
        "Prompt cleanup empty line",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

int prompt_cleanup_truncate_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: prompt-cleanup-truncate <on|off|status>",
        "Examples:", "  prompt-cleanup-truncate on      Truncate multiline prompts during cleanup",
        "  prompt-cleanup-truncate off     Preserve multiline prompts during cleanup",
        "  prompt-cleanup-truncate status  Show the current setting"};

    static const ToggleCommandConfig config{
        "prompt-cleanup-truncate",
        usage_lines,
        []() { return ic_prompt_cleanup_truncate_multiline_is_enabled(); },
        [](bool enable) { ic_enable_prompt_cleanup_truncate_multiline(enable); },
        "Prompt cleanup truncation",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

int right_prompt_follow_cursor_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: right-prompt-follow-cursor <on|off|status>", "Examples:",
        "  right-prompt-follow-cursor on      Move the inline right prompt with the cursor",
        "  right-prompt-follow-cursor off     Pin the inline right prompt to the first row",
        "  right-prompt-follow-cursor status  Show the current setting"};

    static const ToggleCommandConfig config{
        "right-prompt-follow-cursor",
        usage_lines,
        []() { return ic_inline_right_prompt_follows_cursor(); },
        [](bool enable) { ic_enable_inline_right_prompt_cursor_follow(enable); },
        "Right prompt cursor tracking",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

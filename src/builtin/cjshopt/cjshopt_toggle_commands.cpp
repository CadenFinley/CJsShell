#include "cjshopt_command.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "cjsh.h"
#include "cjsh_completions.h"
#include "error_out.h"
#include "isocline/isocline.h"

namespace {
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

bool parse_toggle_value(const ToggleCommandConfig& config, const std::string& normalized,
                        bool* enable) {
    if (matches_token(normalized, {"on", "enable", "enabled", "true", "1", "--enable"})) {
        *enable = true;
        return true;
    }
    if (matches_token(normalized, {"off", "disable", "disabled", "false", "0", "--disable"})) {
        *enable = false;
        return true;
    }

    if (std::any_of(config.true_synonyms.begin(), config.true_synonyms.end(),
                    [&](const std::string& token) { return normalized == token; })) {
        *enable = true;
        return true;
    }
    if (std::any_of(config.false_synonyms.begin(), config.false_synonyms.end(),
                    [&](const std::string& token) { return normalized == token; })) {
        *enable = false;
        return true;
    }

    return false;
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

    if (normalized == "status" || normalized == "--status") {
        if (!g_startup_active) {
            const char* verb = config.status_label_is_plural ? "are" : "is";
            std::cout << config.status_label << ' ' << verb << " currently "
                      << (config.get_current() ? "enabled" : "disabled") << ".\n";
        }
        return 0;
    }

    bool enable = false;
    if (!parse_toggle_value(config, normalized, &enable)) {
        print_error({ErrorType::INVALID_ARGUMENT, config.command_name,
                     "Unknown option '" + option + "'", config.usage_lines});
        return 1;
    }

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

    if (matches_token(normalized, {"status", "--status"})) {
        if (!g_startup_active) {
            std::cout << describe_status() << '\n';
        }
        return 0;
    }

    const bool was_enabled = ic_line_numbers_are_enabled();
    const bool was_relative = ic_line_numbers_are_relative();
    bool changed = false;

    if (matches_token(normalized, {"off", "disable", "disabled", "false", "0", "--disable"})) {
        ic_enable_line_numbers(false);
        changed = (was_enabled || was_relative);
    } else if (matches_token(normalized, {"relative", "rel", "--relative"})) {
        ic_enable_relative_line_numbers(true);
        changed = (!was_enabled || !was_relative);
    } else if (matches_token(normalized, {"absolute", "abs", "--absolute"}) ||
               matches_token(normalized, {"on", "enable", "enabled", "true", "1", "--enable"})) {
        ic_enable_line_numbers(true);
        ic_enable_relative_line_numbers(false);
        changed = (!was_enabled || was_relative);
    } else {
        print_error({ErrorType::INVALID_ARGUMENT, "line-numbers", "Unknown option '" + option + "'",
                     usage_lines});
        return 1;
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

    std::string option = args[1];
    std::string normalized = option;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (normalized == "status" || normalized == "--status") {
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

    std::string option = args[1];
    std::string normalized = option;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (normalized == "status" || normalized == "--status") {
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
        []() { return config::uses_cleanup; },
        [](bool enable) { config::uses_cleanup = enable; },
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
        []() { return config::cleanup_newline_after_execution; },
        [](bool enable) { config::cleanup_newline_after_execution = enable; },
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
        []() { return config::cleanup_adds_empty_line; },
        [](bool enable) { config::cleanup_adds_empty_line = enable; },
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
        []() { return config::cleanup_truncates_multiline; },
        [](bool enable) { config::cleanup_truncates_multiline = enable; },
        "Prompt cleanup truncation",
        false,
        "Add `cjshopt {command} {state}` to your ~/.cjshrc to persist this change.\n",
        {},
        {}};

    return handle_toggle_command(config, args);
}

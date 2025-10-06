#include "cjshopt_command.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include "bookmark_database.h"
#include "cjsh.h"
#include "cjsh_completions.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "isocline/isocline.h"

namespace {
void print_cjshopt_usage() {
    std::cout << "Usage: cjshopt <subcommand> [options]\n";
    std::cout << "Available subcommands:\n";
    std::cout
        << "  style_def <token_type> <style>   Define or redefine a syntax highlighting style\n";
    std::cout << "  login-startup-arg [--flag-name]  Add a startup flag (config file only)\n";
    std::cout << "  completion-case <on|off|status>  Configure completion case sensitivity (default: enabled)\n";
    std::cout << "  completion-spell <on|off|status> Configure completion spell correction (default: enabled)\n";
    std::cout << "  line-numbers <on|off|relative|absolute|status>    Configure line numbers in multiline input (default: enabled)\n";
    std::cout << "  hint-delay <milliseconds>        Set hint display delay in milliseconds\n";
    std::cout << "  completion-preview <on|off|status> Configure completion preview (default: enabled)\n";
    std::cout << "  hint <on|off|status>            Configure inline hints (default: enabled)\n";
    std::cout << "  multiline-indent <on|off|status> Configure auto-indent in multiline (default: enabled)\n";
    std::cout << "  multiline <on|off|status>       Configure multiline input (default: enabled)\n";
    std::cout << "  inline-help <on|off|status>     Configure inline help messages (default: enabled)\n";
    std::cout << "  auto-tab <on|off|status>        Configure automatic tab completion (default: enabled)\n";
    std::cout << "  keybind <subcommand> [...]       Inspect or modify key bindings (modifications "
                 "in config only)\n";
    std::cout << "  generate-profile [--force]       Create or overwrite ~/.cjprofile\n";
    std::cout << "  generate-rc [--force]            Create or overwrite ~/.cjshrc\n";
    std::cout << "  generate-logout [--force]        Create or overwrite ~/.cjsh_logout\n";
    std::cout << "  set-max-bookmarks <number>       Limit stored directory bookmarks (10-1000)\n";
    std::cout << "  set-history-max <number|default|status> Configure history persistence\n";
    std::cout
        << "  bookmark-blacklist <subcommand>  Manage directories excluded from bookmarking\n";
    std::cout << "Use 'cjshopt <subcommand> --help' to see usage for a specific subcommand.\n";
}

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
        print_error({ErrorType::INVALID_ARGUMENT, config.command_name, "Too many arguments provided",
                     config.usage_lines});
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

int cjshopt_command(const std::vector<std::string>& args) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) {
        if (!g_startup_active) {
            print_cjshopt_usage();
        }
        return 0;
    }

    if (args.size() < 2) {
        print_error(
            {ErrorType::INVALID_ARGUMENT,
             "cjshopt",
             "Missing subcommand argument",
             {
                 "Usage: cjshopt <subcommand> [options]",
                 "Available subcommands:",
                 "  style_def <token_type> <style>   Define or redefine a syntax "
                 "highlighting style",
                 "  login-startup-arg [--flag-name]  Add a startup flag "
                 "(config file only)",
                 "  completion-case <on|off|status>  Configure completion case "
                 "sensitivity (default: disabled)",
                 "  completion-spell <on|off|status> Configure completion spell correction (default: enabled)",
                 "  line-numbers <on|off|relative|absolute|status>    Configure line numbers in multiline input (default: enabled)",
                 "  hint-delay <milliseconds>        Set hint display delay in milliseconds",
                 "  completion-preview <on|off|status> Configure completion preview (default: enabled)",
                 "  hint <on|off|status>            Configure inline hints (default: enabled)",
                 "  multiline-indent <on|off|status> Configure auto-indent in multiline (default: enabled)",
                 "  multiline <on|off|status>       Configure multiline input (default: enabled)",
                 "  inline-help <on|off|status>     Configure inline help messages (default: enabled)",
                 "  auto-tab <on|off|status>        Configure automatic tab completion (default: enabled)",
                 "  keybind <subcommand> [...]       Inspect or modify key bindings "
                 "(modifications in config only)",
                 "  generate-profile [--force]       Create or overwrite ~/.cjprofile",
                 "  generate-rc [--force]            Create or overwrite ~/.cjshrc",
                 "  generate-logout [--force]        Create or overwrite ~/.cjsh_logout",
                 "  set-max-bookmarks <number>       Limit stored directory bookmarks (10-1000)",
                 "  set-history-max <number|default|status> Configure history persistence",
                 "  bookmark-blacklist <subcommand>  Manage directories excluded from bookmarking",
             }});
        return 1;
    }

    const std::string& subcommand = args[1];

    if (subcommand == "style_def") {
        return style_def_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "login-startup-arg") {
        return startup_flag_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "completion-case") {
        return completion_case_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "completion-spell") {
        return completion_spell_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "line-numbers") {
        return line_numbers_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "hint-delay") {
        return hint_delay_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "completion-preview") {
        return completion_preview_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "hint") {
        return hint_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "multiline-indent") {
        return multiline_indent_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "multiline") {
        return multiline_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "inline-help") {
        return inline_help_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "auto-tab") {
        return auto_tab_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "keybind") {
        return keybind_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "generate-profile") {
        return generate_profile_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "generate-rc") {
        return generate_rc_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "generate-logout") {
        return generate_logout_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "set-max-bookmarks") {
        return set_max_bookmarks_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "set-history-max") {
        return set_history_max_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "bookmark-blacklist") {
        return bookmark_blacklist_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "cjshopt",
                     "unknown subcommand '" + subcommand + "'",
                     {"Available subcommands: style_def, login-startup-arg, completion-case, "
                      "completion-spell, line-numbers, hint-delay, completion-preview, hint, "
                      "multiline-indent, multiline, inline-help, auto-tab, keybind, generate-profile, "
                      "generate-rc, generate-logout, set-max-bookmarks, set-history-max, bookmark-blacklist"}});
        return 1;
    }
}

int handle_generate_command_common(const std::vector<std::string>& args,
                                   const std::string& command_name,
                                   const cjsh_filesystem::fs::path& target_path,
                                   const std::string& description,
                                   const std::function<bool()>& generator) {
    static const std::vector<std::string> base_usage = {
        "Options:", "  -f, --force   Overwrite the existing file if it exists"};

    bool force = false;

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& option = args[i];
        if (option == "--help" || option == "-h") {
            if (!g_startup_active) {
                std::cout << "Usage: " << command_name << " [--force]\n";
                std::cout << description << "\n";
                for (const auto& line : base_usage) {
                    std::cout << line << '\n';
                }
            }
            return 0;
        }

        if (option == "--force" || option == "-f") {
            force = true;
            continue;
        }

        print_error({ErrorType::INVALID_ARGUMENT,
                     command_name,
                     "Unknown option '" + option + "'",
                     {"Use --help to view available options"}});
        return 1;
    }

    bool file_exists = cjsh_filesystem::fs::exists(target_path);
    if (file_exists && !force) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     command_name,
                     "File already exists at '" + target_path.string() + "'",
                     {"Pass --force to overwrite the existing file"}});
        return 1;
    }

    if (!generator()) {
        return 1;
    }

    if (!g_startup_active) {
        std::cout << (file_exists ? "Updated" : "Created") << " " << target_path << '\n';
    }

    return 0;
}

int generate_profile_command(const std::vector<std::string>& args) {
    return handle_generate_command_common(args, "generate-profile",
                                          cjsh_filesystem::g_cjsh_profile_path,
                                          "Create a default ~/.cjprofile configuration file.",
                                          []() { return cjsh_filesystem::create_profile_file(); });
}

int generate_rc_command(const std::vector<std::string>& args) {
    return handle_generate_command_common(args, "generate-rc", cjsh_filesystem::g_cjsh_source_path,
                                          "Create a default ~/.cjshrc configuration file.",
                                          []() { return cjsh_filesystem::create_source_file(); });
}

int generate_logout_command(const std::vector<std::string>& args) {
    return handle_generate_command_common(args, "generate-logout",
                                          cjsh_filesystem::g_cjsh_logout_path,
                                          "Create a default ~/.cjsh_logout file.",
                                          []() { return cjsh_filesystem::create_logout_file(); });
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
        print_error({ErrorType::INVALID_ARGUMENT, "line-numbers", "Missing option argument",
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

    if (matches_token(normalized,
                      {"off", "disable", "disabled", "false", "0", "--disable"})) {
        ic_enable_line_numbers(false);
        changed = (was_enabled || was_relative);
    } else if (matches_token(normalized, {"relative", "rel", "--relative"})) {
        ic_enable_relative_line_numbers(true);
        changed = (!was_enabled || !was_relative);
    } else if (matches_token(normalized, {"absolute", "abs", "--absolute"}) ||
               matches_token(normalized, {"on", "enable", "enabled", "true", "1",
                                          "--enable"})) {
        ic_enable_line_numbers(true);
        ic_enable_relative_line_numbers(false);
        changed = (!was_enabled || was_relative);
    } else {
        print_error({ErrorType::INVALID_ARGUMENT, "line-numbers",
                     "Unknown option '" + option + "'", usage_lines});
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
        print_error({ErrorType::INVALID_ARGUMENT, "hint-delay", "Missing delay value",
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
            // Note: There's no API to get current hint delay, so we just inform the user
            std::cout << "To check or modify hint delay, use: hint-delay <milliseconds>\n";
        }
        return 0;
    }

    // Try to parse as integer
    try {
        long delay_ms = std::stol(option);
        if (delay_ms < 0) {
            print_error({ErrorType::INVALID_ARGUMENT, "hint-delay",
                         "Delay must be non-negative", usage_lines});
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

int hint_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: hint <on|off|status>",
        "Examples:", "  hint on      Enable inline hints",
        "  hint off     Disable inline hints",
        "  hint status  Show the current setting"};

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

namespace {
struct KeyBindingDefault {
    ic_key_action_t action;
    const char* canonical_name;
    const char* description;
};

const std::vector<KeyBindingDefault> kKeyBindingDefaults = {
    {IC_KEY_ACTION_CURSOR_LEFT, "cursor-left", "go one character to the left"},
    {IC_KEY_ACTION_CURSOR_RIGHT_OR_COMPLETE, "cursor-right", "go one character to the right"},
    {IC_KEY_ACTION_CURSOR_UP, "cursor-up", "go one row up, or back in the history"},
    {IC_KEY_ACTION_CURSOR_DOWN, "cursor-down", "go one row down, or forward in the history"},
    {IC_KEY_ACTION_CURSOR_WORD_PREV, "cursor-word-prev", "go to the start of the previous word"},
    {IC_KEY_ACTION_CURSOR_WORD_NEXT_OR_COMPLETE, "cursor-word-next",
     "go to the end of the current word"},
    {IC_KEY_ACTION_CURSOR_LINE_START, "cursor-line-start", "go to the start of the current line"},
    {IC_KEY_ACTION_CURSOR_LINE_END, "cursor-line-end", "go to the end of the current line"},
    {IC_KEY_ACTION_CURSOR_INPUT_START, "cursor-input-start",
     "go to the start of the current input"},
    {IC_KEY_ACTION_CURSOR_INPUT_END, "cursor-input-end", "go to the end of the current input"},
    {IC_KEY_ACTION_CURSOR_MATCH_BRACE, "cursor-match-brace", "jump to matching brace"},
    {IC_KEY_ACTION_HISTORY_PREV, "history-prev", "go back in the history"},
    {IC_KEY_ACTION_HISTORY_NEXT, "history-next", "go forward in the history"},
    {IC_KEY_ACTION_HISTORY_SEARCH, "history-search",
     "search the history starting with the current word"},
    {IC_KEY_ACTION_DELETE_FORWARD, "delete-forward", "delete the current character"},
    {IC_KEY_ACTION_DELETE_BACKWARD, "delete-backward", "delete the previous character"},
    {IC_KEY_ACTION_DELETE_WORD_START_WS, "delete-word-start-ws", "delete to preceding white space"},
    {IC_KEY_ACTION_DELETE_WORD_START, "delete-word-start",
     "delete to the start of the current word"},
    {IC_KEY_ACTION_DELETE_WORD_END, "delete-word-end", "delete to the end of the current word"},
    {IC_KEY_ACTION_DELETE_LINE_START, "delete-line-start",
     "delete to the start of the current line"},
    {IC_KEY_ACTION_DELETE_LINE_END, "delete-line-end", "delete to the end of the current line"},
    {IC_KEY_ACTION_TRANSPOSE_CHARS, "transpose-chars",
     "swap with previous character (move character backward)"},
    {IC_KEY_ACTION_CLEAR_SCREEN, "clear-screen", "clear screen"},
    {IC_KEY_ACTION_UNDO, "undo", "undo"},
    {IC_KEY_ACTION_REDO, "redo", "redo"},
    {IC_KEY_ACTION_COMPLETE, "complete", "try to complete the current input"},
    {IC_KEY_ACTION_INSERT_NEWLINE, "insert-newline", "create a new line for multi-line input"},
};

const std::vector<std::string> kKeybindUsage = {
    "Usage: keybind <subcommand> [...]",
    "",
    "Note: Key binding modifications can ONLY be made in configuration files (e.g., ~/.cjshrc).",
    "      They cannot be changed at runtime.",
    "",
    "Subcommands:",
    "  list                            Show current default and custom key bindings (works at ",
    "runtime)",
    "  set <action> <keys...>          Replace bindings for an action (config file only)",
    "  add <action> <keys...>          Add key bindings for an action (config file only)",
    "  clear <keys...>                 Remove bindings for the specified key(s) (config file only)",
    "  clear-action <action>           Remove all custom bindings for an action (config file only)",
    "  reset                           Clear all custom key bindings and restore defaults (config ",
    "file only)",
    "  profile list                    List available key binding profiles (runtime)",
    "  profile set <name>              Activate a key binding profile (config file only)",
    "",
    "Use 'keybind --help' for detailed guidance.",
};

std::string trim_copy(const std::string& input) {
    const size_t begin = input.find_first_not_of(" \t");
    if (begin == std::string::npos) {
        return "";
    }
    const size_t end = input.find_last_not_of(" \t");
    return input.substr(begin, end - begin + 1);
}

std::vector<std::string> split_key_spec_string(const std::string& spec) {
    std::vector<std::string> result;
    size_t start = 0;
    while (start <= spec.size()) {
        size_t end = spec.find('|', start);
        const size_t length = (end == std::string::npos ? spec.size() : end) - start;
        std::string token = spec.substr(start, length);
        token = trim_copy(token);
        if (!token.empty()) {
            result.push_back(token);
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return result;
}

std::vector<std::string> parse_key_spec_arguments(const std::vector<std::string>& args,
                                                  size_t start_index) {
    std::vector<std::string> specs;
    for (size_t i = start_index; i < args.size(); ++i) {
        std::vector<std::string> tokens = split_key_spec_string(args[i]);
        specs.insert(specs.end(), tokens.begin(), tokens.end());
    }
    return specs;
}

std::string join_specs(const std::vector<std::string>& specs) {
    if (specs.empty()) {
        return "(none)";
    }
    std::ostringstream oss;
    for (size_t i = 0; i < specs.size(); ++i) {
        if (i != 0) {
            oss << ", ";
        }
        oss << specs[i];
    }
    return oss.str();
}

std::string pipe_join_specs(const std::vector<std::string>& specs) {
    if (specs.empty()) {
        return "";
    }
    std::ostringstream oss;
    for (size_t i = 0; i < specs.size(); ++i) {
        if (i != 0) {
            oss << '|';
        }
        oss << specs[i];
    }
    return oss.str();
}

std::string to_lower_copy(const std::string& input) {
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return result;
}

std::vector<ic_key_binding_entry_t> collect_bindings() {
    size_t count = ic_list_key_bindings(nullptr, 0);
    std::vector<ic_key_binding_entry_t> entries(count);
    if (count > 0) {
        ic_list_key_bindings(entries.data(), count);
    }
    return entries;
}

const KeyBindingDefault* find_default(ic_key_action_t action) {
    for (const auto& entry : kKeyBindingDefaults) {
        if (entry.action == action) {
            return &entry;
        }
    }
    return nullptr;
}

std::unordered_map<ic_key_action_t, std::vector<std::string>> group_bindings_by_action(
    const std::vector<ic_key_binding_entry_t>& entries) {
    std::unordered_map<ic_key_action_t, std::vector<std::string>> grouped;
    for (const auto& entry : entries) {
        char buffer[64];
        if (ic_format_key_spec(entry.key, buffer, sizeof(buffer))) {
            grouped[entry.action].emplace_back(buffer);
        }
    }
    for (auto& pair : grouped) {
        auto& vec = pair.second;
        std::sort(vec.begin(), vec.end());
        vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
    }
    return grouped;
}

std::vector<std::string> available_action_names() {
    std::vector<std::string> names;
    names.reserve(kKeyBindingDefaults.size() + 1);
    for (const auto& entry : kKeyBindingDefaults) {
        names.emplace_back(entry.canonical_name);
    }
    names.emplace_back("none");
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}

void print_keybind_usage() {
    if (g_startup_active) {
        return;
    }
    for (const auto& line : kKeybindUsage) {
        std::cout << line << '\n';
    }
    std::cout << "Available actions: ";
    auto names = available_action_names();
    for (size_t i = 0; i < names.size(); ++i) {
        std::cout << names[i];
        if (i + 1 < names.size()) {
            std::cout << ", ";
        }
    }
    std::cout << '\n';
}

bool parse_key_specs_to_codes(const std::vector<std::string>& specs,
                              std::vector<std::pair<ic_keycode_t, std::string>>* out_codes,
                              std::string* invalid_spec) {
    out_codes->clear();
    std::unordered_set<ic_keycode_t> seen;
    for (const auto& spec : specs) {
        ic_keycode_t key_code = 0;
        if (!ic_parse_key_spec(spec.c_str(), &key_code)) {
            if (invalid_spec != nullptr) {
                *invalid_spec = spec;
            }
            return false;
        }
        if (seen.insert(key_code).second) {
            out_codes->emplace_back(key_code, spec);
        }
    }
    return true;
}

std::string canonical_action_name(ic_key_action_t action) {
    const KeyBindingDefault* info = find_default(action);
    if (info != nullptr) {
        return info->canonical_name;
    }
    const char* resolved = ic_key_action_name(action);
    if (resolved != nullptr) {
        return resolved;
    }
    return "(unknown)";
}

void remove_profile_defaults_from_group(
    std::unordered_map<ic_key_action_t, std::vector<std::string>>* grouped, ic_key_action_t action,
    const char* spec_string) {
    if (grouped == nullptr || spec_string == nullptr || spec_string[0] == '\0')
        return;
    auto it = grouped->find(action);
    if (it == grouped->end())
        return;
    auto tokens = split_key_spec_string(std::string(spec_string));
    if (tokens.empty())
        return;
    auto& specs = it->second;
    if (specs.empty())
        return;
    for (const auto& token : tokens) {
        ic_keycode_t key = 0;
        if (!ic_parse_key_spec(token.c_str(), &key))
            continue;
        char formatted[64];
        if (!ic_format_key_spec(key, formatted, sizeof(formatted)))
            continue;
        const std::string canonical = to_lower_copy(formatted);
        for (size_t idx = 0; idx < specs.size();) {
            if (to_lower_copy(specs[idx]) == canonical) {
                specs.erase(specs.begin() + static_cast<std::ptrdiff_t>(idx));
            } else {
                ++idx;
            }
        }
    }
    if (specs.empty()) {
        grouped->erase(it);
    }
}

int keybind_list_command() {
    auto entries = collect_bindings();
    auto grouped = group_bindings_by_action(entries);

    if (g_startup_active) {
        return 0;
    }

    const char* active_profile = ic_get_key_binding_profile();
    std::cout << "Active key binding profile: "
              << (active_profile != nullptr ? active_profile : "emacs") << "\n\n";

    size_t name_width = std::strlen("Action");
    for (const auto& entry : kKeyBindingDefaults) {
        name_width = std::max(name_width, std::strlen(entry.canonical_name));
    }
    for (const auto& pair : grouped) {
        if (find_default(pair.first) != nullptr) {
            continue;
        }
        const char* resolved = ic_key_action_name(pair.first);
        if (resolved != nullptr) {
            name_width = std::max(name_width, std::strlen(resolved));
        }
    }

    constexpr size_t default_column_width = 28;
    std::cout << std::left << std::setw(static_cast<int>(name_width) + 2) << "Action"
              << std::setw(static_cast<int>(default_column_width)) << "Default"
              << "Custom" << '\n';
    std::cout << std::string(name_width + 2 + default_column_width + 6, '-') << '\n';

    std::unordered_set<ic_key_action_t> printed;

    for (const auto& entry : kKeyBindingDefaults) {
        const char* spec_cstr = ic_key_binding_profile_default_specs(entry.action);
        std::string spec_string = (spec_cstr != nullptr ? spec_cstr : "");
        std::vector<std::string> default_specs = split_key_spec_string(spec_string);
        std::string default_display = join_specs(default_specs);

        remove_profile_defaults_from_group(&grouped, entry.action, spec_cstr);

        std::string custom_display = "(none)";
        auto it = grouped.find(entry.action);
        if (it != grouped.end()) {
            custom_display = join_specs(it->second);
            printed.insert(entry.action);
        }

        std::cout << std::left << std::setw(static_cast<int>(name_width) + 2)
                  << entry.canonical_name << std::setw(static_cast<int>(default_column_width))
                  << default_display << custom_display << '\n';
    }

    for (const auto& pair : grouped) {
        if (printed.find(pair.first) != printed.end()) {
            continue;
        }
        std::string name = canonical_action_name(pair.first);
        std::cout << std::left << std::setw(static_cast<int>(name_width) + 2) << name
                  << std::setw(static_cast<int>(default_column_width)) << "(none)"
                  << join_specs(pair.second) << '\n';
    }

    if (entries.empty()) {
        std::cout << "\nNo custom key bindings are currently defined.\n";
        std::cout << "To customize key bindings, add 'cjshopt keybind ...' commands to your "
                     "~/.cjshrc file.\n";
    } else {
        std::cout << "\nCustom key bindings are defined in your configuration files.\n";
        std::cout << "To modify them, edit your ~/.cjshrc file.\n";
    }

    return 0;
}

int keybind_profile_list_command() {
    if (g_startup_active) {
        return 0;
    }

    size_t count = ic_list_key_binding_profiles(nullptr, 0);
    std::vector<ic_key_binding_profile_info_t> profiles(count);
    if (count > 0) {
        ic_list_key_binding_profiles(profiles.data(), count);
    }

    const char* active = ic_get_key_binding_profile();
    std::string active_lower = (active != nullptr ? to_lower_copy(active) : "");
    std::cout << "Available key binding profiles:\n";
    for (const auto& profile : profiles) {
        bool is_active = (profile.name != nullptr && to_lower_copy(profile.name) == active_lower);
        std::cout << "  " << (is_active ? "* " : "  ")
                  << (profile.name != nullptr ? profile.name : "(unknown)");
        if (profile.description != nullptr && profile.description[0] != '\0') {
            std::cout << " - " << profile.description;
        }
        std::cout << '\n';
    }
    if (profiles.empty()) {
        std::cout << "  (no profiles available)\n";
    }
    return 0;
}

int keybind_profile_set_command(const std::vector<std::string>& args) {
    if (args.size() != 4) {
        print_error({ErrorType::INVALID_ARGUMENT, "keybind", "profile set requires a profile name",
                     kKeybindUsage});
        return 1;
    }

    const std::string& profile_name = args[3];
    if (!ic_set_key_binding_profile(profile_name.c_str())) {
        size_t count = ic_list_key_binding_profiles(nullptr, 0);
        std::vector<ic_key_binding_profile_info_t> profiles(count);
        if (count > 0) {
            ic_list_key_binding_profiles(profiles.data(), count);
        }
        std::vector<std::string> names;
        names.reserve(profiles.size());
        for (const auto& profile : profiles) {
            if (profile.name != nullptr) {
                names.emplace_back(profile.name);
            }
        }
        std::ostringstream oss;
        for (size_t i = 0; i < names.size(); ++i) {
            if (i != 0) {
                oss << ", ";
            }
            oss << names[i];
        }
        print_error(
            {ErrorType::INVALID_ARGUMENT,
             "keybind",
             "Unknown key binding profile '" + profile_name + "'",
             {"Available profiles: " + (names.empty() ? std::string("(none)") : oss.str())}});
        return 1;
    }

    if (!g_startup_active) {
        std::cout << "Key binding profile set to '" << profile_name << "'.\n";
        std::cout << "Add `cjshopt keybind profile set " << profile_name
                  << "` to your ~/.cjshrc to persist this change.\n";
    }
    return 0;
}

int keybind_set_or_add_command(const std::vector<std::string>& args, bool replace_existing) {
    if (args.size() < 4) {
        print_error({ErrorType::INVALID_ARGUMENT, "keybind",
                     std::string(replace_existing ? "set" : "add") +
                         " requires an action and at least one key specification",
                     kKeybindUsage});
        return 1;
    }

    const std::string& action_arg = args[2];
    ic_key_action_t action = ic_key_action_from_name(action_arg.c_str());
    if (action == IC_KEY_ACTION__MAX) {
        print_error({ErrorType::INVALID_ARGUMENT, "keybind", "Unknown action '" + action_arg + "'",
                     kKeybindUsage});
        return 1;
    }

    auto spec_args = parse_key_spec_arguments(args, 3);
    if (spec_args.empty()) {
        print_error({ErrorType::INVALID_ARGUMENT, "keybind",
                     "Provide at least one key specification", kKeybindUsage});
        return 1;
    }

    std::vector<std::pair<ic_keycode_t, std::string>> parsed;
    std::string invalid_spec;
    if (!parse_key_specs_to_codes(spec_args, &parsed, &invalid_spec)) {
        print_error({ErrorType::INVALID_ARGUMENT, "keybind",
                     "Invalid key specification '" + invalid_spec + "'", kKeybindUsage});
        return 1;
    }

    std::vector<ic_key_binding_entry_t> previous;
    if (replace_existing && action != IC_KEY_ACTION_NONE) {
        auto entries = collect_bindings();
        for (const auto& entry : entries) {
            if (entry.action == action) {
                previous.push_back(entry);
                ic_clear_key_binding(entry.key);
            }
        }
    }

    for (const auto& entry : parsed) {
        ic_clear_key_binding(entry.first);
    }

    std::vector<ic_keycode_t> bound;
    bound.reserve(parsed.size());
    for (const auto& entry : parsed) {
        if (!ic_bind_key(entry.first, action)) {
            for (const auto& key : bound) {
                ic_clear_key_binding(key);
            }
            for (const auto& prev : previous) {
                ic_bind_key(prev.key, prev.action);
            }
            print_error({ErrorType::INVALID_ARGUMENT, "keybind",
                         "Failed to bind key specification '" + entry.second + "'", kKeybindUsage});
            return 1;
        }
        bound.push_back(entry.first);
    }

    if (!g_startup_active) {
        std::vector<std::string> spec_strings;
        spec_strings.reserve(parsed.size());
        for (const auto& entry : parsed) {
            spec_strings.push_back(entry.second);
        }
        std::string action_display = canonical_action_name(action);
        std::cout << (replace_existing ? "Set " : "Added ") << action_display << " -> "
                  << join_specs(spec_strings) << '\n';
        std::cout << "Add `cjshopt keybind " << (replace_existing ? "set " : "add ")
                  << action_display << " '" << pipe_join_specs(spec_strings)
                  << "'` to your ~/.cjshrc to persist this change.\n";
    }

    return 0;
}

int keybind_clear_keys_command(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        print_error({ErrorType::INVALID_ARGUMENT, "keybind",
                     "clear requires at least one key specification", kKeybindUsage});
        return 1;
    }

    auto spec_args = parse_key_spec_arguments(args, 2);
    if (spec_args.empty()) {
        print_error({ErrorType::INVALID_ARGUMENT, "keybind",
                     "Provide at least one key specification", kKeybindUsage});
        return 1;
    }

    std::vector<std::pair<ic_keycode_t, std::string>> parsed;
    std::string invalid_spec;
    if (!parse_key_specs_to_codes(spec_args, &parsed, &invalid_spec)) {
        print_error({ErrorType::INVALID_ARGUMENT, "keybind",
                     "Invalid key specification '" + invalid_spec + "'", kKeybindUsage});
        return 1;
    }

    std::vector<std::string> removed;
    std::vector<std::string> missing;
    for (const auto& entry : parsed) {
        if (ic_clear_key_binding(entry.first)) {
            removed.push_back(entry.second);
        } else {
            missing.push_back(entry.second);
        }
    }

    if (!g_startup_active) {
        if (!removed.empty()) {
            std::cout << "Cleared key binding(s) for: " << join_specs(removed) << '\n';
        }
        if (!missing.empty()) {
            std::cout << "No custom binding found for: " << join_specs(missing) << '\n';
        }
        if (removed.empty() && missing.empty()) {
            std::cout << "Nothing to clear.\n";
        }
    }

    return 0;
}

int keybind_clear_action_command(const std::vector<std::string>& args) {
    if (args.size() != 3) {
        print_error({ErrorType::INVALID_ARGUMENT, "keybind", "clear-action requires an action name",
                     kKeybindUsage});
        return 1;
    }

    const std::string& action_arg = args[2];
    ic_key_action_t action = ic_key_action_from_name(action_arg.c_str());
    if (action == IC_KEY_ACTION__MAX) {
        print_error({ErrorType::INVALID_ARGUMENT, "keybind", "Unknown action '" + action_arg + "'",
                     kKeybindUsage});
        return 1;
    }

    auto entries = collect_bindings();
    std::vector<std::string> removed;
    for (const auto& entry : entries) {
        if (entry.action == action) {
            char buffer[64];
            if (ic_format_key_spec(entry.key, buffer, sizeof(buffer))) {
                removed.emplace_back(buffer);
            }
            ic_clear_key_binding(entry.key);
        }
    }

    if (!g_startup_active) {
        if (!removed.empty()) {
            std::cout << "Cleared custom bindings for " << canonical_action_name(action) << ": "
                      << join_specs(removed) << '\n';
        } else {
            std::cout << "No custom bindings were set for " << canonical_action_name(action)
                      << ".\n";
        }
    }

    return 0;
}

int keybind_reset_command() {
    ic_reset_key_bindings();
    if (!g_startup_active) {
        std::cout << "All custom key bindings cleared.\n";
    }
    return 0;
}
}  // namespace

int keybind_command(const std::vector<std::string>& args) {
    if (args.size() == 1) {
        print_error(
            {ErrorType::INVALID_ARGUMENT, "keybind", "Missing subcommand argument", kKeybindUsage});
        return 1;
    }

    const std::string& subcommand = args[1];
    if (subcommand == "--help" || subcommand == "-h") {
        print_keybind_usage();
        return 0;
    }

    if (subcommand == "list") {
        if (args.size() != 2) {
            print_error({ErrorType::INVALID_ARGUMENT, "keybind",
                         "list does not accept additional arguments", kKeybindUsage});
            return 1;
        }
        return keybind_list_command();
    }

    if (subcommand == "profile") {
        if (args.size() < 3) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "keybind",
                         "profile requires a subcommand",
                         {"Usage:", "  keybind profile list", "  keybind profile set <name>"}});
            return 1;
        }
        const std::string& profile_sub = args[2];
        if (profile_sub == "list") {
            if (args.size() != 3) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             "keybind",
                             "profile list does not accept additional arguments",
                             {}});
                return 1;
            }
            return keybind_profile_list_command();
        }
        if (profile_sub == "set") {
            return keybind_profile_set_command(args);
        }
        print_error({ErrorType::INVALID_ARGUMENT,
                     "keybind",
                     "Unknown profile subcommand '" + profile_sub + "'",
                     {"Valid profile subcommands are: list, set"}});
        return 1;
    }

    if (subcommand == "set") {
        return keybind_set_or_add_command(args, true);
    }

    if (subcommand == "add") {
        return keybind_set_or_add_command(args, false);
    }

    if (subcommand == "clear") {
        return keybind_clear_keys_command(args);
    }

    if (subcommand == "clear-action") {
        return keybind_clear_action_command(args);
    }

    if (subcommand == "reset") {
        if (args.size() != 2) {
            print_error({ErrorType::INVALID_ARGUMENT, "keybind",
                         "reset does not accept additional arguments", kKeybindUsage});
            return 1;
        }
        return keybind_reset_command();
    }

    print_error({ErrorType::INVALID_ARGUMENT, "keybind", "Unknown subcommand '" + subcommand + "'",
                 kKeybindUsage});
    return 1;
}

int startup_flag_command(const std::vector<std::string>& args) {
    // Only allow setting startup flags during startup (in config files)
    if (!g_startup_active) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "login-startup-arg",
                     "Startup flags can only be set in configuration files (e.g., ~/.cjprofile)",
                     {"To set startup flags, add 'cjshopt login-startup-arg ...' commands to your "
                      "~/.cjprofile file."}});
        return 1;
    }

    if (args.size() < 2) {
        print_error(
            {ErrorType::INVALID_ARGUMENT,
             "login-startup-arg",
             "Missing flag argument",
             {"Usage: login-startup-arg [--flag-name]",
              "Available flags:", "  --login              Set login mode",
              "  --interactive        Force interactive mode",
              "  --debug              Enable debug mode", "  --no-themes          Disable themes",
              "  --no-colors          Disable colors", "  --no-titleline       Disable title line",
              "  --show-startup-time  Display shell startup time",
              "  --no-source          Don't source the .cjshrc file",
              "  --no-completions     Disable tab completions",
              "  --no-syntax-highlighting Disable syntax highlighting",
              "  --no-smart-cd        Disable smart cd functionality",
              "  --no-prompt          Use simple '#' prompt instead of themed prompt",
              R"(  --minimal            Disable all unique cjsh features (themes, colors, completions, syntax highlighting, smart cd, sourcing, custom ls, startup time display))",
              R"(  --disable-custom-ls  Use system ls command instead of builtin ls)",
              "  --startup-test       Enable startup test mode"}});
        return 1;
    }

    const std::string& flag = args[1];

    if (flag == "--login" || flag == "--interactive" || flag == "--debug" ||
        flag == "--no-prompt" || flag == "--no-themes" || flag == "--no-colors" ||
        flag == "--no-titleline" || flag == "--show-startup-time" || flag == "--no-source" ||
        flag == "--no-completions" || flag == "--no-syntax-highlighting" ||
        flag == "--no-smart-cd" || flag == "--minimal" || flag == "--startup-test" ||
        flag == "--disable-custom-ls") {
        bool flag_exists = false;
        for (const auto& existing_flag : g_profile_startup_args) {
            if (existing_flag == flag) {
                flag_exists = true;
                break;
            }
        }

        if (!flag_exists) {
            g_profile_startup_args.push_back(flag);
        }
    } else {
        print_error(
            {ErrorType::INVALID_ARGUMENT, "login-startup-arg", "unknown flag '" + flag + "'", {}});
        return 1;
    }

    return 0;
}

static std::unordered_map<std::string, std::string> g_custom_styles;  // NOLINT
static const std::unordered_map<std::string, std::string> default_styles = {
    {"unknown-command", "bold color=#FF5555"},
    {"colon", "bold color=#8BE9FD"},
    {"path-exists", "color=#50FA7B"},
    {"path-not-exists", "color=#FF5555"},
    {"glob-pattern", "color=#F1FA8C"},
    {"operator", "bold color=#FF79C6"},
    {"keyword", "bold color=#BD93F9"},
    {"builtin", "color=#FFB86C"},
    {"system", "color=#50FA7B"},
    {"installed", "color=#8BE9FD"},
    {"variable", "color=#8BE9FD"},
    {"string", "color=#F1FA8C"},
    {"comment", "color=#6272A4"},
    {"function-definition", "bold color=#F1FA8C"},
    {"ic-linenumbers", "ansi-lightgray"}};

static std::string resolve_style_registry_name(const std::string& token_type) {
    if (token_type.rfind("ic-", 0) == 0) {
        return token_type;
    }
    return "cjsh-" + token_type;
}

int style_def_command(const std::vector<std::string>& args) {
    if (args.size() == 1) {
        if (!g_startup_active) {
            std::cout << "Usage: style_def <token_type> <style>\n\n";
            std::cout << "Define or redefine a syntax highlighting style.\n\n";
            std::cout << "Token types:\n";
            for (const auto& pair : default_styles) {
                std::cout << "  " << pair.first << " (default: " << pair.second << ")\n";
            }
            std::cout << "\nStyle format: [bold] [italic] [underline] "
                         "color=#RRGGBB|color=name\n";
            std::cout << "Color names: red, green, blue, yellow, magenta, cyan, "
                         "white, black\n";
            std::cout << "ANSI colors: ansi-black, ansi-red, ansi-green, "
                         "ansi-yellow, etc.\n\n";
            std::cout << "Examples:\n";
            std::cout << "  style_def builtin \"bold color=#FFB86C\"\n";
            std::cout << "  style_def system \"color=#50FA7B\"\n";
            std::cout << "  style_def installed \"color=#8BE9FD\"\n";
            std::cout << "  style_def comment \"italic color=green\"\n";
            std::cout << "  style_def string \"color=#F1FA8C\"\n\n";
            std::cout << "To reset all styles to defaults, use: style_def --reset\n";
        }
        return 0;
    }

    if (args.size() == 2 && args[1] == "--reset") {
        reset_to_default_styles();
        if (!g_startup_active) {
            std::cout << "All syntax highlighting styles reset to defaults.\n";
        }
        return 0;
    }

    if (args.size() != 3) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "style_def",
                     "expected 2 arguments: <token_type> <style>",
                     {"Use 'style_def' to see available token types"}});
        return 1;
    }

    const std::string& token_type = args[1];
    const std::string& style = args[2];

    if (default_styles.find(token_type) == default_styles.end()) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "style_def",
                     "unknown token type: " + token_type,
                     {"Use 'style_def' to see available token types"}});
        return 1;
    }

    apply_custom_style(token_type, style);

    return 0;
}

void apply_custom_style(const std::string& token_type, const std::string& style) {
    std::string full_style_name = resolve_style_registry_name(token_type);

    auto default_it = default_styles.find(token_type);
    if (default_it != default_styles.end() && default_it->second == style) {
        bool had_custom_override = g_custom_styles.erase(token_type) > 0;

        if (had_custom_override) {
            ic_style_def(full_style_name.c_str(), style.c_str());
        }
        return;
    }

    auto existing_style = g_custom_styles.find(token_type);
    if (existing_style != g_custom_styles.end() && existing_style->second == style) {
        return;
    }

    g_custom_styles[token_type] = style;

    ic_style_def(full_style_name.c_str(), style.c_str());
}

void reset_to_default_styles() {
    g_custom_styles.clear();

    for (const auto& pair : default_styles) {
        std::string full_style_name = resolve_style_registry_name(pair.first);
        ic_style_def(full_style_name.c_str(), pair.second.c_str());
    }
}

const std::unordered_map<std::string, std::string>& get_custom_styles() {
    return g_custom_styles;
}

void load_custom_styles_from_config() {
    std::ifstream config_file(cjsh_filesystem::g_cjsh_source_path);
    if (!config_file.is_open()) {
        return;
    }

    std::string line;

    while (std::getline(config_file, line)) {
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        std::string command_line = trimmed;

        if (command_line.rfind("cjshopt", 0) == 0 &&
            (command_line.size() == 7 || command_line[7] == ' ' || command_line[7] == '\t')) {
            command_line.erase(0, 7);
            auto after_prefix = command_line.find_first_not_of(" \t");
            if (after_prefix == std::string::npos) {
                continue;
            }
            command_line.erase(0, after_prefix);
        }

        auto first_non_space = command_line.find_first_not_of(" \t");
        if (first_non_space == std::string::npos) {
            continue;
        }
        command_line.erase(0, first_non_space);

        if (command_line.rfind("style_def ", 0) == 0) {
            std::istringstream iss(command_line);
            std::string command;
            std::string token_type;

            iss >> command;

            if (!(iss >> token_type)) {
                continue;
            }

            std::string remaining;
            std::getline(iss, remaining);

            remaining.erase(0, remaining.find_first_not_of(" \t"));

            if (remaining.empty()) {
                continue;
            }

            if ((remaining.front() == '"' && remaining.back() == '"') ||
                (remaining.front() == '\'' && remaining.back() == '\'')) {
                remaining = remaining.substr(1, remaining.length() - 2);
            }

            if (default_styles.find(token_type) != default_styles.end()) {
                apply_custom_style(token_type, remaining);
            }
        }
    }

    config_file.close();
}

int set_max_bookmarks_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: set-max-bookmarks <number>",
        "",
        "Set the maximum number of directory bookmarks to store.",
        "Default is 100. Minimum is 10. Maximum is 1000.",
        "",
        "Example:",
        "  set-max-bookmarks 200"};

    if (args.size() == 1) {
        if (!g_startup_active) {
            for (const auto& line : usage_lines) {
                std::cout << line << '\n';
            }
        }
        print_error({ErrorType::INVALID_ARGUMENT, "set-max-bookmarks",
                     "expected 1 argument: <number>", usage_lines});
        return 1;
    }

    if (args.size() > 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "set-max-bookmarks",
                     "too many arguments provided", usage_lines});
        return 1;
    }

    const std::string& option = args[1];
    if (option == "--help" || option == "-h") {
        if (!g_startup_active) {
            for (const auto& line : usage_lines) {
                std::cout << line << '\n';
            }
        }
        return 0;
    }

    int number = 0;
    try {
        number = std::stoi(option);
    } catch (const std::invalid_argument&) {
        print_error({ErrorType::INVALID_ARGUMENT, "set-max-bookmarks", "invalid number: " + option,
                     usage_lines});
        return 1;
    } catch (const std::out_of_range&) {
        print_error({ErrorType::INVALID_ARGUMENT, "set-max-bookmarks",
                     "number out of range: " + option, usage_lines});
        return 1;
    }

    if (number < 10 || number > 1000) {
        print_error({ErrorType::INVALID_ARGUMENT, "set-max-bookmarks",
                     "number must be between 10 and 1000", usage_lines});
        return 1;
    }

    bookmark_database::g_bookmark_db.set_max_bookmarks(number);
    if (!g_startup_active) {
        std::cout << "Maximum bookmarks set to "
                  << bookmark_database::g_bookmark_db.get_max_bookmarks() << ".\n";
    }

    return 0;
}

int set_history_max_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: set-history-max <number|default|status>",
        "",
        "Configure the maximum number of entries written to the history file.",
        "Use 0 to disable history persistence entirely.",
        "Use 'default' to restore the built-in limit (" +
            std::to_string(get_history_default_history_limit()) + " entries).",
        "Use 'status' to view the current setting.",
        "Valid range: " + std::to_string(get_history_min_history_limit()) + " - " +
            std::to_string(get_history_max_history_limit()) + "."};

    if (args.size() == 1) {
        if (!g_startup_active) {
            for (const auto& line : usage_lines) {
                std::cout << line << '\n';
            }
        }
        print_error(
            {ErrorType::INVALID_ARGUMENT, "set-history-max", "expected 1 argument", usage_lines});
        return 1;
    }

    if (args.size() > 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "set-history-max", "too many arguments provided",
                     usage_lines});
        return 1;
    }

    const std::string& option = args[1];
    std::string normalized = option;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (normalized == "--help" || normalized == "-h") {
        if (!g_startup_active) {
            for (const auto& line : usage_lines) {
                std::cout << line << '\n';
            }
        }
        return 0;
    }

    if (normalized == "status" || normalized == "--status") {
        if (!g_startup_active) {
            long current_limit = get_history_max_entries();
            if (current_limit <= 0) {
                std::cout << "History persistence is currently disabled.\n";
            } else {
                std::cout << "History file retains up to " << current_limit << " entries." << '\n';
            }
        }
        return 0;
    }

    long requested_limit = 0;
    if (normalized == "default" || normalized == "--default") {
        requested_limit = get_history_default_history_limit();
    } else {
        try {
            requested_limit = std::stol(option);
        } catch (const std::invalid_argument&) {
            print_error({ErrorType::INVALID_ARGUMENT, "set-history-max",
                         "invalid number: " + option, usage_lines});
            return 1;
        } catch (const std::out_of_range&) {
            print_error({ErrorType::INVALID_ARGUMENT, "set-history-max",
                         "number out of range: " + option, usage_lines});
            return 1;
        }
    }

    if (requested_limit < get_history_min_history_limit()) {
        print_error({ErrorType::INVALID_ARGUMENT, "set-history-max",
                     "value must be greater than or equal to " +
                         std::to_string(get_history_min_history_limit()),
                     usage_lines});
        return 1;
    }

    if (requested_limit > get_history_max_history_limit()) {
        print_error({ErrorType::INVALID_ARGUMENT, "set-history-max",
                     "value exceeds the maximum allowed: " +
                         std::to_string(get_history_max_history_limit()),
                     usage_lines});
        return 1;
    }

    std::string error_message;
    if (!set_history_max_entries(requested_limit, &error_message)) {
        if (error_message.empty()) {
            error_message = "Failed to update history limit.";
        }
        print_error({ErrorType::RUNTIME_ERROR, "set-history-max", error_message, {}});
        return 1;
    }

    if (!g_startup_active) {
        long applied_limit = get_history_max_entries();
        if (applied_limit <= 0) {
            std::cout << "History persistence disabled.\n";
        } else {
            std::cout << "History file will retain up to " << applied_limit << " entries." << '\n';
        }
    }

    return 0;
}

int bookmark_blacklist_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: bookmark-blacklist <subcommand> [path]",
        "",
        "Manage directories that cannot be bookmarked.",
        "",
        "Subcommands:",
        "  add <path>      Add a directory to the blacklist",
        "  remove <path>   Remove a directory from the blacklist",
        "  list            List all blacklisted directories",
        "  clear           Clear the entire blacklist",
        "",
        "Examples:",
        "  cjshopt bookmark-blacklist add /tmp",
        "  cjshopt bookmark-blacklist remove /tmp",
        "  cjshopt bookmark-blacklist list",
        "  cjshopt bookmark-blacklist clear"};

    if (args.size() < 2) {
        if (!g_startup_active) {
            for (const auto& line : usage_lines) {
                std::cout << line << '\n';
            }
        }
        print_error({ErrorType::INVALID_ARGUMENT, "bookmark-blacklist", "expected a subcommand",
                     usage_lines});
        return 1;
    }

    const std::string& subcommand = args[1];

    if (subcommand == "--help" || subcommand == "-h") {
        if (!g_startup_active) {
            for (const auto& line : usage_lines) {
                std::cout << line << '\n';
            }
        }
        return 0;
    }

    if (subcommand == "add") {
        if (args.size() < 3) {
            print_error({ErrorType::INVALID_ARGUMENT, "bookmark-blacklist add",
                         "expected a path argument", usage_lines});
            return 1;
        }

        const std::string& path = args[2];

        // First, check if any bookmarks will be removed
        std::vector<std::string> affected_bookmarks;
        if (!g_startup_active) {
            try {
                std::filesystem::path fs_path(path);
                std::string canonical_path;

                if (std::filesystem::exists(fs_path)) {
                    canonical_path = std::filesystem::canonical(fs_path).string();
                } else {
                    canonical_path = std::filesystem::absolute(fs_path).string();
                }

                auto all_bookmarks = bookmark_database::get_directory_bookmarks();
                for (const auto& [name, bookmark_path] : all_bookmarks) {
                    if (bookmark_path == canonical_path) {
                        affected_bookmarks.push_back(name);
                    }
                }
            } catch (...) {
                // Ignore errors during check
            }
        }

        auto result = bookmark_database::add_path_to_blacklist(path);

        if (result.is_error()) {
            print_error({ErrorType::RUNTIME_ERROR, "bookmark-blacklist add", result.error(), {}});
            return 1;
        }

        if (!g_startup_active) {
            std::cout << "Added to blacklist: " << path << '\n';
            if (!affected_bookmarks.empty()) {
                std::cout << "Removed " << affected_bookmarks.size() << " existing bookmark(s): ";
                for (size_t i = 0; i < affected_bookmarks.size(); ++i) {
                    if (i > 0)
                        std::cout << ", ";
                    std::cout << affected_bookmarks[i];
                }
                std::cout << '\n';
            }
        }
        return 0;

    } else if (subcommand == "remove") {
        if (args.size() < 3) {
            print_error({ErrorType::INVALID_ARGUMENT, "bookmark-blacklist remove",
                         "expected a path argument", usage_lines});
            return 1;
        }

        const std::string& path = args[2];
        auto result = bookmark_database::remove_path_from_blacklist(path);

        if (result.is_error()) {
            print_error(
                {ErrorType::RUNTIME_ERROR, "bookmark-blacklist remove", result.error(), {}});
            return 1;
        }

        if (!g_startup_active) {
            std::cout << "Removed from blacklist: " << path << '\n';
        }
        return 0;

    } else if (subcommand == "list") {
        auto blacklist = bookmark_database::get_bookmark_blacklist();

        if (blacklist.empty()) {
            if (!g_startup_active) {
                std::cout << "No directories are currently blacklisted.\n";
            }
        } else {
            if (!g_startup_active) {
                std::cout << "Blacklisted directories:\n";
                for (const auto& path : blacklist) {
                    std::cout << "  " << path << '\n';
                }
            }
        }
        return 0;

    } else if (subcommand == "clear") {
        auto result = bookmark_database::clear_bookmark_blacklist();

        if (result.is_error()) {
            print_error({ErrorType::RUNTIME_ERROR, "bookmark-blacklist clear", result.error(), {}});
            return 1;
        }

        if (!g_startup_active) {
            std::cout << "Blacklist cleared.\n";
        }
        return 0;

    } else {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "bookmark-blacklist",
                     "unknown subcommand '" + subcommand + "'",
                     {"Available subcommands: add, remove, list, clear"}});
        return 1;
    }

    return 0;
}

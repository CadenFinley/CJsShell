/*
  cjshopt_misc_commands.cpp

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
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "cjsh.h"
#include "cjsh_completions.h"
#include "error_out.h"
#include "flags.h"
#include "isocline.h"
#include "token_constants.h"

namespace {

struct StartupFlagInfo {
    const char* name;
    const char* description;
};

constexpr StartupFlagInfo kStartupFlags[] = {
    {"--login", "Set login mode"},
    {"--interactive", "Force interactive mode"},
    {"--posix", "Enable POSIX mode"},
    {"--no-exec", "Read commands without executing"},
    {"--no-colors", "Disable colors"},
    {"--no-titleline", "Disable title line"},
    {"--show-startup-time", "Display shell startup time"},
    {"--no-source", "Skip sourcing configuration files"},
    {"--no-completions", "Disable tab completions"},
    {"--no-completion-learning", "Skip on-demand completion scraping"},
    {"--no-smart-cd", "Disable smart cd auto-jumps"},
    {"--no-script-extension-interpreter", "Disable extension-based script runners"},
    {"--no-syntax-highlighting", "Disable syntax highlighting"},
    {"--no-error-suggestions", "Disable error suggestions"},
    {"--no-prompt-vars", "Ignore PS1/PS2 prompt variables"},
    {"--no-history", "Disable history recording"},
    {"--no-history-expansion", "Disable history expansion"},
    {"--no-sh-warning", "Suppress the sh invocation warning"},
    {"--minimal", "Disable cjsh extras"},
    {"--secure", "Enable secure mode"},
    {"--startup-test", "Enable startup test mode"},
};

const std::vector<std::string>& startup_flag_help_lines() {
    static const std::vector<std::string> lines = [] {
        std::vector<std::string> help = {"Usage: login-startup-arg [--flag-name]",
                                         "Available flags:"};
        for (const auto& entry : kStartupFlags) {
            help.emplace_back("  " + std::string(entry.name) + "  " + entry.description);
        }
        return help;
    }();
    return lines;
}

bool is_supported_startup_flag(const std::string& flag) {
    return std::any_of(std::begin(kStartupFlags), std::end(kStartupFlags),
                       [&](const auto& entry) { return flag == entry.name; });
}

std::string resolve_style_registry_name(const std::string& token_type) {
    if (token_type.rfind("ic-", 0) == 0) {
        return token_type;
    }
    return "cjsh-" + token_type;
}

std::string style_preview_sample(const std::string& token_type) {
    if (token_type == "unknown-command") {
        return "notarealcmd";
    }
    if (token_type == "colon") {
        return ":";
    }
    if (token_type == "path-exists") {
        return "/usr";
    }
    if (token_type == "path-not-exists") {
        return "/nope";
    }
    if (token_type == "glob-pattern") {
        return "*.cpp";
    }
    if (token_type == "operator") {
        return "&& || | >";
    }
    if (token_type == "keyword") {
        return "if then fi";
    }
    if (token_type == "builtin") {
        return "cd";
    }
    if (token_type == "system") {
        return "ls";
    }
    if (token_type == "variable") {
        return "$HOME";
    }
    if (token_type == "assignment-value") {
        return "FOO=bar";
    }
    if (token_type == "string") {
        return "\"hello\"";
    }
    if (token_type == "comment") {
        return "# comment";
    }
    if (token_type == "command-substitution") {
        return "$(date)";
    }
    if (token_type == "arithmetic") {
        return "$((1+2))";
    }
    if (token_type == "option") {
        return "--help";
    }
    if (token_type == "number") {
        return "42";
    }
    if (token_type == "function-definition") {
        return "myfunc()";
    }
    if (token_type == "history-expansion") {
        return "!!";
    }
    if (token_type == "ic-prompt") {
        return "prompt";
    }
    if (token_type == "ic-linenumbers") {
        return "1";
    }
    if (token_type == "ic-linenumber-current") {
        return "2";
    }
    if (token_type == "ic-info") {
        return "info";
    }
    if (token_type == "ic-source") {
        return "source";
    }
    if (token_type == "ic-diminish") {
        return "dim";
    }
    if (token_type == "ic-emphasis") {
        return "emphasis";
    }
    if (token_type == "ic-hint") {
        return "hint";
    }
    if (token_type == "ic-error") {
        return "error";
    }
    if (token_type == "ic-bracematch") {
        return "{}";
    }
    if (token_type == "ic-whitespace-char") {
        return "space";
    }
    return token_type;
}

void print_style_preview() {
    std::vector<std::string> token_types;
    token_types.reserve(token_constants::default_styles().size());
    for (const auto& pair : token_constants::default_styles()) {
        token_types.push_back(pair.first);
    }
    std::sort(token_types.begin(), token_types.end());

    ic_println("Syntax style preview:");
    for (const auto& token_type : token_types) {
        std::string style_name = resolve_style_registry_name(token_type);
        std::string sample = style_preview_sample(token_type);
        std::string line = token_type + ": [" + style_name + "]" + sample + "[/]";
        ic_println(line.c_str());
    }
    ic_println("Use: cjshopt style_def <token_type> \"<style>\"");
}
}  // namespace

int startup_flag_command(const std::vector<std::string>& args) {
    if (!g_startup_active) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "login-startup-arg",
                     "Startup flags can only be set in configuration files (e.g., ~/.cjprofile)",
                     {"To set startup flags, add 'cjshopt login-startup-arg ...' commands to your "
                      "~/.cjprofile file."}});
        return 1;
    }

    const auto& help_lines = startup_flag_help_lines();

    if (args.size() < 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "login-startup-arg", "Missing flag argument",
                     help_lines});
        return 1;
    }

    const std::string& flag = args[1];

    if (!is_supported_startup_flag(flag)) {
        print_error({ErrorType::INVALID_ARGUMENT, "login-startup-arg",
                     "unknown flag '" + flag + "'", help_lines});
        return 1;
    }

    auto& stored_flags = flags::profile_startup_args();
    if (std::find(stored_flags.begin(), stored_flags.end(), flag) == stored_flags.end()) {
        stored_flags.push_back(flag);
    }

    return 0;
}

int style_def_command(const std::vector<std::string>& args) {
    if (args.size() == 1) {
        if (!g_startup_active) {
            std::cout << "Usage: style_def <token_type> <style>\n\n";
            std::cout << "Define or redefine a syntax highlighting style.\n\n";
            std::cout << "Token types:\n";
            for (const auto& pair : token_constants::default_styles()) {
                std::cout << "  " << pair.first << " (default: " << pair.second << ")\n";
            }
            std::cout << "\nStyle format: [bold] [italic] [underline] color=#RRGGBB|color=name\n";
            std::cout << "Color names: red, green, blue, yellow, magenta, cyan, white, black\n";
            std::cout << "ANSI colors: ansi-black, ansi-red, ansi-green, ansi-yellow, etc.\n\n";
            std::cout << "Examples:\n";
            std::cout << "  style_def builtin \"bold color=#FFB86C\"\n";
            std::cout << "  style_def system \"color=#50FA7B\"\n";
            std::cout << "  style_def comment \"italic color=green\"\n";
            std::cout << "  style_def string \"color=#F1FA8C\"\n\n";
            std::cout << "To reset all styles to defaults, use: style_def --reset\n";
            std::cout << "To preview current styles, use: style_def preview\n";
        }
        return 0;
    }

    if (args.size() == 2 && (args[1] == "preview" || args[1] == "--preview")) {
        if (!g_startup_active) {
            print_style_preview();
        }
        return 0;
    }

    if (args.size() == 2 && args[1] == "--reset") {
        for (const auto& pair : token_constants::default_styles()) {
            ic_style_def(pair.first.c_str(), pair.second.c_str());
        }
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

    if (token_constants::default_styles().find(token_type) ==
        token_constants::default_styles().end()) {
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
    ic_style_def(full_style_name.c_str(), style.c_str());
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
        "Minimum value: " + std::to_string(get_history_min_history_limit()) + " (no upper limit)."};

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

int set_completion_max_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: set-completion-max <number|default|status>",
        "",
        "Configure the maximum number of completion entries shown in menus.",
        "Use 'default' to restore the built-in limit (" +
            std::to_string(get_completion_default_max_results()) + " entries).",
        "Use 'status' to view the current setting.",
        "Minimum value: " + std::to_string(get_completion_min_allowed_results()) +
            " (no upper limit)."};

    if (args.size() == 1) {
        if (!g_startup_active) {
            for (const auto& line : usage_lines) {
                std::cout << line << '\n';
            }
        }
        print_error({ErrorType::INVALID_ARGUMENT, "set-completion-max", "expected 1 argument",
                     usage_lines});
        return 1;
    }

    if (args.size() > 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "set-completion-max",
                     "too many arguments provided", usage_lines});
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
            long current_limit = get_completion_max_results();
            std::cout << "Completion menu currently shows up to " << current_limit << " entries."
                      << '\n';
        }
        return 0;
    }

    long requested_limit = 0;
    if (normalized == "default" || normalized == "--default") {
        requested_limit = get_completion_default_max_results();
    } else {
        try {
            requested_limit = std::stol(option);
        } catch (const std::invalid_argument&) {
            print_error({ErrorType::INVALID_ARGUMENT, "set-completion-max",
                         "invalid number: " + option, usage_lines});
            return 1;
        } catch (const std::out_of_range&) {
            print_error({ErrorType::INVALID_ARGUMENT, "set-completion-max",
                         "number out of range: " + option, usage_lines});
            return 1;
        }
    }

    if (requested_limit < get_completion_min_allowed_results()) {
        print_error({ErrorType::INVALID_ARGUMENT, "set-completion-max",
                     "value must be greater than or equal to " +
                         std::to_string(get_completion_min_allowed_results()),
                     usage_lines});
        return 1;
    }

    std::string error_message;
    if (!set_completion_max_results(requested_limit, &error_message)) {
        if (error_message.empty()) {
            error_message = "Failed to update completion limit.";
        }
        print_error({ErrorType::RUNTIME_ERROR, "set-completion-max", error_message, {}});
        return 1;
    }

    if (!g_startup_active) {
        long applied_limit = get_completion_max_results();
        std::cout << "Completion menu will display up to " << applied_limit << " entries." << '\n';
    }

    return 0;
}

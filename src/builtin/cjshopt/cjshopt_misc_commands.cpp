#include "cjshopt_command.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "cjsh.h"
#include "cjsh_completions.h"
#include "error_out.h"
#include "isocline/isocline.h"
#include "token_constants.h"

namespace {

struct StartupFlagInfo {
    const char* name;
    const char* description;
};

constexpr StartupFlagInfo kStartupFlags[] = {
    {"--login", "Set login mode"},
    {"--interactive", "Force interactive mode"},
    {"--no-colors", "Disable colors"},
    {"--no-titleline", "Disable title line"},
    {"--show-startup-time", "Display shell startup time"},
    {"--no-source", "Skip sourcing configuration files"},
    {"--no-completions", "Disable tab completions"},
    {"--no-syntax-highlighting", "Disable syntax highlighting"},
    {"--no-smart-cd", "Disable smart cd functionality"},
    {"--no-history-expansion", "Disable history expansion"},
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

    auto& stored_flags = profile_startup_args();
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

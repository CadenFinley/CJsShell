#include "cjshopt_command.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "isocline/isocline.h"
#include "utils/cjsh_completions.h"

extern bool g_startup_active;

namespace {
void print_cjshopt_usage() {
    std::cout << "Usage: cjshopt <subcommand> [options]\n";
    std::cout << "Available subcommands:\n";
    std::cout << "  style_def <token_type> <style>   Define or redefine a syntax highlighting style\n";
    std::cout << "  login-startup-arg [--flag-name]  Add a startup flag to be applied when sourcing the profile\n";
    std::cout << "  completion-case <on|off|status>  Configure completion case sensitivity\n";
    std::cout << "  generate-profile [--force]       Create or overwrite ~/.cjprofile\n";
    std::cout << "  generate-rc [--force]            Create or overwrite ~/.cjshrc\n";
    std::cout << "  generate-logout [--force]        Create or overwrite ~/.cjsh_logout\n";
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
        print_error({ErrorType::INVALID_ARGUMENT,
                     "cjshopt",
                     "Missing subcommand argument",
                     {
                         "Usage: cjshopt <subcommand> [options]",
                         "Available subcommands:",
                         "  style_def <token_type> <style>   Define or redefine a syntax "
                         "highlighting style",
                         "  login-startup-arg [--flag-name]  Add a startup flag to be "
                         "applied when sourcing the profile",
                         "  completion-case <on|off|status>  Configure completion case "
                         "sensitivity",
                         "  generate-profile [--force]       Create or overwrite ~/.cjprofile",
                         "  generate-rc [--force]            Create or overwrite ~/.cjshrc"
                         "  generate-logout [--force]        Create or overwrite ~/.cjsh_logout",
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
    } else if (subcommand == "generate-profile") {
        return generate_profile_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "generate-rc") {
        return generate_rc_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else if (subcommand == "generate-logout") {
        return generate_logout_command(std::vector<std::string>(args.begin() + 1, args.end()));
    } else {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "cjshopt",
                     "unknown subcommand '" + subcommand + "'",
                     {"Available subcommands: style_def, login-startup-arg, completion-case, "
                      "generate-profile, generate-rc"}});
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

    if (args.size() == 1) {
        print_error({ErrorType::INVALID_ARGUMENT, "completion-case", "Missing option argument",
                     usage_lines});
        return 1;
    }

    if (args.size() == 2 && (args[1] == "--help" || args[1] == "-h")) {
        if (!g_startup_active) {
            for (const auto& line : usage_lines) {
                std::cout << line << '\n';
            }
            std::cout << "Current: " << (is_completion_case_sensitive() ? "enabled" : "disabled")
                      << std::endl;
        }
        return 0;
    }

    if (args.size() != 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "completion-case", "Too many arguments provided",
                     usage_lines});
        return 1;
    }

    std::string option = args[1];
    std::string normalized = option;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (normalized == "status" || normalized == "--status") {
        if (!g_startup_active) {
            std::cout << "Completion case sensitivity is currently "
                      << (is_completion_case_sensitive() ? "enabled" : "disabled") << "."
                      << std::endl;
        }
        return 0;
    }

    bool enable_case_sensitive = false;
    bool recognized_option = true;

    if (normalized == "on" || normalized == "enable" || normalized == "enabled" ||
        normalized == "true" || normalized == "1" || normalized == "case-sensitive" ||
        normalized == "--enable" || normalized == "--case-sensitive") {
        enable_case_sensitive = true;
    } else if (normalized == "off" || normalized == "disable" || normalized == "disabled" ||
               normalized == "false" || normalized == "0" || normalized == "case-insensitive" ||
               normalized == "--disable" || normalized == "--case-insensitive") {
        enable_case_sensitive = false;
    } else {
        recognized_option = false;
    }

    if (!recognized_option) {
        print_error({ErrorType::INVALID_ARGUMENT, "completion-case",
                     "Unknown option '" + option + "'", usage_lines});
        return 1;
    }

    bool currently_enabled = is_completion_case_sensitive();
    if (currently_enabled == enable_case_sensitive) {
        return 0;
    }

    set_completion_case_sensitive(enable_case_sensitive);

    if (!g_startup_active) {
        std::cout << "Completion case sensitivity "
                  << (enable_case_sensitive ? "enabled" : "disabled") << "." << std::endl;
    }

    return 0;
}

extern std::vector<std::string> g_profile_startup_args;

int startup_flag_command(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        print_error(
            {ErrorType::INVALID_ARGUMENT,
             "login-startup-arg",
             "Missing flag argument",
             {"Usage: login-startup-arg [--flag-name]",
              "Available flags:", "  --login              Set login mode",
              "  --interactive        Force interactive mode",
              "  --debug              Enable debug mode", "  --no-plugins         Disable plugins",
              "  --no-themes          Disable themes", "  --no-ai              Disable AI features",
              "  --no-colors          Disable colors", "  --no-titleline       Disable title line",
              "  --show-startup-time  Display shell startup time",
              "  --no-source          Don't source the .cjshrc file",
              "  --no-completions     Disable tab completions",
              "  --no-syntax-highlighting Disable syntax highlighting",
              "  --no-smart-cd        Disable smart cd functionality",
              "  --minimal            Disable all unique cjsh features "
              "(plugins, "
              "themes, AI, colors, completions, syntax highlighting, smart cd, "
              "sourcing, custom ls, startup time display)",
              "  --disable-custom-ls  Use system ls command instead of builtin "
              "ls",
              "  --startup-test       Enable startup test mode"}});
        return 1;
    }

    const std::string& flag = args[1];

    if (flag == "--login" || flag == "--interactive" || flag == "--debug" ||
        flag == "--no-plugins" || flag == "--no-themes" || flag == "--no-ai" ||
        flag == "--no-colors" || flag == "--no-titleline" || flag == "--show-startup-time" ||
        flag == "--no-source" || flag == "--no-completions" || flag == "--no-syntax-highlighting" ||
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

static std::unordered_map<std::string, std::string> g_custom_styles;
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
    {"function-definition", "bold color=#F1FA8C"}};

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
    std::string full_style_name = "cjsh-" + token_type;

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
        std::string full_style_name = "cjsh-" + pair.first;
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

        if (trimmed.find("style_def ") == 0) {
            std::istringstream iss(trimmed);
            std::string command, token_type, style;

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
#include "style_def_command.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "isocline/isocline.h"

// Map to store custom styles
static std::unordered_map<std::string, std::string> g_custom_styles;

// Default styles that can be customized
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
        // Show help
        std::cout << "Usage: style_def <token_type> <style>\n\n";
        std::cout << "Define or redefine a syntax highlighting style.\n\n";
        std::cout << "Token types:\n";
        for (const auto& pair : default_styles) {
            std::cout << "  " << pair.first << " (default: " << pair.second
                      << ")\n";
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
        std::cout
            << "To reset all styles to defaults, use: style_def --reset\n";
        return 0;
    }

    if (args.size() == 2 && args[1] == "--reset") {
        // Reset to default styles
        reset_to_default_styles();
        std::cout << "All syntax highlighting styles reset to defaults.\n";
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

    // Validate token type
    if (default_styles.find(token_type) == default_styles.end()) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "style_def",
                     "unknown token type: " + token_type,
                     {"Use 'style_def' to see available token types"}});
        return 1;
    }

    // Apply the style
    apply_custom_style(token_type, style);

    if (g_debug_mode) {
        std::cerr << "DEBUG: Applied style for " << token_type << ": " << style
                  << std::endl;
    }

    return 0;
}

void apply_custom_style(const std::string& token_type,
                        const std::string& style) {
    // Store the custom style
    g_custom_styles[token_type] = style;

    // Apply to isocline with the cjsh- prefix
    std::string full_style_name = "cjsh-" + token_type;
    ic_style_def(full_style_name.c_str(), style.c_str());

    if (g_debug_mode) {
        std::cerr << "DEBUG: Defined style " << full_style_name << " = "
                  << style << std::endl;
    }
}

void reset_to_default_styles() {
    g_custom_styles.clear();

    // Reapply all default styles
    for (const auto& pair : default_styles) {
        std::string full_style_name = "cjsh-" + pair.first;
        ic_style_def(full_style_name.c_str(), pair.second.c_str());
    }

    if (g_debug_mode) {
        std::cerr << "DEBUG: Reset all styles to defaults" << std::endl;
    }
}

const std::unordered_map<std::string, std::string>& get_custom_styles() {
    return g_custom_styles;
}

void load_custom_styles_from_config() {
    std::ifstream config_file(cjsh_filesystem::g_cjsh_source_path);
    if (!config_file.is_open()) {
        if (g_debug_mode) {
            std::cerr << "DEBUG: No .cjshrc file found, using default styles"
                      << std::endl;
        }
        return;
    }

    std::string line;
    int line_number = 0;

    while (std::getline(config_file, line)) {
        line_number++;

        // Skip empty lines and comments
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        // Look for style_def commands
        if (trimmed.find("style_def ") == 0) {
            // Parse the style_def command
            std::istringstream iss(trimmed);
            std::string command, token_type, style;

            // Read command
            iss >> command;

            // Read token type
            if (!(iss >> token_type)) {
                if (g_debug_mode) {
                    std::cerr << "DEBUG: Invalid style_def at line "
                              << line_number << ": missing token type"
                              << std::endl;
                }
                continue;
            }

            // Read the rest as style (handle quoted strings)
            std::string remaining;
            std::getline(iss, remaining);

            // Trim leading whitespace
            remaining.erase(0, remaining.find_first_not_of(" \t"));

            if (remaining.empty()) {
                if (g_debug_mode) {
                    std::cerr << "DEBUG: Invalid style_def at line "
                              << line_number << ": missing style" << std::endl;
                }
                continue;
            }

            // Remove quotes if present
            if ((remaining.front() == '"' && remaining.back() == '"') ||
                (remaining.front() == '\'' && remaining.back() == '\'')) {
                remaining = remaining.substr(1, remaining.length() - 2);
            }

            // Validate token type
            if (default_styles.find(token_type) != default_styles.end()) {
                apply_custom_style(token_type, remaining);
                if (g_debug_mode) {
                    std::cerr << "DEBUG: Loaded custom style from .cjshrc: "
                              << token_type << " = " << remaining << std::endl;
                }
            } else {
                if (g_debug_mode) {
                    std::cerr << "DEBUG: Unknown token type in .cjshrc at line "
                              << line_number << ": " << token_type << std::endl;
                }
            }
        }
    }

    config_file.close();
}
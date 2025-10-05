#include "theme_command.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "prompt/theme.h"
#include "prompt/theme_parser.h"

namespace {

void print_theme_help() {
    std::cout << "Usage: theme [command] [args]\n"
              << "Commands:\n"
              << "  theme                      Show current theme and list available themes\n"
              << "  theme load <name>          Load a theme by name\n"
              << "  theme info <name>          Display theme metadata and requirements\n"
              << "  theme preview <name|all>   Preview one or all local themes\n"
              << "  theme reload               Reload the active theme from disk\n"
              << "  theme uninstall <name>     Remove an installed theme\n";
}

std::filesystem::path resolve_theme_file_path(const std::string& theme_name) {
    return cjsh_filesystem::g_cjsh_theme_path / Theme::ensure_theme_extension(theme_name);
}

}  // namespace

int theme_command(const std::vector<std::string>& args) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) {
        print_theme_help();
        return 0;
    }
    if (!config::themes_enabled) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "theme",
                     "Themes are disabled",
                     {"Enable themes in configuration or remove theme commands "
                      "from startup."}});
        return 1;
    }
    if (g_theme == nullptr) {
        initialize_themes();
    }
    if (args.size() < 2) {
        if (g_theme) {
            std::cout << "Current theme: " << g_current_theme << std::endl;
            std::cout << "Available themes: " << std::endl;
            for (const auto& theme : g_theme->list_themes()) {
                std::cout << "  " << theme << std::endl;
            }
            std::cout << "\nUsage:" << std::endl;
            std::cout << "  theme <theme_name>        - Load a theme" << std::endl;
            std::cout << "  theme load <theme_name>   - Load a theme" << std::endl;
            std::cout << "  theme info <theme_name>   - Show theme information" << std::endl;
            std::cout << "  theme preview <theme_name> - Preview a local theme" << std::endl;
            std::cout << "  theme preview all         - Preview all available "
                         "local themes"
                      << std::endl;
            std::cout << "  theme reload              - Reload the active "
                         "theme from disk"
                      << std::endl;
            std::cout << "  theme uninstall <theme_name> - Uninstall a theme" << std::endl;
        } else {
            print_error({ErrorType::RUNTIME_ERROR,
                         "theme",
                         "Theme manager not initialized",
                         {"Try running 'theme' again after initialization "
                          "completes."}});
            return 1;
        }
        return 0;
    }

    if (args[1] == "reload") {
        if (g_theme) {
            if (g_theme->load_theme(g_current_theme, true)) {
                return 0;
            } else {
                return 2;
            }
        } else {
            print_error({ErrorType::RUNTIME_ERROR,
                         "theme",
                         "Theme manager not initialized",
                         {"Try running 'theme' again after initialization "
                          "completes."}});
            return 1;
        }
    }

    if (args[1] == "info" && args.size() > 2) {
        const std::string& theme_input = args[2];
        std::string themeName = Theme::strip_theme_extension(theme_input);
        std::filesystem::path theme_file = resolve_theme_file_path(themeName);

        if (!std::filesystem::exists(theme_file)) {
            print_error({ErrorType::FILE_NOT_FOUND,
                         "theme",
                         "Theme '" + themeName + "' not found",
                         {"Run 'theme' with no arguments to list available themes."}});
            return 1;
        }

        ThemeDefinition theme_def;
        try {
            theme_def = ThemeParser::parse_file(theme_file.string());
        } catch (const ThemeParseException& e) {
            std::string message = "Failed to parse theme file '" + theme_file.string() + "'";
            if (e.line() > 0) {
                message += " at line " + std::to_string(e.line());
            }
            if (!e.detail().empty()) {
                message += ": " + e.detail();
            }
            if (e.error_info()) {
                ErrorInfo error = *e.error_info();
                error.type = ErrorType::SYNTAX_ERROR;
                error.command_used = "theme";
                error.message = message;
                if (error.suggestions.empty()) {
                    error.suggestions.push_back("Check theme syntax and try again.");
                }
                print_error(error);
            } else {
                print_error({ErrorType::SYNTAX_ERROR,
                             "theme",
                             message,
                             {"Check theme syntax and try again."}});
            }
            return 1;
        } catch (const std::exception& e) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "theme",
                         "Failed to process theme '" + theme_file.string() + "': " + e.what(),
                         {}});
            return 1;
        }

        std::cout << "Theme: " << themeName << std::endl;

        int total_emoji_count = 0;
        int total_segment_count = 0;

        auto count_emoji = [](const std::string& str) {
            int emoji_count = 0;
            for (size_t i = 0; i < str.size();) {
                unsigned char c = static_cast<unsigned char>(str[i]);
                if ((c & 0xF8) == 0xF0) {
                    emoji_count++;
                    i += 4;
                } else if ((c & 0xF0) == 0xE0) {
                    emoji_count++;
                    i += 3;
                } else if ((c & 0xE0) == 0xC0) {
                    i += 2;
                } else {
                    i += 1;
                }
            }
            return emoji_count;
        };

        auto count_segments = [&](const std::vector<ThemeSegment>& segments) {
            for (const auto& segment : segments) {
                int emoji_count = count_emoji(segment.content);
                if (emoji_count > 0) {
                    total_emoji_count += emoji_count;
                    total_segment_count++;
                }
            }
        };

        count_segments(theme_def.ps1_segments);
        count_segments(theme_def.git_segments);
        count_segments(theme_def.ai_segments);
        count_segments(theme_def.newline_segments);
        count_segments(theme_def.inline_right_segments);

        const ThemeRequirements& requirements = theme_def.requirements;
        if (!requirements.colors.empty() ||
            !requirements.fonts.empty() || !requirements.custom.empty()) {
            std::cout << "Requirements:" << std::endl;

            if (!requirements.colors.empty()) {
                std::cout << "  Colors: " << requirements.colors << std::endl;
            }

            if (!requirements.fonts.empty()) {
                std::cout << "  Fonts: ";
                bool first = true;
                for (const auto& font : requirements.fonts) {
                    if (!first)
                        std::cout << ", ";
                    std::cout << font;
                    first = false;
                }
                std::cout << std::endl;
            }

            if (!requirements.custom.empty()) {
                std::cout << "  Custom requirements:" << std::endl;
                for (const auto& [key, value] : requirements.custom) {
                    std::cout << "    " << key << ": " << value << std::endl;
                }
            }
        } else {
            std::cout << "No special requirements." << std::endl;
        }

        if (total_emoji_count > 0) {
            std::cout << "\nDisplay Information:" << std::endl;
            std::cout << "  Emoji/Wide Characters: " << total_emoji_count << " (in "
                      << total_segment_count << " segments)" << std::endl;
            std::cout << "  Terminal Width Impact: Each emoji typically takes 2 "
                         "character spaces"
                      << std::endl;
            std::cout << "  Estimated Extra Width: ~" << total_emoji_count << " characters"
                      << std::endl;
            std::cout << "  Note: Minimum recommended terminal width for this theme: "
                         "100+ columns"
                      << std::endl;
        }

        return 0;
    }

    if (args[1] == "preview") {
        if (g_theme) {
            if (args.size() < 3) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             "theme",
                             "Please specify a theme name to preview or 'all'",
                             {"Usage: theme preview <theme_name>", "       theme preview all"}});
                return 1;
            }

            std::string theme_name = args[2];
            if (theme_name == "all") {
                std::vector<std::string> all_themes = g_theme->list_themes();
                all_themes.erase(std::remove(all_themes.begin(), all_themes.end(), g_current_theme),
                                 all_themes.end());
                bool success = true;

                for (const auto& theme : all_themes) {
                    std::cout << "\n\n";
                    if (preview_theme(theme) != 0) {
                        success = false;
                    }
                }

                return success ? 0 : 1;
            } else {
                std::string canonical_theme = Theme::strip_theme_extension(theme_name);
                std::filesystem::path theme_file = resolve_theme_file_path(canonical_theme);
                if (std::filesystem::exists(theme_file)) {
                    return preview_theme(canonical_theme);
                } else {
                    print_error(
                        {ErrorType::FILE_NOT_FOUND,
                         "theme",
                         "Theme '" + theme_name + "' not found locally. Please install it first.",
                         {"Run 'theme' to list installed themes."}});
                    return 1;
                }
            }
        } else {
            print_error({ErrorType::RUNTIME_ERROR,
                         "theme",
                         "Theme manager not initialized",
                         {"Try running 'theme reload' or restart the shell."}});
            return 1;
        }
    }

    if (args[1] == "uninstall") {
        if (args.size() <= 2) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "theme",
                         "Please specify a theme name to uninstall",
                         {"Usage: theme uninstall <theme_name>"}});
            return 1;
        }

        if (g_theme) {
            std::string themeName = args[2];
            return uninstall_theme(themeName);
        } else {
            print_error({ErrorType::RUNTIME_ERROR,
                         "theme",
                         "Theme manager not initialized",
                         {"Try running 'theme' again after initialization "
                          "completes."}});
            return 1;
        }
    }

    if (args[1] == "load" && args.size() > 2) {
        if (g_theme) {
            std::string themeName = args[2];
            if (g_theme->load_theme(themeName, true)) {
                return 0;
            } else {
                return 2;
            }
        } else {
            print_error({ErrorType::RUNTIME_ERROR,
                         "theme",
                         "Theme manager not initialized",
                         {"Try running 'theme' again after initialization "
                          "completes."}});
            return 1;
        }
    }

    if (g_theme) {
        std::string themeName = args[1];
        if (g_theme->load_theme(themeName, true)) {
            return 0;
        } else {
            return 2;
        }
    } else {
        print_error({ErrorType::RUNTIME_ERROR,
                     "theme",
                     "Theme manager not initialized",
                     {"Try running 'theme' again after initialization completes."}});
        return 1;
    }
}

int uninstall_theme(const std::string& themeName) {
    std::string canonical_theme = Theme::strip_theme_extension(themeName);

    if (canonical_theme == "default") {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "theme",
                     "Cannot uninstall the default theme",
                     {"The default theme is required by cjsh."}});
        return 1;
    }

    std::filesystem::path theme_file = resolve_theme_file_path(canonical_theme);

    if (!std::filesystem::exists(theme_file)) {
        print_error({ErrorType::FILE_NOT_FOUND,
                     "theme",
                     "Theme '" + canonical_theme + "' not found",
                     {"Run 'theme' to list installed themes."}});
        return 1;
    }

    if (g_current_theme == canonical_theme) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "theme",
                     "Cannot uninstall the active theme '" + canonical_theme + "'",
                     {"Switch to a different theme before uninstalling."}});
        return 1;
    }

    try {
        std::filesystem::remove(theme_file);
        std::cout << "Theme '" << canonical_theme << "' uninstalled successfully." << std::endl;
        std::cout << "If you are loading this theme from your .cjshrc file or "
                     "another source file, please remove that line."
                  << std::endl;
        return 0;
    } catch (const std::filesystem::filesystem_error& e) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "theme",
                     "Failed to uninstall theme '" + canonical_theme + "'",
                     {e.what()}});
        return 1;
    }
}

#include "theme_command.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "error_out.h"

int theme_command(const std::vector<std::string>& args) {
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
            std::cout << "  theme <theme_name>        - Load a theme"
                      << std::endl;
            std::cout << "  theme load <theme_name>   - Load a theme"
                      << std::endl;
            std::cout << "  theme info <theme_name>   - Show theme information"
                      << std::endl;
            std::cout << "  theme preview <theme_name> - Preview a local theme"
                      << std::endl;
            std::cout << "  theme preview all         - Preview all available "
                         "local themes"
                      << std::endl;
            std::cout << "  theme reload              - Reload the active "
                         "theme from disk"
                      << std::endl;
            std::cout << "  theme uninstall <theme_name> - Uninstall a theme"
                      << std::endl;
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
        std::string themeName = args[2];
        std::string theme_file = cjsh_filesystem::g_cjsh_theme_path.string() +
                                 "/" + themeName + ".json";

        if (!std::filesystem::exists(theme_file)) {
            print_error(
                {ErrorType::FILE_NOT_FOUND,
                 "theme",
                 "Theme '" + themeName + "' not found",
                 {"Run 'theme' with no arguments to list available themes."}});
            return 1;
        }

        std::ifstream file(theme_file);
        nlohmann::json theme_json;
        file >> theme_json;
        file.close();

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

        for (const auto& segment_array : {"ps1_segments", "git_segments",
                                          "ai_segments", "newline_segments"}) {
            if (theme_json.contains(segment_array) &&
                theme_json[segment_array].is_array()) {
                for (const auto& segment : theme_json[segment_array]) {
                    if (segment.contains("content") &&
                        segment["content"].is_string()) {
                        std::string content =
                            segment["content"].get<std::string>();
                        int emoji_count = count_emoji(content);
                        if (emoji_count > 0) {
                            total_emoji_count += emoji_count;
                            total_segment_count++;
                        }
                    }
                }
            }
        }

        if (theme_json.contains("requirements") &&
            theme_json["requirements"].is_object() &&
            !theme_json["requirements"].empty()) {
            std::cout << "Requirements:" << std::endl;

            if (theme_json["requirements"].contains("plugins") &&
                theme_json["requirements"]["plugins"].is_array()) {
                std::cout << "  Plugins: ";
                bool first = true;
                for (const auto& plugin :
                     theme_json["requirements"]["plugins"]) {
                    if (!first)
                        std::cout << ", ";
                    std::cout << plugin.get<std::string>();
                    first = false;
                }
                std::cout << std::endl;
            }

            if (theme_json["requirements"].contains("colors") &&
                theme_json["requirements"]["colors"].is_string()) {
                std::cout
                    << "  Colors: "
                    << theme_json["requirements"]["colors"].get<std::string>()
                    << std::endl;
            }

            if (theme_json["requirements"].contains("fonts") &&
                theme_json["requirements"]["fonts"].is_array()) {
                std::cout << "  Fonts: ";
                bool first = true;
                for (const auto& font : theme_json["requirements"]["fonts"]) {
                    if (!first)
                        std::cout << ", ";
                    std::cout << font.get<std::string>();
                    first = false;
                }
                std::cout << std::endl;
            }

            if (theme_json["requirements"].contains("custom") &&
                theme_json["requirements"]["custom"].is_object()) {
                std::cout << "  Custom requirements:" << std::endl;
                for (auto it = theme_json["requirements"]["custom"].begin();
                     it != theme_json["requirements"]["custom"].end(); ++it) {
                    std::cout << "    " << it.key() << ": " << it.value()
                              << std::endl;
                }
            }
        } else {
            std::cout << "No special requirements." << std::endl;
        }

        if (total_emoji_count > 0) {
            std::cout << "\nDisplay Information:" << std::endl;
            std::cout << "  Emoji/Wide Characters: " << total_emoji_count
                      << " (in " << total_segment_count << " segments)"
                      << std::endl;
            std::cout
                << "  Terminal Width Impact: Each emoji typically takes 2 "
                   "character spaces"
                << std::endl;
            std::cout << "  Estimated Extra Width: ~" << total_emoji_count
                      << " characters" << std::endl;
            std::cout
                << "  Note: Minimum recommended terminal width for this theme: "
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
                             {"Usage: theme preview <theme_name>",
                              "       theme preview all"}});
                return 1;
            }

            std::string theme_name = args[2];
            if (theme_name == "all") {
                std::vector<std::string> all_themes = g_theme->list_themes();
                all_themes.erase(std::remove(all_themes.begin(),
                                             all_themes.end(), g_current_theme),
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
                std::string theme_file =
                    cjsh_filesystem::g_cjsh_theme_path.string() + "/" +
                    theme_name + ".json";
                if (std::filesystem::exists(theme_file)) {
                    return preview_theme(theme_name);
                } else {
                    print_error(
                        {ErrorType::FILE_NOT_FOUND,
                         "theme",
                         "Theme '" + theme_name +
                             "' not found locally. Please install it first.",
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
        print_error(
            {ErrorType::RUNTIME_ERROR,
             "theme",
             "Theme manager not initialized",
             {"Try running 'theme' again after initialization completes."}});
        return 1;
    }
}

int uninstall_theme(const std::string& themeName) {
    if (themeName == "default") {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "theme",
                     "Cannot uninstall the default theme",
                     {"The default theme is required by cjsh."}});
        return 1;
    }

    std::string theme_file =
        cjsh_filesystem::g_cjsh_theme_path.string() + "/" + themeName + ".json";

    if (!std::filesystem::exists(theme_file)) {
        print_error({ErrorType::FILE_NOT_FOUND,
                     "theme",
                     "Theme '" + themeName + "' not found",
                     {"Run 'theme' to list installed themes."}});
        return 1;
    }

    if (g_current_theme == themeName) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "theme",
                     "Cannot uninstall the active theme '" + themeName + "'",
                     {"Switch to a different theme before uninstalling."}});
        return 1;
    }

    try {
        std::filesystem::remove(theme_file);
        std::cout << "Theme '" << themeName << "' uninstalled successfully."
                  << std::endl;
        std::cout << "If you are loading this theme from your .cjshrc file or "
                     "another source file, please remove that line."
                  << std::endl;
        return 0;
    } catch (const std::filesystem::filesystem_error& e) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "theme",
                     "Failed to uninstall theme '" + themeName + "'",
                     {e.what()}});
        return 1;
    }
}
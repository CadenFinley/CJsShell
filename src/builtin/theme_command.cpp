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

#define PRINT_ERROR(MSG)        \
  do {                          \
    std::cerr << (MSG) << '\n'; \
  } while (0)

int theme_command(const std::vector<std::string>& args) {
  if (!config::themes_enabled) {
    PRINT_ERROR("Themes are disabled");
    return 1;
  }
  if (g_theme == nullptr) {
    PRINT_ERROR("theme: theme manager not initialized");
    return 1;
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
      std::cout << "  theme info <theme_name>   - Show theme information"
                << std::endl;
      std::cout << "  theme preview <theme_name> - Preview a local theme"
                << std::endl;
      std::cout
          << "  theme preview all         - Preview all available local themes"
          << std::endl;
      std::cout << "  theme uninstall <theme_name> - Uninstall a theme"
                << std::endl;
    } else {
      std::cerr << "theme: theme manager not initialized" << std::endl;
      return 1;
    }
    return 0;
  }

  if (args[1] == "info" && args.size() > 2) {
    std::string themeName = args[2];
    std::string theme_file =
        cjsh_filesystem::g_cjsh_theme_path.string() + "/" + themeName + ".json";

    if (!std::filesystem::exists(theme_file)) {
      std::cerr << "theme: '" << themeName << "' not found" << std::endl;
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

    for (const auto& segment_array :
         {"ps1_segments", "git_segments", "ai_segments", "newline_segments"}) {
      if (theme_json.contains(segment_array) &&
          theme_json[segment_array].is_array()) {
        for (const auto& segment : theme_json[segment_array]) {
          if (segment.contains("content") && segment["content"].is_string()) {
            std::string content = segment["content"].get<std::string>();
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
        for (const auto& plugin : theme_json["requirements"]["plugins"]) {
          if (!first)
            std::cout << ", ";
          std::cout << plugin.get<std::string>();
          first = false;
        }
        std::cout << std::endl;
      }

      if (theme_json["requirements"].contains("colors") &&
          theme_json["requirements"]["colors"].is_string()) {
        std::cout << "  Colors: "
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
          std::cout << "    " << it.key() << ": " << it.value() << std::endl;
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
      std::cout << "  Estimated Extra Width: ~" << total_emoji_count
                << " characters" << std::endl;
      std::cout << "  Note: Minimum recommended terminal width for this theme: "
                   "100+ columns"
                << std::endl;
    }

    return 0;
  }

  if (args[1] == "preview") {
    if (g_theme) {
      if (args.size() < 3) {
        std::cerr << "theme: please specify a theme name to preview or 'all'"
                  << std::endl;
        return 1;
      }

      std::string theme_name = args[2];
      if (theme_name == "all") {
        std::vector<std::string> all_themes = g_theme->list_themes();
        all_themes.erase(
            std::remove(all_themes.begin(), all_themes.end(), g_current_theme),
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
        std::string theme_file = cjsh_filesystem::g_cjsh_theme_path.string() +
                                 "/" + theme_name + ".json";
        if (std::filesystem::exists(theme_file)) {
          return preview_theme(theme_name);
        } else {
          std::cerr << "theme: '" << theme_name
                    << "' not found locally. Please install it first."
                    << std::endl;
          return 1;
        }
      }
    } else {
      std::cerr << "theme: theme manager not initialized" << std::endl;
      return 1;
    }
  }

  if (args[1] == "uninstall" && args.size() > 2) {
    if (g_theme) {
      std::string themeName = args[2];
      return uninstall_theme(themeName);
    } else {
      std::cerr << "theme: theme manager not initialized" << std::endl;
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
      std::cerr << "theme: theme manager not initialized" << std::endl;
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
    std::cerr << "theme: theme manager not initialized" << std::endl;
    return 1;
  }
}

int uninstall_theme(const std::string& themeName) {
  if (themeName == "default") {
    std::cerr << "theme: cannot uninstall the default theme" << std::endl;
    return 1;
  }

  std::string theme_file =
      cjsh_filesystem::g_cjsh_theme_path.string() + "/" + themeName + ".json";

  if (!std::filesystem::exists(theme_file)) {
    std::cerr << "theme: '" << themeName << "' not found" << std::endl;
    return 1;
  }

  if (g_current_theme == themeName) {
    std::cerr << "theme: cannot uninstall the currently active theme '"
              << themeName << "'. Please switch to a different theme first."
              << std::endl;
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
    std::cerr << "theme: failed to uninstall theme '" << themeName
              << "': " << e.what() << std::endl;
    return 1;
  }
}
#include "theme_command.h"

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
  if (g_theme == nullptr) {
    PRINT_ERROR("Theme manager not initialized");
    return 1;
  }
  if (!g_theme->get_enabled()) {
    PRINT_ERROR("Themes are disabled");
    return 1;
  }
  if (args.size() < 2) {
    if (g_theme) {
      std::cout << "Current theme: " << g_current_theme << std::endl;
      std::cout << "Available themes: " << std::endl;
      for (const auto& theme : g_theme->list_themes()) {
        std::cout << "  " << theme << std::endl;
      }
    } else {
      std::cerr << "Theme manager not initialized" << std::endl;
      return 1;
    }
    return 0;
  }

  if (args[1] == "info" && args.size() > 2) {
    std::string themeName = args[2];
    std::string theme_file =
        cjsh_filesystem::g_cjsh_theme_path.string() + "/" + themeName + ".json";

    if (!std::filesystem::exists(theme_file)) {
      std::cerr << "Error: Theme '" << themeName << "' not found." << std::endl;
      return 1;
    }

    std::ifstream file(theme_file);
    nlohmann::json theme_json;
    file >> theme_json;
    file.close();

    std::cout << "Theme: " << themeName << std::endl;

    // Count emoji and wide characters in theme segments
    int total_emoji_count = 0;
    int total_segment_count = 0;

    // Helper function to count emoji characters in a string
    auto count_emoji = [](const std::string& str) {
      int emoji_count = 0;
      for (size_t i = 0; i < str.size();) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if ((c & 0xF8) == 0xF0) {
          // 4-byte UTF-8 sequence (emoji)
          emoji_count++;
          i += 4;
        } else if ((c & 0xF0) == 0xE0) {
          // 3-byte UTF-8 sequence (some wide characters)
          emoji_count++;
          i += 3;
        } else if ((c & 0xE0) == 0xC0) {
          // 2-byte UTF-8 sequence
          i += 2;
        } else {
          // ASCII character
          i += 1;
        }
      }
      return emoji_count;
    };

    // Count emoji in all segments
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
          if (!first) std::cout << ", ";
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
          if (!first) std::cout << ", ";
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

    // Display emoji and display width information
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

  if (args[1] == "load" && args.size() > 2) {
    if (g_theme) {
      std::string themeName = args[2];
      if (g_theme->load_theme(themeName)) {
        g_current_theme = themeName;
        update_theme_in_rc_file(themeName);
        return 0;
      } else {
        std::cerr << "Error: Theme '" << themeName
                  << "' not found or could not be loaded." << std::endl;
        std::cout << "Staying with current theme: '" << g_current_theme << "'"
                  << std::endl;
        return 0;
      }
    } else {
      std::cerr << "Theme manager not initialized" << std::endl;
      return 1;
    }
  }

  if (g_theme) {
    std::string themeName = args[1];
    if (g_theme->load_theme(themeName)) {
      g_current_theme = themeName;
      update_theme_in_rc_file(themeName);
      return 0;
    } else {
      std::cerr << "Error: Theme '" << themeName
                << "' not found or could not be loaded." << std::endl;
      std::cout << "Staying with current theme: '" << g_current_theme << "'"
                << std::endl;
      return 0;
    }
  } else {
    std::cerr << "Theme manager not initialized" << std::endl;
    return 1;
  }
}

int update_theme_in_rc_file(const std::string& themeName) {
  std::filesystem::path rc_path = cjsh_filesystem::g_cjsh_source_path;

  std::vector<std::string> lines;
  std::string line;
  std::ifstream read_file(rc_path);

  bool theme_line_found = false;
  size_t last_theme_line_idx = 0;

  if (read_file.is_open()) {
    while (std::getline(read_file, line)) {
      lines.push_back(line);
      if (line.find("theme ") == 0) {
        theme_line_found = true;
        last_theme_line_idx = lines.size() - 1;
      }
    }
    read_file.close();
  }

  std::string new_theme_line = "theme load " + themeName;

  if (theme_line_found) {
    lines[last_theme_line_idx] = new_theme_line;
  } else {
    lines.push_back(new_theme_line);
  }

  std::ofstream write_file(rc_path);
  if (write_file.is_open()) {
    for (const auto& l : lines) {
      write_file << l << std::endl;
    }
    write_file.close();

    if (g_debug_mode) {
      std::cout << "Theme setting updated in " << rc_path.string() << std::endl;
    }
  } else {
    PRINT_ERROR("Error: Unable to open .cjshrc file for writing at " +
                rc_path.string());
  }
  return 0;
}

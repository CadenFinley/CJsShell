#include "theme_command.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>

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
      std::cout << "\nUsage:" << std::endl;
      std::cout << "  theme <theme_name>        - Load a theme" << std::endl;
      std::cout << "  theme load <theme_name>   - Load a theme" << std::endl;
      std::cout << "  theme info <theme_name>   - Show theme information" << std::endl;
      std::cout << "  theme preview <theme_name> - Preview a theme without loading it" << std::endl;
      std::cout << "  theme preview all         - Preview all available themes" << std::endl;
      std::cout << "  theme install <theme_name> - Install a theme" << std::endl;
      std::cout << "  theme uninstall <theme_name> - Uninstall a theme" << std::endl;
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

  if (args[1] == "preview") {
    if(g_theme) {
      if (args.size() < 3) {
        std::cerr << "Error: Please specify a theme name to preview or 'all'." << std::endl;
        return 1;
      }
      
      std::string theme_name = args[2];
      if(theme_name == "all") {
        std::vector<std::string> all_themes = g_theme->list_themes();
        all_themes.erase(std::remove(all_themes.begin(), all_themes.end(), g_current_theme), all_themes.end());
        bool success = true;
        
        for (const auto& theme : all_themes) {
          std::cout << "\n\n";
          if (preview_theme(theme) != 0) {
            success = false;
          }
        }
        
        return success ? 0 : 1;
      } else {
        return preview_theme(theme_name);
      }
    } else {
      std::cerr << "Theme manager not initialized" << std::endl;
      return 1;
    }
  }

  if (args[1] == "install" && args.size() > 2) {
    if (g_theme) {
      std::string themeName = args[2];
      return install_theme(themeName);
    } else {
      std::cerr << "Theme manager not initialized" << std::endl;
      return 1;
    }
  }

  if (args[1] == "uninstall" && args.size() > 2) {
    if (g_theme) {
      std::string themeName = args[2];
      return uninstall_theme(themeName);
    } else {
      std::cerr << "Theme manager not initialized" << std::endl;
      return 1;
    }
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

int install_theme(const std::string& themeName) {
  std::string github_themes_url = "https://raw.githubusercontent.com/CadenFinley/CJsShell/master/themes/";
  std::string theme_file_name = themeName + ".json";
  std::string remote_theme_url = github_themes_url + theme_file_name;
  
  std::filesystem::path local_theme_path = cjsh_filesystem::g_cjsh_theme_path / theme_file_name;
  
  if (std::filesystem::exists(local_theme_path)) {
    std::cout << "Theme '" << themeName << "' already exists locally. Replacing with the latest version..." << std::endl;
  }
  std::string curl_cmd = "sh -c \"curl -s -f -o \"" + local_theme_path.string() + "\" " + remote_theme_url + "\"";
  std::cout << "Downloading theme '" << themeName << "' from repository..." << std::endl;
  
  int result = std::system(curl_cmd.c_str());
  if (result != 0) {
    std::cerr << "Error: Failed to download theme '" << themeName << "' from repository." << std::endl;
    std::cerr << "The theme may not exist or there might be network issues." << std::endl;
    return 1;
  }
  
  try {
    std::ifstream file(local_theme_path);
    if (!file.is_open()) {
      std::cerr << "Error: Could not open the downloaded theme file." << std::endl;
      std::filesystem::remove(local_theme_path);
      return 1;
    }
    
    nlohmann::json theme_json;
    file >> theme_json;
    file.close();
    
    std::cout << "Theme '" << themeName << "' installed successfully." << std::endl;
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: Invalid theme file format: " << e.what() << std::endl;
    std::filesystem::remove(local_theme_path);
    return 1;
  }
}

int uninstall_theme(const std::string& themeName) {
  // Check if we're trying to uninstall the default theme
  if (themeName == "default") {
    std::cerr << "Error: Cannot uninstall the default theme." << std::endl;
    return 1;
  }
  
  // Theme file path
  std::string theme_file = cjsh_filesystem::g_cjsh_theme_path.string() + "/" + themeName + ".json";
  
  // Check if theme exists
  if (!std::filesystem::exists(theme_file)) {
    std::cerr << "Error: Theme '" << themeName << "' not found." << std::endl;
    return 1;
  }
  
  // Check if theme is currently active
  bool is_active = (g_current_theme == themeName);
  
  // Remove the theme file
  try {
    std::filesystem::remove(theme_file);
    std::cout << "Theme '" << themeName << "' uninstalled successfully." << std::endl;
    
    // If theme was active, switch to default
    if (is_active) {
      std::cout << "The uninstalled theme was active. Switching to 'default' theme..." << std::endl;
      if (g_theme && g_theme->load_theme("default")) {
        g_current_theme = "default";
        update_theme_in_rc_file("default");
        std::cout << "Switched to 'default' theme." << std::endl;
      } else {
        std::cerr << "Error: Failed to load 'default' theme." << std::endl;
        return 1;
      }
    }
    
    return 0;
  } catch (const std::filesystem::filesystem_error& e) {
    std::cerr << "Error: Failed to uninstall theme '" << themeName << "': " << e.what() << std::endl;
    return 1;
  }
}

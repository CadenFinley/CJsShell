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
      std::cout << "  theme info <theme_name>   - Show theme information"
                << std::endl;
      std::cout << "  theme preview <theme_name> - Preview a theme (local or "
                   "remote) without loading it"
                << std::endl;
      std::cout
          << "  theme preview all         - Preview all available local themes"
          << std::endl;
      std::cout
          << "  theme install             - List themes available to install"
          << std::endl;
      std::cout << "  theme install <theme_name> - Install a theme"
                << std::endl;
      std::cout << "  theme uninstall <theme_name> - Uninstall a theme"
                << std::endl;
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
    if (g_theme) {
      if (args.size() < 3) {
        std::cerr << "Error: Please specify a theme name to preview or 'all'."
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
        // Check if theme exists locally
        std::string theme_file = cjsh_filesystem::g_cjsh_theme_path.string() +
                                 "/" + theme_name + ".json";
        if (std::filesystem::exists(theme_file)) {
          return preview_theme(theme_name);
        } else {
          // If not local, try to preview from repository
          std::cout
              << "Theme '" << theme_name
              << "' not found locally. Trying to preview from repository..."
              << std::endl;
          return preview_remote_theme(theme_name);
        }
      }
    } else {
      std::cerr << "Theme manager not initialized" << std::endl;
      return 1;
    }
  }

  if (args[1] == "install") {
    if (g_theme) {
      if (args.size() < 3) {
        // list all available themes to install from git repo
        return list_available_themes();
      }
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
      if (g_theme->load_theme(themeName, true, true)) {
        return 0;
      } else {
        return 2;
      }
    } else {
      std::cerr << "Theme manager not initialized" << std::endl;
      return 1;
    }
  }

  if (g_theme) {
    std::string themeName = args[1];
    if (g_theme->load_theme(themeName, true, true)) {
      return 0;
    } else {
      return 2;
    }
  } else {
    std::cerr << "Theme manager not initialized" << std::endl;
    return 1;
  }
}

int install_theme(const std::string& themeName) {
  std::string github_themes_url =
      "https://raw.githubusercontent.com/CadenFinley/CJsShell/master/themes/";
  std::string theme_file_name = themeName + "/" + themeName + ".json";
  std::string remote_theme_url = github_themes_url + theme_file_name;

  std::filesystem::path local_theme_path =
      cjsh_filesystem::g_cjsh_theme_path / (themeName + ".json");

  if (std::filesystem::exists(local_theme_path)) {
    std::cout
        << "Theme '" << themeName
        << "' already exists locally. Replacing with the latest version..."
        << std::endl;
  }
  std::string curl_cmd = "sh -c \"curl -s -f -o \"" +
                         local_theme_path.string() + "\" " + remote_theme_url +
                         "\"";
  std::cout << "Downloading theme '" << themeName << "' from repository..."
            << std::endl;

  int result = std::system(curl_cmd.c_str());
  if (result != 0) {
    std::cerr << "Error: Failed to download theme '" << themeName
              << "' from repository." << std::endl;
    std::cerr << "The theme may not exist or there might be network issues."
              << std::endl;
    return 1;
  }

  try {
    std::ifstream file(local_theme_path);
    if (!file.is_open()) {
      std::cerr << "Error: Could not open the downloaded theme file."
                << std::endl;
      std::filesystem::remove(local_theme_path);
      return 1;
    }

    nlohmann::json theme_json;
    file >> theme_json;
    file.close();

    std::cout << "Theme '" << themeName << "' installed successfully."
              << std::endl;
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
  std::string theme_file =
      cjsh_filesystem::g_cjsh_theme_path.string() + "/" + themeName + ".json";

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
    std::cout << "Theme '" << themeName << "' uninstalled successfully."
              << std::endl;

    // If theme was active, switch to default
    if (is_active) {
      std::cout
          << "The uninstalled theme was active. Switching to 'default' theme..."
          << std::endl;
      if (g_theme && g_theme->load_theme("default", true, true)) {
        std::cout << "Switched to 'default' theme." << std::endl;
      } else {
        std::cerr << "Error: Failed to load 'default' theme." << std::endl;
        return 1;
      }
    }

    return 0;
  } catch (const std::filesystem::filesystem_error& e) {
    std::cerr << "Error: Failed to uninstall theme '" << themeName
              << "': " << e.what() << std::endl;
    return 1;
  }
}

int list_available_themes() {
  std::cout << "Fetching available themes from repository..." << std::endl;

  // Use GitHub API to list contents of the themes directory
  std::string github_api_url =
      "https://api.github.com/repos/CadenFinley/CJsShell/contents/themes";

  // Use curl to fetch the contents using the GitHub API
  std::array<char, 128> buffer;
  std::string result;
  std::string curl_command = "curl -s --connect-timeout 10 " + github_api_url;
  FILE* raw_pipe = popen(curl_command.c_str(), "r");

  // Using a custom deleter function instead of direct decltype(&pclose)
  auto pipe_deleter = [](FILE* f) { pclose(f); };
  std::unique_ptr<FILE, decltype(pipe_deleter)> pipe(raw_pipe, pipe_deleter);

  if (!pipe) {
    std::cerr << "Error: Failed to connect to GitHub repository." << std::endl;
    std::cerr << "Please check your internet connection and try again."
              << std::endl;
    return 1;
  }

  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }

  // Check if we got a valid response
  if (result.empty() || result.find("Not Found") != std::string::npos) {
    std::cerr << "Error: Could not access GitHub repository." << std::endl;
    std::cerr << "Please check your internet connection and try again."
              << std::endl;
    return 1;
  }

  // Parse JSON response
  std::vector<std::string> available_themes;  // Theme names
  try {
    nlohmann::json contents = nlohmann::json::parse(result);

    if (!contents.is_array()) {
      std::cerr << "Error: Unexpected response from GitHub API." << std::endl;
      return 1;
    }

    for (const auto& item : contents) {
      if (item.contains("name") && item["name"].is_string()) {
        std::string theme_name = item["name"].get<std::string>();

        // Skip special theme files
        if (theme_name != "all_features_theme" &&
            theme_name != "plugin_test") {
          available_themes.push_back(theme_name);
        }
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Error parsing repository data: " << e.what() << std::endl;
    return 1;
  }

  if (available_themes.empty()) {
    std::cout << "No themes found or unable to parse repository contents."
              << std::endl;
    std::cout << "Please check your internet connection and try again."
              << std::endl;
    return 1;
  }

  // Sort themes alphabetically by name
  std::sort(available_themes.begin(), available_themes.end());

  // Get list of locally installed themes
  std::vector<std::string> installed_themes;
  try {
    for (const auto& entry : std::filesystem::directory_iterator(
             cjsh_filesystem::g_cjsh_theme_path)) {
      if (entry.is_regular_file() && entry.path().extension() == ".json") {
        std::string theme_name = entry.path().stem().string();
        installed_themes.push_back(theme_name);
      }
    }
  } catch (const std::filesystem::filesystem_error& e) {
    std::cerr << "Warning: Could not read local themes directory: " << e.what()
              << std::endl;
  }

  // Print available themes
  std::cout << "Available themes to install:" << std::endl;
  for (const auto& theme : available_themes) {
    bool is_installed =
        std::find(installed_themes.begin(), installed_themes.end(), theme) !=
        installed_themes.end();
    std::cout << "  " << theme;
    if (is_installed) {
      std::cout << " [installed]";
    }
    std::cout << std::endl;
  }

  std::cout << "\nTo install a theme, use: theme install <theme_name>"
            << std::endl;
  std::cout << "To update an already installed theme, use the same command."
            << std::endl;
  std::cout
      << "For more information about a theme, use: theme info <theme_name>"
      << std::endl;
  return 0;
}

int preview_remote_theme(const std::string& themeName) {
  std::cout << "Fetching theme information for '" << themeName
            << "' from repository..." << std::endl;

  // GitHub raw content URL for the theme file
  std::string github_theme_url =
      "https://raw.githubusercontent.com/CadenFinley/CJsShell/master/themes/" +
      themeName + ".json";

  // Use curl to fetch the theme file
  std::array<char, 128> buffer2;
  std::string result;
  std::string curl_command = "curl -s --connect-timeout 10 " + github_theme_url;
  FILE* raw_pipe = popen(curl_command.c_str(), "r");

  // Using a custom deleter function instead of direct decltype(&pclose)
  auto pipe_deleter = [](FILE* f) { pclose(f); };
  std::unique_ptr<FILE, decltype(pipe_deleter)> pipe(raw_pipe, pipe_deleter);

  if (!pipe) {
    std::cerr << "Error: Failed to connect to GitHub repository." << std::endl;
    std::cerr << "Please check your internet connection and try again."
              << std::endl;
    return 1;
  }

  while (fgets(buffer2.data(), buffer2.size(), pipe.get()) != nullptr) {
    result += buffer2.data();
  }

  // Check if we got a valid response
  if (result.empty() || result.find("404: Not Found") != std::string::npos) {
    std::cerr << "Error: Theme '" << themeName << "' not found in repository."
              << std::endl;
    return 1;
  }

  // Parse JSON response
  try {
    nlohmann::json theme_json = nlohmann::json::parse(result);

    std::cout << "Theme: " << themeName << " (Remote)" << std::endl;

    // Display theme requirements if available
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

    // Count segments
    int total_segments = 0;

    for (const auto& segment_array :
         {"ps1_segments", "git_segments", "ai_segments", "newline_segments"}) {
      if (theme_json.contains(segment_array) &&
          theme_json[segment_array].is_array()) {
        total_segments += theme_json[segment_array].size();
      }
    }

    std::cout << "\nTheme Structure:" << std::endl;
    std::cout << "  Total Segments: " << total_segments << std::endl;

    // Show segment types distribution
    if (theme_json.contains("ps1_segments") &&
        theme_json["ps1_segments"].is_array()) {
      std::cout << "  Main Prompt Segments: "
                << theme_json["ps1_segments"].size() << std::endl;
    }

    if (theme_json.contains("git_segments") &&
        theme_json["git_segments"].is_array()) {
      std::cout << "  Git Segments: " << theme_json["git_segments"].size()
                << std::endl;
    }

    if (theme_json.contains("ai_segments") &&
        theme_json["ai_segments"].is_array()) {
      std::cout << "  AI Segments: " << theme_json["ai_segments"].size()
                << std::endl;
    }

    if (theme_json.contains("newline_segments") &&
        theme_json["newline_segments"].is_array()) {
      std::cout << "  Newline Segments: "
                << theme_json["newline_segments"].size() << std::endl;
    }

    std::cout << "\nTo install this theme, use: theme install " << themeName
              << std::endl;

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error parsing theme data: " << e.what() << std::endl;
    return 1;
  }
}

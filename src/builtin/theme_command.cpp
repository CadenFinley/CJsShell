#include "theme_command.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "cjsh_filesystem.h"
#include "cjsh.h"

#define PRINT_ERROR(MSG)        \
  do {                          \
    std::cerr << (MSG) << '\n'; \
  } while (0)

int theme_command(const std::vector<std::string>& args) {
  if (g_theme == nullptr) {
    PRINT_ERROR("Theme manager not initialized");
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

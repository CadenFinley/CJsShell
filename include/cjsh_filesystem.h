#pragma once

#include <unistd.h>
#include <limits.h>
#include <filesystem>
#include <iostream>

// the cjsh file system
namespace cjsh_filesystem {
  namespace fs = std::filesystem;
  
  // ALL STORED IN FULL PATHS
  const fs::path g_user_home_path = []() {
    const char* home = std::getenv("HOME");
    if (!home || home[0] == '\0') {
      std::cerr << "Warning: HOME environment variable not set or empty. Using /tmp as fallback." << std::endl;
      return fs::path("/tmp");
    }
    return fs::path(home);
  }();

  extern fs::path g_cjsh_path; // where the executable is located

  // used if login
  const fs::path g_cjsh_profile_path = g_user_home_path / ".cjprofile"; //envvars and PATH setup

  // used if interactive
  const fs::path g_cjsh_source_path = g_user_home_path / ".cjshrc"; // aliases, prompt, functions, themes
  
  const fs::path g_cjsh_data_path = g_user_home_path / ".cjsh"; // directory for all cjsh things
  const fs::path g_cjsh_plugin_path = g_cjsh_data_path / "plugins";
  const fs::path g_cjsh_theme_path = g_cjsh_data_path / "themes";
  const fs::path g_cjsh_history_path = g_cjsh_data_path / "history.txt";
  const fs::path g_cjsh_uninstall_path = g_cjsh_data_path / "uninstall.sh";
  const fs::path g_cjsh_update_cache_path = g_cjsh_data_path / "update_cache.json";
}

bool initialize_cjsh_path();

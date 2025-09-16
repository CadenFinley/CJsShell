#pragma once

#include <limits.h>
#include <unistd.h>

#include <filesystem>
#include <iostream>
#include <vector>

// the cjsh file system
namespace cjsh_filesystem {
namespace fs = std::filesystem;

// ALL STORED IN FULL PATHS
const fs::path g_user_home_path = []() {
  const char* home = std::getenv("HOME");
  if (!home || home[0] == '\0') {
    std::cerr << "Warning: HOME environment variable not set or empty. Using "
                 "/tmp as fallback."
              << std::endl;
    return fs::path("/tmp");
  }
  return fs::path(home);
}();

// This needs to be non-const because it's initialized at runtime
extern fs::path g_cjsh_path;

// used if login
const fs::path g_cjsh_profile_path =
    g_user_home_path /
    ".cjprofile";  // envvars loaded on login shell also startup flags

// used if interactive
const fs::path g_cjsh_source_path =
    g_user_home_path / ".cjshrc";  // aliases, prompt, functions, themes loaded
                                   // on interactive shell

const fs::path g_config_path =
    g_user_home_path / ".config";                           // config directory
const fs::path g_cache_path = g_user_home_path / ".cache";  // cache directory

const fs::path g_cjsh_data_path =
    g_config_path / "cjsh";  // directory for all cjsh things
const fs::path g_cjsh_cache_path =
    g_cache_path / "cjsh";  // cache directory for cjsh

const fs::path g_cjsh_plugin_path =
    g_cjsh_data_path / "plugins";  // where all plugins are stored
const fs::path g_cjsh_theme_path =
    g_cjsh_data_path / "themes";  // where all themes are stored
const fs::path g_cjsh_history_path =
    g_cjsh_cache_path / "history.txt";  // where the history is stored

const fs::path g_cjsh_ai_config_path =
    g_cjsh_data_path / "ai";  // where the ai config is stored
const fs::path g_cjsh_ai_config_file_path =
    g_cjsh_ai_config_path / "config.json";  // where the ai config is stored
const fs::path g_cjsh_ai_default_config_path =
    g_cjsh_ai_config_path / "default.json";  // default ai config

const fs::path g_cjsh_ai_conversations_path =
    g_cjsh_cache_path /
    "conversations";  // where the ai conversations are stored

const fs::path g_cjsh_found_executables_path =
    g_cjsh_cache_path /
    "cached_executables.cache";  // where the found executables are stored for
                               // syntax highlighting and completions

std::vector<fs::path> read_cached_executables();
bool build_executable_cache();
bool file_exists(const cjsh_filesystem::fs::path& path);
bool should_refresh_executable_cache();
bool initialize_cjsh_path();
bool initialize_cjsh_directories();
std::filesystem::path get_cjsh_path();
std::string find_executable_in_path(const std::string& name);
}  // namespace cjsh_filesystem

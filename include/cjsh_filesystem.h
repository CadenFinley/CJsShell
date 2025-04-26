#pragma once

#include <string>

// the cjsh file system
namespace cjsh_filesystem {
  // ALL STORED IN FULL PATHS
  const std::string g_user_home_path = std::getenv("HOME") ? std::getenv("HOME") : "";
  extern std::string g_cjsh_path; // Modified: removed const, added extern

  // used if login
  const std::string g_cjsh_config_path = g_user_home_path + "/.cjprofile"; //envvars and PATH setup

  // used if interactive
  const std::string g_cjsh_data_path = g_user_home_path + "/.cjsh_data";
  const std::string g_cjsh_plugin_path = g_cjsh_data_path + "/plugins";
  const std::string g_cjsh_theme_path = g_cjsh_data_path + "/themes";
  const std::string g_cjsh_history_path = g_cjsh_data_path + "/history.txt";
  const std::string g_cjsh_uninstall_path = g_cjsh_data_path + "/uninstall.sh";
  const std::string g_cjsh_update_cache_path = g_cjsh_data_path + "/update_cache.json";
  const std::string g_cjsh_source_path = g_user_home_path + "/.cjshrc"; // aliases, prompt, functions, themes
};

#include "basic_info.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <pwd.h>
#include <unistd.h>

#include "cjsh.h"

bool BasicInfo::is_root_path(const std::filesystem::path& path) {
  if (g_debug_mode)
    std::cerr << "DEBUG: is_root_path for " << path.string() << std::endl;
  return path == path.root_path();
}

std::string BasicInfo::get_current_file_path() {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_current_file_path START" << std::endl;

  std::string path = std::filesystem::current_path().string();

  if (path == "/") {
    if (g_debug_mode)
      std::cerr << "DEBUG: get_current_file_path END: /" << std::endl;
    return "/";
  }

  char* home_dir = getenv("HOME");
  if (home_dir) {
    std::string home_str(home_dir);
    if (path.length() >= home_str.length() &&
        path.substr(0, home_str.length()) == home_str) {
      if (path == home_str) {
        path = "~";
      } else {
        path = "~" + path.substr(home_str.length());
      }
    }
  }

  if (g_debug_mode)
    std::cerr << "DEBUG: get_current_file_path END: " << path << std::endl;
  return path;
}

std::string BasicInfo::get_current_file_name() {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_current_file_name START" << std::endl;

  std::filesystem::path current_path = std::filesystem::current_path();
  std::string filename = current_path.filename().string();

  if (filename.empty()) {
    // This might happen for root directory
    filename = current_path.string();
  }

  // Handle home directory special case
  char* home_dir = getenv("HOME");
  if (home_dir) {
    std::string home_str(home_dir);
    std::string current_str = current_path.string();
    
    if (current_str == home_str) {
      filename = "~";
    } else if (current_str.length() > home_str.length() &&
               current_str.substr(0, home_str.length()) == home_str &&
               current_str[home_str.length()] == '/') {
      // We're in a subdirectory of home, just return the directory name
      filename = current_path.filename().string();
    }
  }

  if (g_debug_mode)
    std::cerr << "DEBUG: get_current_file_name END: " << filename << std::endl;
  return filename;
}

std::string BasicInfo::get_username() {
  uid_t uid = getuid();
  struct passwd* pw = getpwuid(uid);
  if (pw) {
    return std::string(pw->pw_name);
  }
  return "unknown";
}

std::string BasicInfo::get_hostname() {
  char hostname[256];
  if (gethostname(hostname, sizeof(hostname)) == 0) {
    return std::string(hostname);
  }
  return "unknown";
}
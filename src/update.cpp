#include "update.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <thread>

#include "cjsh.h"
#include "cjsh_filesystem.h"

using json = nlohmann::json;

static std::vector<int> parseVersion(const std::string& ver) {
  std::vector<int> parts;
  std::istringstream iss(ver);
  std::string token;
  while (std::getline(iss, token, '.')) {
    parts.push_back(std::stoi(token));
  }
  return parts;
}

static bool is_newer_version(const std::string& latest,
                             const std::string& current) {
  auto a = parseVersion(latest), b = parseVersion(current);
  size_t n = std::max(a.size(), b.size());
  a.resize(n);
  b.resize(n);
  for (size_t i = 0; i < n; i++) {
    if (a[i] > b[i]) return true;
    if (a[i] < b[i]) return false;
  }
  return false;
}

bool check_for_update() {
  if (!g_silent_update_check)
    std::cout << "Checking for updates from GitHub...";
  std::string cmd = "curl -s " + c_update_url, result;
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    std::cerr << "Error: Unable to execute update check.\n";
    return false;
  }
  char buf[128];
  while (fgets(buf, sizeof(buf), pipe)) result += buf;
  pclose(pipe);
  try {
    auto data = json::parse(result);
    if (data.contains("tag_name")) {
      std::string latest = data["tag_name"].get<std::string>();
      if (!latest.empty() && latest[0] == 'v') latest.erase(0, 1);
      std::string current = c_version;
      if (!current.empty() && current[0] == 'v') current.erase(0, 1);
      g_cached_version = latest;
      if (is_newer_version(latest, current)) {
        std::cout << "\nLast Updated: " << g_last_updated << "\n"
                  << c_version << " -> " << latest << std::endl;
        return true;
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Error parsing update data: " << e.what() << std::endl;
  }
  return false;
}

bool load_update_cache() {
  auto& path = cjsh_filesystem::g_cjsh_update_cache_path;
  if (!std::filesystem::exists(path)) return false;
  std::ifstream f(path);
  if (!f.is_open()) return false;
  try {
    json cache;
    f >> cache;
    f.close();
    if (cache.contains("update_available") &&
        cache.contains("latest_version") && cache.contains("check_time")) {
      g_cached_update = cache["update_available"].get<bool>();
      g_cached_version = cache["latest_version"].get<std::string>();
      g_last_update_check = cache["check_time"].get<time_t>();
      return true;
    }
  } catch (const std::exception& e) {
    std::cerr << "Error loading update cache: " << e.what() << std::endl;
  }
  return false;
}

void save_update_cache(bool upd, const std::string& latest) {
  namespace fs = std::filesystem;
  auto path = cjsh_filesystem::g_cjsh_update_cache_path;
  fs::create_directories(fs::path(path).parent_path());

  json cache;
  cache["update_available"] = upd;
  cache["latest_version"] = latest;
  cache["check_time"] = std::time(nullptr);

  std::ofstream f(path);
  if (f.is_open()) {
    if (g_debug_mode) std::cout << "Writing update cache to: " << path << "\n";
    f << cache.dump(4) << "\n";
    f.close();
    g_cached_update = upd;
    g_cached_version = latest;
    g_last_update_check = std::time(nullptr);
  } else {
    std::cerr << "Warning: Could not open update cache file for writing: "
              << path << "\n";
  }
}

bool should_check_for_updates() {
  if (g_last_update_check == 0) return true;
  time_t now = std::time(nullptr);
  time_t elapsed = now - g_last_update_check;
  if (g_debug_mode) {
    std::cout << "Time since last update check: " << elapsed
              << " sec (interval: " << g_update_check_interval << ")\n";
  }
  return elapsed > g_update_check_interval;
}

bool execute_update_if_available(bool avail) {
  if (!avail) {
    std::cout << "\nYou are up to date!.\n";
    return false;
  }
  std::cout << "\nAn update is available. Please run:\n"
            << "  brew upgrade cadenfinley/tap/cjsh\n";
  std::cout << "or would you like to automattically run that command? (y/n): ";
  std::string response;
  std::getline(std::cin, response);
  if (response == "y" || response == "Y") {
    std::string cmd = "brew upgrade cadenfinley/tap/cjsh";
    std::cout << "Running command: " << cmd << "\n";
    int exit_code = g_shell->execute_command(cmd);
    if (exit_code == 0) {
      std::cout << "Update completed successfully.\n";
      // Update the cache to reflect the current version after successful update
      save_update_cache(false, c_version);
      return true;
    } else {
      std::cerr << "Error: Update command failed with exit code " << exit_code
                << "\n";
      return false;
    }
  }
  return false;
}

void display_changelog(const std::string& path) {
  std::ifstream f(path);
  if (!f.is_open()) return;
  std::string content((std::istreambuf_iterator<char>(f)), {});
  f.close();
  std::cout << "\n===== CHANGELOG =====\n"
            << content << "\n=====================\n";
}

void startup_update_process() {
  g_first_boot = !std::filesystem::exists(cjsh_filesystem::g_cjsh_cache_path /
                                          ".first_boot_complete");
  if (g_first_boot) {
    std::cout << "\n" << get_colorized_splash() << "\nWelcome to CJ's Shell!\n";
    mark_first_boot_complete();
    g_title_line = false;
  }
  auto cl = (cjsh_filesystem::g_cjsh_cache_path / "CHANGELOG.txt").string();
  if (std::filesystem::exists(cl)) {
    display_changelog(cl);
    std::filesystem::rename(
        cl, cjsh_filesystem::g_cjsh_cache_path / "latest_changelog.txt");
    g_last_updated = get_current_time_string();
    // After displaying changelog (which indicates a successful update), update the cache
    save_update_cache(false, c_version);
  } else if (g_check_updates) {
    bool upd = false;
    if (load_update_cache()) {
      // Check if cached version matches current version
      std::string current = c_version;
      if (!current.empty() && current[0] == 'v') current.erase(0, 1);
      std::string cached = g_cached_version;
      if (!cached.empty() && cached[0] == 'v') cached.erase(0, 1);
      
      // If current version matches or is newer than cached version, we're updated
      if (!cached.empty() && !is_newer_version(cached, current)) {
        // Reset cache if we're at or beyond the cached version
        save_update_cache(false, current);
        upd = false;
      } else if (should_check_for_updates()) {
        upd = check_for_update();
        save_update_cache(upd, g_cached_version);
      } else if (g_cached_update) {
        upd = true;
        if (!g_silent_update_check)
          std::cout << "\nUpdate available: " << g_cached_version
                    << " (cached)\n";
      }
    } else {
      upd = check_for_update();
      save_update_cache(upd, g_cached_version);
    }
    if (upd)
      execute_update_if_available(upd);
    else if (!g_silent_update_check)
      std::cout << " You are up to date!\n";
  }
}

bool is_first_boot() {
  std::filesystem::path first_boot_flag =
      cjsh_filesystem::g_cjsh_cache_path / ".first_boot_complete";
  return !std::filesystem::exists(first_boot_flag);
}

void mark_first_boot_complete() {
  std::filesystem::path first_boot_flag =
      cjsh_filesystem::g_cjsh_cache_path / ".first_boot_complete";
  std::ofstream flag_file(first_boot_flag);
  flag_file.close();
}

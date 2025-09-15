#pragma once

#include <pwd.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "nlohmann/json.hpp"
#include "basic_info.h"
#include "git_info.h"
#include "system_info.h"
#include "environment_info.h"
#include "network_info.h"
#include "time_info.h"

class PromptInfo {
 private:
  // Module instances for different information types
  BasicInfo basic_info;
  GitInfo git_info;
  SystemInfo system_info;
  EnvironmentInfo environment_info;
  NetworkInfo network_info;
  TimeInfo time_info;

 public:
 // Core methods that remain in PromptInfo
  std::string get_basic_prompt();
  std::string get_basic_title();
  std::string get_basic_ai_prompt();
  bool is_variable_used(const std::string& var_name,
                        const std::vector<nlohmann::json>& segments);
  std::unordered_map<std::string, std::string> get_variables(
      const std::vector<nlohmann::json>& segments, bool is_git_repo = false,
      const std::filesystem::path& repo_root = {});

  // Delegated methods to respective modules
  // Basic Info
  bool is_root_path(const std::filesystem::path& path) { return basic_info.is_root_path(path); }
  std::string get_current_file_name() { return basic_info.get_current_file_name(); }
  std::string get_current_file_path() { return basic_info.get_current_file_path(); }
  std::string get_username() { return basic_info.get_username(); }
  std::string get_hostname() { return basic_info.get_hostname(); }

  // Git Info
  std::string get_git_branch(const std::filesystem::path& git_head_path) { return git_info.get_git_branch(git_head_path); }
  std::string get_git_status(const std::filesystem::path& repo_root) { return git_info.get_git_status(repo_root); }
  std::string get_local_path(const std::filesystem::path& repo_root) { return git_info.get_local_path(repo_root); }
  std::string get_git_remote(const std::filesystem::path& repo_root) { return git_info.get_git_remote(repo_root); }
  std::string get_git_tag(const std::filesystem::path& repo_root) { return git_info.get_git_tag(repo_root); }
  std::string get_git_last_commit(const std::filesystem::path& repo_root) { return git_info.get_git_last_commit(repo_root); }
  std::string get_git_author(const std::filesystem::path& repo_root) { return git_info.get_git_author(repo_root); }
  int get_git_ahead_behind(const std::filesystem::path& repo_root, int& ahead, int& behind) { return git_info.get_git_ahead_behind(repo_root, ahead, behind); }
  int get_git_stash_count(const std::filesystem::path& repo_root) { return git_info.get_git_stash_count(repo_root); }
  bool get_git_has_staged_changes(const std::filesystem::path& repo_root) { return git_info.get_git_has_staged_changes(repo_root); }
  int get_git_uncommitted_changes(const std::filesystem::path& repo_root) { return git_info.get_git_uncommitted_changes(repo_root); }

  // System Info
  std::string get_os_info() { return system_info.get_os_info(); }
  std::string get_kernel_version() { return system_info.get_kernel_version(); }
  float get_cpu_usage() { return system_info.get_cpu_usage(); }
  float get_memory_usage() { return system_info.get_memory_usage(); }
  std::string get_battery_status() { return system_info.get_battery_status(); }
  std::string get_uptime() { return system_info.get_uptime(); }
  std::string get_disk_usage(const std::filesystem::path& path = "/") { return system_info.get_disk_usage(path); }
  std::string get_swap_usage() { return system_info.get_swap_usage(); }
  std::string get_load_avg() { return system_info.get_load_avg(); }

  // Environment Info
  std::string get_terminal_type() { return environment_info.get_terminal_type(); }
  std::pair<int, int> get_terminal_dimensions() { return environment_info.get_terminal_dimensions(); }
  std::string get_active_language_version(const std::string& language) { return environment_info.get_active_language_version(language); }
  bool is_in_virtual_environment(std::string& env_name) { return environment_info.is_in_virtual_environment(env_name); }
  int get_background_jobs_count() { return environment_info.get_background_jobs_count(); }
  std::string get_shell() { return environment_info.get_shell(); }
  std::string get_shell_version() { return environment_info.get_shell_version(); }

  // Network Info
  std::string get_ip_address(bool external = false) { return network_info.get_ip_address(external); }
  bool is_vpn_active() { return network_info.is_vpn_active(); }
  std::string get_active_network_interface() { return network_info.get_active_network_interface(); }

  // Time Info
  std::string get_current_time(bool twelve_hour_format = false) { return time_info.get_current_time(twelve_hour_format); }
  std::string get_current_date() { return time_info.get_current_date(); }
  int get_current_day() { return time_info.get_current_day(); }
  int get_current_month() { return time_info.get_current_month(); }
  int get_current_year() { return time_info.get_current_year(); }
  std::string get_current_day_name() { return time_info.get_current_day_name(); }
  std::string get_current_month_name() { return time_info.get_current_month_name(); }
};
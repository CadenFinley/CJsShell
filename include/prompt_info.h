#pragma once

#include <string>
#include <filesystem>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <pwd.h>
#include <unistd.h>
#include <sys/ioctl.h> 
#include "nlohmann/json.hpp"

class PromptInfo {
private:
    std::chrono::steady_clock::time_point last_git_status_check;
    std::string cached_git_dir;
    std::string cached_status_symbols;
    bool cached_is_clean_repo;
    std::mutex git_status_mutex;
    bool is_git_status_check_running;
    std::unordered_map<std::string, std::pair<std::string, std::chrono::steady_clock::time_point>> cache;
    std::mutex cache_mutex;

    bool is_root_path(const std::filesystem::path& path);
    template<typename F>
    std::string get_cached_value(const std::string& key, F value_func, int ttl_seconds = 60);

public:
    PromptInfo();
    ~PromptInfo();
    bool is_git_repository(std::filesystem::path& repo_root);
    std::string get_git_branch(const std::filesystem::path& git_head_path);
    std::string get_git_status(const std::filesystem::path& repo_root);
    std::string get_local_path(const std::filesystem::path& repo_root);
    int get_git_ahead_behind(const std::filesystem::path& repo_root, int& ahead, int& behind);
    int get_git_stash_count(const std::filesystem::path& repo_root);
    bool get_git_has_staged_changes(const std::filesystem::path& repo_root);
    int get_git_uncommitted_changes(const std::filesystem::path& repo_root);
    std::string get_current_file_name();
    std::string get_current_file_path();
    std::string get_username();
    std::string get_hostname();
    std::string get_os_info();
    std::string get_kernel_version();
    float get_cpu_usage();
    float get_memory_usage();
    std::string get_battery_status();
    std::string get_uptime();
    std::string get_terminal_type();
    std::pair<int, int> get_terminal_dimensions();
    std::string get_active_language_version(const std::string& language);
    bool is_in_virtual_environment(std::string& env_name);
    std::string get_ip_address(bool external = false);
    bool is_vpn_active();
    std::string get_active_network_interface();
    int get_background_jobs_count();
    std::string get_current_time(bool twelve_hour_format = false);
    std::string get_current_date();
    std::string get_shell();
    std::string get_shell_version();
    bool is_variable_used(const std::string& var_name, const std::vector<nlohmann::json>& segments);
    std::unordered_map<std::string, std::string> get_variables(const std::vector<nlohmann::json>& segments, bool is_git_repo = false, const std::filesystem::path& repo_root = {});
};

template<typename F>
std::string PromptInfo::get_cached_value(const std::string& key, F value_func, int ttl_seconds) {
    auto now = std::chrono::steady_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto it = cache.find(key);
        if (it != cache.end()) {
            auto& [value, timestamp] = it->second;
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - timestamp).count();
            if (elapsed < ttl_seconds) {
                return value;
            }
        }
    }
    
    std::string value = value_func();
    
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        cache[key] = {value, now};
    }
    
    return value;
}

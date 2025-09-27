#pragma once

#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

class GitInfo {
   private:
    std::chrono::steady_clock::time_point last_git_status_check;
    std::string cached_git_dir;
    std::string cached_status_symbols;
    bool cached_is_clean_repo;
    std::mutex git_status_mutex;
    bool is_git_status_check_running;
    std::unordered_map<
        std::string,
        std::pair<std::string, std::chrono::steady_clock::time_point>>
        cache;
    std::mutex cache_mutex;

    template <typename F>
    std::string get_cached_value(const std::string& key, F value_func,
                                 int ttl_seconds = 60);

   public:
    GitInfo();
    ~GitInfo();

    std::string get_git_branch(const std::filesystem::path& git_head_path);
    std::string get_git_status(const std::filesystem::path& repo_root);
    std::string get_local_path(const std::filesystem::path& repo_root);
    std::string get_git_remote(const std::filesystem::path& repo_root);
    std::string get_git_tag(const std::filesystem::path& repo_root);
    std::string get_git_last_commit(const std::filesystem::path& repo_root);
    std::string get_git_author(const std::filesystem::path& repo_root);
    int get_git_ahead_behind(const std::filesystem::path& repo_root, int& ahead,
                             int& behind);
    int get_git_stash_count(const std::filesystem::path& repo_root);
    bool get_git_has_staged_changes(const std::filesystem::path& repo_root);
    int get_git_uncommitted_changes(const std::filesystem::path& repo_root);
};

template <typename F>
std::string GitInfo::get_cached_value(const std::string& key, F value_func,
                                      int ttl_seconds) {
    auto now = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto it = cache.find(key);
        if (it != cache.end()) {
            auto& [value, timestamp] = it->second;
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                               now - timestamp)
                               .count();
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
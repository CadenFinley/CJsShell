#pragma once

#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

extern std::chrono::steady_clock::time_point last_git_status_check;
extern std::string cached_git_dir;
extern std::string cached_status_symbols;
extern bool cached_is_clean_repo;
extern std::mutex git_status_mutex;
extern bool is_git_status_check_running;

std::string get_git_branch(const std::filesystem::path& git_head_path);
std::string get_git_status(const std::filesystem::path& repo_root);
std::string get_local_path(const std::filesystem::path& repo_root);
std::string get_git_remote(const std::filesystem::path& repo_root);
std::string get_git_tag(const std::filesystem::path& repo_root);
std::string get_git_last_commit(const std::filesystem::path& repo_root);
std::string get_git_author(const std::filesystem::path& repo_root);
int get_git_ahead_behind(const std::filesystem::path& repo_root, int& ahead, int& behind);
int get_git_stash_count(const std::filesystem::path& repo_root);
bool get_git_has_staged_changes(const std::filesystem::path& repo_root);
int get_git_uncommitted_changes(const std::filesystem::path& repo_root);
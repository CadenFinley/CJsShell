#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace directory_info {


extern bool use_logical_path;
extern bool truncate_to_repo;
extern int truncation_length;
extern std::string truncation_symbol;
extern std::string home_symbol;
extern std::unordered_map<std::string, std::string> substitutions;


std::string contract_path(const std::filesystem::path& path,
                          const std::filesystem::path& home_dir,
                          const std::string& home_symbol);
std::string contract_repo_path(const std::filesystem::path& path,
                               const std::filesystem::path& repo_root);
std::string substitute_path(const std::string& path,
                            const std::unordered_map<std::string, std::string>& substitutions);
std::string truncate_path(const std::string& path, int max_length);
std::string to_fish_style(int dir_length, const std::string& full_path,
                          const std::string& truncated_path);
bool is_readonly_dir(const std::filesystem::path& path);


std::string get_display_directory();
std::string get_directory_name();
std::string get_truncated_path();
std::string get_repo_relative_path(const std::filesystem::path& repo_root);
bool is_truncated();

void set_use_logical_path(bool use_logical);
void set_truncate_to_repo(bool truncate);
void set_truncation_length(int length);
void set_home_symbol(const std::string& symbol);
void add_substitution(const std::string& from, const std::string& to);

}  
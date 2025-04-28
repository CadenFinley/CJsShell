#pragma once

#include <string>
#include <filesystem>
#include <chrono>
#include <mutex>
#include <cstdlib>
#include <pwd.h>
#include <unistd.h>
#include "theme.h"

class Prompt {
  private:
    std::chrono::steady_clock::time_point last_git_status_check = std::chrono::steady_clock::now() - std::chrono::seconds(30);
    std::string cached_git_dir;
    std::string cached_status_symbols;
    bool cached_is_clean_repo = true;
    std::mutex git_status_mutex;
    bool is_git_status_check_running = false;
    
    bool is_root_path(const std::filesystem::path& path);
    std::string get_current_file_name();
    std::string get_current_file_path();
    std::string replace_placeholder(const std::string& format, const std::string& placeholder, const std::string& value);
    std::string get_current_time();
    std::string get_current_date();
    std::string get_shell();
    std::string get_shell_version();
    
    // Modified to emphasize format_str is mainly for terminal title
    bool is_variable_used(const std::string& var_name, const std::string& format_str, const std::vector<nlohmann::json>& segments = {});
    
  public:
    Prompt();
    ~Prompt();
    std::string get_prompt();
    std::string get_ai_prompt();
    std::string get_title_prompt();
};
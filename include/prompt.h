#pragma once
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <regex>
#include <chrono>
#include <mutex>

/**
 * @brief Returns the formatted shell prompt string for the user.
 *
 * The prompt includes styling, current directory, and Git status information as applicable.
 * The format and content may vary based on internal settings such as whether to display the full path.
 *
 * @return std::string The generated prompt string to display to the user.
 */
class Prompt {
  private:
    const std::string SHELL_COLOR = "\033[1;31m";
    const std::string RESET_COLOR = "\033[0m";
    const std::string DIRECTORY_COLOR = "\033[1;34m";
    const std::string BRANCH_COLOR = "\033[1;33m";
    const std::string GIT_COLOR = "\033[1;32m";
    std::string terminal_name = "cjsh";
    bool display_whole_path = false;
    
    // Git status caching
    std::chrono::steady_clock::time_point last_git_status_check = std::chrono::steady_clock::now() - std::chrono::seconds(30);
    std::string cached_git_dir;
    std::string cached_status_symbols;
    bool cached_is_clean_repo = true;
    std::mutex git_status_mutex;
    bool is_git_status_check_running = false;
    
    bool is_root_path(const std::filesystem::path& path);
    std::string get_current_file_name();
    std::string get_current_file_path();
    
  public:
    Prompt();
    ~Prompt();
    std::string get_prompt();
    std::string get_ai_prompt();
};
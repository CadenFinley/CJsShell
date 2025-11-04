#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

#include "prompt_info.h"

class Theme;

enum class PromptType : std::uint8_t {
    PS1,
    GIT,
    NEWLINE,
    INLINE_RIGHT,
    TITLE,
    ALL
};

class Prompt {
   private:
    PromptInfo info;
    std::filesystem::path repo_root;
    Theme* theme_ = nullptr;
    std::string replace_placeholder(const std::string& format, const std::string& placeholder,
                                    const std::string& value);
    std::unordered_map<std::string, std::string> get_variables(PromptType type,
                                                               bool is_git_repo = false);

   public:
    Prompt();
    ~Prompt();
    std::string get_prompt();
    std::string get_title_prompt();
    std::string get_newline_prompt();
    std::string get_inline_right_prompt();
    bool is_git_repository(std::filesystem::path& repo_root);

    std::string get_initial_duration();

    void start_command_timing();
    void end_command_timing(int exit_code);
    void reset_command_timing();
    void set_initial_duration(long long microseconds);

    void set_theme(Theme* theme);
    void clear_cached_state();
};

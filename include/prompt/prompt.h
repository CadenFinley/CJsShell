#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include "prompt_info.h"
#include "theme.h"

enum class PromptType {
    PS1,
    GIT,
    AI,
    NEWLINE,
    INLINE_RIGHT,
    TITLE,
    ALL
};

class Prompt {
   private:
    PromptInfo info;
    std::filesystem::path repo_root;
    std::string replace_placeholder(const std::string& format,
                                    const std::string& placeholder,
                                    const std::string& value);
    std::unordered_map<std::string, std::string> get_variables(
        PromptType type, bool is_git_repo = false);

   public:
    Prompt();
    ~Prompt();
    std::string get_prompt();
    std::string get_ai_prompt();
    std::string get_title_prompt();
    std::string get_newline_prompt();
    std::string get_inline_right_prompt();
    bool is_git_repository(std::filesystem::path& repo_root);

    // Command timing access
    void start_command_timing() {
        info.start_command_timing();
    }
    void end_command_timing(int exit_code) {
        info.end_command_timing(exit_code);
    }
    void reset_command_timing() {
        info.reset_command_timing();
    }
    void set_initial_duration(long long microseconds) {
        info.set_initial_duration(microseconds);
    }
};

#pragma once
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <vector>

enum class PromptItems {
    USERNAME,
    HOSTNAME,
    PATH,
    DIRECTORY,
    TIME,
    DATE,
    SHELL,
    GIT_BRANCH,
    GIT_STATUS,
    REPO_NAME
};

enum class AIPromptItems {
    AI_DIVIDER,
    AI_MODEL,
    AI_AGENT_TYPE
};

enum class TerminalTitleItems {
    SHELL,
    USERNAME,
    HOSTNAME,
    PATH,
    DIRECTORY
};

class Theme {
  public:
    Theme(std::string theme_directory);
    ~Theme();
    bool load_theme(const std::string& theme_name);
    std::vector<std::string> list_themes();

    std::string get_ps1_prompt_format();
    std::string get_git_prompt_format();
    std::string get_ai_prompt_format();
    std::string get_terminal_title_format();
  private:
    std::string theme_directory;
    std::string ps1_format;
    std::string git_format;
    std::string ai_format;
    std::string terminal_title_format;
    
    std::string process_color_tags(const std::string& format);
    void create_default_theme();
};

//example ps1 prompt
// [RED] {USERNAME}[WHITE]@[GREEN]{HOSTNAME} [WHITE]: [BLUE]{PATH} [WHITE]$ [RESET]

//example git prompt
// [RED] {USERNAME} [BLUE]{DIRECTORY} [GREEN]{REPO_NAME} git:([YELLOW]{GIT_BRANCH} {GIT_STATUS}(GREEN)) [RESET]
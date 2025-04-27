#pragma once
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <vector>

enum class Colors {
    // Basic colors
    BLACK,
    RED,
    GREEN,
    YELLOW,
    BLUE,
    MAGENTA,
    CYAN,
    WHITE,
    
    // Bright variants
    BLACK_BRIGHT,
    RED_BRIGHT,
    GREEN_BRIGHT,
    YELLOW_BRIGHT,
    BLUE_BRIGHT,
    MAGENTA_BRIGHT,
    CYAN_BRIGHT,
    WHITE_BRIGHT,
    
    // Bold variants
    BLACK_BOLD,
    RED_BOLD,
    GREEN_BOLD,
    YELLOW_BOLD,
    BLUE_BOLD,
    MAGENTA_BOLD,
    CYAN_BOLD,
    WHITE_BOLD,
    
    // Underline variants
    BLACK_UNDERLINE,
    RED_UNDERLINE,
    GREEN_UNDERLINE,
    YELLOW_UNDERLINE,
    BLUE_UNDERLINE,
    MAGENTA_UNDERLINE,
    CYAN_UNDERLINE,
    WHITE_UNDERLINE,
    
    // Dark variants
    BLACK_DARK,
    RED_DARK,
    GREEN_DARK,
    YELLOW_DARK,
    BLUE_DARK,
    MAGENTA_DARK,
    CYAN_DARK,
    
    // Background colors
    BG_BLACK,
    BG_RED,
    BG_GREEN,
    BG_YELLOW,
    BG_BLUE,
    BG_MAGENTA,
    BG_CYAN,
    BG_WHITE,
    
    // Bright background colors
    BG_BLACK_BRIGHT,
    BG_RED_BRIGHT,
    BG_GREEN_BRIGHT,
    BG_YELLOW_BRIGHT,
    BG_BLUE_BRIGHT,
    BG_MAGENTA_BRIGHT,
    BG_CYAN_BRIGHT,
    BG_WHITE_BRIGHT,
    
    // Special formatting
    BOLD,
    ITALIC,
    UNDERLINE,
    BLINK,
    REVERSE,
    HIDDEN,
    RESET
};

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

class Theme {
  public:
    Theme(std::string theme_directory);
    ~Theme();
    bool load_theme(const std::string& theme_name);
    std::vector<std::string> list_themes();

    std::string get_ps1_prompt_format();
    std::string get_git_prompt_format();
    std::string get_ai_prompt_format();
  private:
    std::string theme_directory;
    std::string ps1_format;
    std::string git_format;
    std::string ai_format;
    
    std::string process_color_tags(const std::string& format);
    void create_default_theme();
};

//example ps1 prompt
// [RED] {USERNAME}[WHITE]@[GREEN]{HOSTNAME} [WHITE]: [BLUE]{PATH} [WHITE]$ [RESET]

//example git prompt
// [RED] {USERNAME} [BLUE]{DIRECTORY} [GREEN]{REPO_NAME} git:([YELLOW]{GIT_BRANCH} {GIT_STATUS}(GREEN)) [RESET]
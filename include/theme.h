#pragma once
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

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
    HIDDEN
};

enum class PromptItems {
    USERNAME,
    HOSTNAME,
    PATH,
    DIRECTORY,
    TIME,
    DATE,
    SHELL
};

enum class GitPromptItems {
    GIT_BRANCH,
    GIT_STATUS
};

enum class AIPromptItems {
    AI_DIVIDER,
    AI_MODEL,
    AI_AGENT_TYPE
};

class Theme {
private:
    std::string m_theme_name;
    nlohmann::json m_theme_data;
    
    // Prompt storage
    std::string m_ps1_prompt;
    std::string m_git_prompt;
    std::string m_ai_prompt;
    
    // Color mappings
    std::unordered_map<PromptItems, Colors> m_ps1_colors;
    std::unordered_map<GitPromptItems, Colors> m_git_colors;
    std::unordered_map<AIPromptItems, Colors> m_ai_colors;
    
    // Build individual prompts
    void build_ps1_prompt();
    void build_git_prompt();
    void build_ai_prompt();

    Colors parse_color_string(const std::string& color_str);
    
    // Create default theme file if it doesn't exist
    static void ensure_default_theme_exists();

public:
    Theme();
    ~Theme() = default;
    
    // Load a theme from a JSON file
    bool load_theme(const std::string& theme_name);
    
    // Get the built prompts
    std::string get_ps1_prompt() const;
    std::string get_git_prompt() const;
    std::string get_ai_prompt() const;
    
    // Get the theme name
    std::string get_theme_name() const;
    
    // Get color for specific elements
    std::string get_color(Colors color) const;
    
    // List available themes
    static std::vector<std::string> list_available_themes();
};
#include "theme.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include "cjsh_filesystem.h"

// ANSI color codes map
const std::unordered_map<Colors, std::string> DEFAULT_COLORS = {
    // Basic colors
    {Colors::BLACK, "\033[30m"},
    {Colors::RED, "\033[31m"},
    {Colors::GREEN, "\033[32m"},
    {Colors::YELLOW, "\033[33m"},
    {Colors::BLUE, "\033[34m"},
    {Colors::MAGENTA, "\033[35m"},
    {Colors::CYAN, "\033[36m"},
    {Colors::WHITE, "\033[37m"},
    
    // Bright variants
    {Colors::BLACK_BRIGHT, "\033[90m"},
    {Colors::RED_BRIGHT, "\033[91m"},
    {Colors::GREEN_BRIGHT, "\033[92m"},
    {Colors::YELLOW_BRIGHT, "\033[93m"},
    {Colors::BLUE_BRIGHT, "\033[94m"},
    {Colors::MAGENTA_BRIGHT, "\033[95m"},
    {Colors::CYAN_BRIGHT, "\033[96m"},
    {Colors::WHITE_BRIGHT, "\033[97m"},
    
    // Bold variants
    {Colors::BLACK_BOLD, "\033[1;30m"},
    {Colors::RED_BOLD, "\033[1;31m"},
    {Colors::GREEN_BOLD, "\033[1;32m"},
    {Colors::YELLOW_BOLD, "\033[1;33m"},
    {Colors::BLUE_BOLD, "\033[1;34m"},
    {Colors::MAGENTA_BOLD, "\033[1;35m"},
    {Colors::CYAN_BOLD, "\033[1;36m"},
    {Colors::WHITE_BOLD, "\033[1;37m"},
    
    // Underline variants
    {Colors::BLACK_UNDERLINE, "\033[4;30m"},
    {Colors::RED_UNDERLINE, "\033[4;31m"},
    {Colors::GREEN_UNDERLINE, "\033[4;32m"},
    {Colors::YELLOW_UNDERLINE, "\033[4;33m"},
    {Colors::BLUE_UNDERLINE, "\033[4;34m"},
    {Colors::MAGENTA_UNDERLINE, "\033[4;35m"},
    {Colors::CYAN_UNDERLINE, "\033[4;36m"},
    {Colors::WHITE_UNDERLINE, "\033[4;37m"},
    
    // Dark variants
    {Colors::BLACK_DARK, "\033[38;5;0m"},
    {Colors::RED_DARK, "\033[38;5;52m"},
    {Colors::GREEN_DARK, "\033[38;5;22m"},
    {Colors::YELLOW_DARK, "\033[38;5;58m"},
    {Colors::BLUE_DARK, "\033[38;5;17m"},
    {Colors::MAGENTA_DARK, "\033[38;5;53m"},
    {Colors::CYAN_DARK, "\033[38;5;23m"},
    
    // Background colors
    {Colors::BG_BLACK, "\033[40m"},
    {Colors::BG_RED, "\033[41m"},
    {Colors::BG_GREEN, "\033[42m"},
    {Colors::BG_YELLOW, "\033[43m"},
    {Colors::BG_BLUE, "\033[44m"},
    {Colors::BG_MAGENTA, "\033[45m"},
    {Colors::BG_CYAN, "\033[46m"},
    {Colors::BG_WHITE, "\033[47m"},
    
    // Bright background colors
    {Colors::BG_BLACK_BRIGHT, "\033[100m"},
    {Colors::BG_RED_BRIGHT, "\033[101m"},
    {Colors::BG_GREEN_BRIGHT, "\033[102m"},
    {Colors::BG_YELLOW_BRIGHT, "\033[103m"},
    {Colors::BG_BLUE_BRIGHT, "\033[104m"},
    {Colors::BG_MAGENTA_BRIGHT, "\033[105m"},
    {Colors::BG_CYAN_BRIGHT, "\033[106m"},
    {Colors::BG_WHITE_BRIGHT, "\033[107m"},
    
    // Special formatting
    {Colors::BOLD, "\033[1m"},
    {Colors::ITALIC, "\033[3m"},
    {Colors::UNDERLINE, "\033[4m"},
    {Colors::BLINK, "\033[5m"},
    {Colors::REVERSE, "\033[7m"},
    {Colors::HIDDEN, "\033[8m"}
};

// Reset code for terminal
const std::string RESET = "\033[0m";

Theme::Theme() : m_theme_name("default") {
    // Ensure default theme exists before proceeding
    ensure_default_theme_exists();
    
    // Initialize with default prompts
    m_ps1_prompt = "\033[1;32m\\u@\\h\033[0m:\033[1;34m\\w\033[0m$ ";
    m_git_prompt = "";
    m_ai_prompt = "";
}

bool Theme::load_theme(const std::string& theme_name) {
    m_theme_name = theme_name;
    std::filesystem::path theme_file = cjsh_filesystem::g_cjsh_theme_path / (theme_name + ".json");
    
    if (!std::filesystem::exists(theme_file)) {
        std::cerr << "Theme file not found: " << theme_file << std::endl;
        return false;
    }
    
    try {
        std::ifstream file(theme_file);
        file >> m_theme_data;
        
        // Parse the colors from the theme data
        if (m_theme_data.contains("ps1_colors")) {
            auto ps1_colors = m_theme_data["ps1_colors"];
            for (auto& [key, value] : ps1_colors.items()) {
                // Convert string key to PromptItems enum
                PromptItems item;
                if (key == "username") item = PromptItems::USERNAME;
                else if (key == "hostname") item = PromptItems::HOSTNAME;
                else if (key == "path") item = PromptItems::PATH;
                else if (key == "directory") item = PromptItems::DIRECTORY;
                else if (key == "time") item = PromptItems::TIME;
                else if (key == "date") item = PromptItems::DATE;
                else if (key == "shell") item = PromptItems::SHELL;
                else continue;
                
                // Convert string value to Colors enum
                Colors color = parse_color_string(value);
                m_ps1_colors[item] = color;
            }
        }
        
        if (m_theme_data.contains("git_colors")) {
            auto git_colors = m_theme_data["git_colors"];
            for (auto& [key, value] : git_colors.items()) {
                GitPromptItems item;
                if (key == "branch") item = GitPromptItems::GIT_BRANCH;
                else if (key == "status") item = GitPromptItems::GIT_STATUS;
                else continue;
                
                Colors color = parse_color_string(value);
                m_git_colors[item] = color;
            }
        }
        
        if (m_theme_data.contains("ai_colors")) {
            auto ai_colors = m_theme_data["ai_colors"];
            for (auto& [key, value] : ai_colors.items()) {
                AIPromptItems item;
                if (key == "divider") item = AIPromptItems::AI_DIVIDER;
                else if (key == "model") item = AIPromptItems::AI_MODEL;
                else if (key == "agent_type") item = AIPromptItems::AI_AGENT_TYPE;
                else continue;
                
                Colors color = parse_color_string(value);
                m_ai_colors[item] = color;
            }
        }
        
        // Build all prompts
        build_ps1_prompt();
        build_git_prompt();
        build_ai_prompt();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading theme: " << e.what() << std::endl;
        return false;
    }
}

void Theme::build_ps1_prompt() {
    if (!m_theme_data.contains("ps1_format")) {
        return;
    }
    
    std::string format = m_theme_data["ps1_format"];
    // Replace placeholders with actual values and colors
    // This is just a placeholder implementation
    m_ps1_prompt = format;
    
    // Replace tokens with appropriate colors and reset codes
    // In a real implementation, you would replace tokens like {username} with 
    // the actual username and its color from m_ps1_colors
}

void Theme::build_git_prompt() {
    if (!m_theme_data.contains("git_format")) {
        return;
    }
    
    std::string format = m_theme_data["git_format"];
    m_git_prompt = format;
    // Similar implementation to build_ps1_prompt but for git
}

void Theme::build_ai_prompt() {
    if (!m_theme_data.contains("ai_format")) {
        return;
    }
    
    std::string format = m_theme_data["ai_format"];
    m_ai_prompt = format;
    // Similar implementation to build_ps1_prompt but for AI
}

std::string Theme::get_ps1_prompt() const {
    return m_ps1_prompt;
}

std::string Theme::get_git_prompt() const {
    return m_git_prompt;
}

std::string Theme::get_ai_prompt() const {
    return m_ai_prompt;
}

std::string Theme::get_theme_name() const {
    return m_theme_name;
}

std::string Theme::get_color(Colors color) const {
    auto it = DEFAULT_COLORS.find(color);
    if (it != DEFAULT_COLORS.end()) {
        return it->second;
    }
    return "";
}

void Theme::ensure_default_theme_exists() {
    std::filesystem::path default_theme_path = cjsh_filesystem::g_cjsh_theme_path / "default.json";
    
    // Check if the themes directory exists, create if not
    if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_theme_path)) {
        std::filesystem::create_directories(cjsh_filesystem::g_cjsh_theme_path);
    }
    
    // If the default theme doesn't exist, create it
    if (!std::filesystem::exists(default_theme_path)) {
        nlohmann::json default_theme;
        
        // PS1 prompt settings
        default_theme["ps1_format"] = "\\[\\033[1;32m\\]\\u@\\h\\[\\033[0m\\]:\\[\\033[1;34m\\]\\w\\[\\033[0m\\]$ ";
        default_theme["ps1_colors"] = {
            {"username", "green_bold"},
            {"hostname", "green_bold"},
            {"path", "blue_bold"},
            {"directory", "blue_bold"},
            {"time", "white"},
            {"date", "white"},
            {"shell", "white"}
        };
        
        // Git prompt settings
        default_theme["git_format"] = "[\\[\\033[1;36m\\]branch: %s\\[\\033[0m\\]]";
        default_theme["git_colors"] = {
            {"branch", "cyan_bold"},
            {"status", "yellow_bold"}
        };
        
        // AI prompt settings
        default_theme["ai_format"] = "\\[\\033[1;35m\\][AI: %s]\\[\\033[0m\\] ";
        default_theme["ai_colors"] = {
            {"divider", "magenta_bold"},
            {"model", "magenta_bold"},
            {"agent_type", "cyan_bold"}
        };
        
        // Write to file
        std::ofstream file(default_theme_path);
        file << default_theme.dump(4); // Pretty print with 4-space indent
        file.close();
    }
}

// Helper function to parse color string to Colors enum
Colors Theme::parse_color_string(const std::string& color_str) {
    // Convert string to Colors enum
    if (color_str == "black") return Colors::BLACK;
    if (color_str == "red") return Colors::RED;
    if (color_str == "green") return Colors::GREEN;
    if (color_str == "yellow") return Colors::YELLOW;
    if (color_str == "blue") return Colors::BLUE;
    if (color_str == "magenta") return Colors::MAGENTA;
    if (color_str == "cyan") return Colors::CYAN;
    if (color_str == "white") return Colors::WHITE;
    
    // Add more color mappings as needed
    
    // Default
    return Colors::WHITE;
}

std::vector<std::string> Theme::list_available_themes() {
    std::vector<std::string> themes;
    
    for (const auto& entry : std::filesystem::directory_iterator(cjsh_filesystem::g_cjsh_theme_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            themes.push_back(entry.path().stem().string());
        }
    }
    
    return themes;
}


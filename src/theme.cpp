#include "theme.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include "colors.h"

Theme::Theme(std::string theme_dir, bool enabled) : theme_directory(theme_dir), is_enabled(enabled) {
    
    if (!std::filesystem::exists(theme_directory + "/default.json")) {
        create_default_theme();
    }
    
    // Load default theme by default
    if (is_enabled) {
        load_theme("default");
    } else {
        // Setup default formats WITHOUT color tags when themes are disabled
        ps1_format = "{USERNAME}@{HOSTNAME} {PATH} $";
        git_format = "{USERNAME} {DIRECTORY} git:({GIT_BRANCH} {GIT_STATUS})";
        ai_format = "{AI_MODEL} {AI_AGENT_TYPE} {AI_DIVIDER}";
        terminal_title_format = "{USERNAME}@{HOSTNAME}: {DIRECTORY}";
        
        // Process formats right away
        process_all_formats();
    }
}

Theme::~Theme() {
    // Cleanup if needed
}

void Theme::create_default_theme() {
    nlohmann::json default_theme;
    
    // Default PS1 prompt format
    default_theme["ps1_prompt"] = "[RED]{USERNAME}[WHITE]@[GREEN]{HOSTNAME} [BLUE]{PATH} [WHITE]$[RESET]";
    
    // Default Git prompt format
    default_theme["git_prompt"] = "[RED]{USERNAME} [BLUE]{DIRECTORY} [GREEN]git:([YELLOW]{GIT_BRANCH} {GIT_STATUS}[GREEN])[RESET]";
    
    // Default AI prompt format
    default_theme["ai_prompt"] = "[BLUE]{AI_MODEL} [YELLOW]{AI_AGENT_TYPE} [WHITE]{AI_DIVIDER}[RESET]";
    
    // Default Terminal Title format
    default_theme["terminal_title"] = "{SHELL} {USERNAME}@{HOSTNAME}: {DIRECTORY}";
    
    // Write to file
    std::ofstream file(theme_directory + "/default.json");
    file << default_theme.dump(4);
    file.close();
}

bool Theme::load_theme(const std::string& theme_name) {
    // If themes are disabled, only use default values
    if (!is_enabled) {
      std::cout << "Themes are disabled. Using default values." << std::endl;
      return true;
    }
    
    std::string theme_file = theme_directory + "/" + theme_name + ".json";
    
    if (!std::filesystem::exists(theme_file)) {
        return false;
    }
    
    std::ifstream file(theme_file);
    nlohmann::json theme_json;
    file >> theme_json;
    file.close();
    
    if (theme_json.contains("ps1_prompt")) {
        ps1_format = theme_json["ps1_prompt"];
    } else {
        ps1_format = "[RED]{USERNAME}[WHITE]@[GREEN]{HOSTNAME} [BLUE]{PATH} [WHITE]$[RESET]";
    }
    if (theme_json.contains("git_prompt")) {
        git_format = theme_json["git_prompt"];
    } else {
        git_format = "[RED]{USERNAME} [BLUE]{DIRECTORY} [GREEN]git:([YELLOW]{GIT_BRANCH} {GIT_STATUS}[GREEN])[RESET]";
    }
    if (theme_json.contains("ai_prompt")) {
        ai_format = theme_json["ai_prompt"];
    } else {
        ai_format = "[BLUE]{AI_MODEL} [YELLOW]{AI_AGENT_TYPE} [WHITE]{AI_DIVIDER}[RESET]";
    }
    if (theme_json.contains("terminal_title")) {
        terminal_title_format = theme_json["terminal_title"];
    } else {
        terminal_title_format = "{USERNAME}@{HOSTNAME}: {DIRECTORY}";
    }
    
    // Process all formats after loading the theme
    process_all_formats();
    
    return true;
}

std::vector<std::string> Theme::list_themes() {
    std::vector<std::string> themes;
    
    // If themes are disabled, return only the default theme
    if (!is_enabled) {
        themes.push_back("default");
        return themes;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(theme_directory)) {
        if (entry.path().extension() == ".json") {
            themes.push_back(entry.path().stem().string());
        }
    }
    return themes;
}

// New method to process all formats at once
void Theme::process_all_formats() {
  processed_ps1_format = process_color_tags(ps1_format);
  processed_git_format = process_color_tags(git_format);
  processed_ai_format = process_color_tags(ai_format);
}

// Simplify the getter methods to return pre-processed formats
std::string Theme::get_ps1_prompt_format() {
    return processed_ps1_format;
}

std::string Theme::get_git_prompt_format() {
    return processed_git_format;
}

std::string Theme::get_ai_prompt_format() {
    return processed_ai_format;
}

std::string Theme::get_terminal_title_format() {
    return terminal_title_format;
}

std::string Theme::process_color_tags(const std::string& format) {
    // If colors are disabled, remove color tags but ensure white text
    if (colors::g_color_capability == colors::ColorCapability::NO_COLOR) {
        // On terminals with dark backgrounds, white text is more visible
        std::string white_text = "\033[37m"; // Basic white text
        return white_text + remove_color_tags(format);
    }
    
    // Normal processing with colors
    std::string result = format;
    
    // Get the centralized color map
    std::unordered_map<std::string, std::string> color_map = colors::get_color_map();
    
    // Process the color tags
    size_t pos = 0;
    while ((pos = result.find('[', pos)) != std::string::npos) {
        size_t end_pos = result.find(']', pos);
        if (end_pos == std::string::npos) {
            break;  // No matching closing bracket
        }
        
        std::string color_name = result.substr(pos + 1, end_pos - pos - 1);
        if (color_map.find(color_name) != color_map.end()) {
            result.replace(pos, end_pos - pos + 1, color_map[color_name]);
        } else {
            // Skip this tag if color not found
            pos = end_pos + 1;
        }
    }
    
    return result;
}

std::string Theme::remove_color_tags(const std::string& format) {
    std::string result = format;
    
    // Remove all [COLOR] tags
    size_t pos = 0;
    while ((pos = result.find('[', pos)) != std::string::npos) {
        size_t end_pos = result.find(']', pos);
        if (end_pos == std::string::npos) {
            break;  // No matching closing bracket
        }
        
        result.erase(pos, end_pos - pos + 1);
    }
    
    return result;
}
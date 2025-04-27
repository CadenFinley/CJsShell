#include "theme.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include "colors.h"

Theme::Theme(std::string theme_dir) : theme_directory(theme_dir) {
    
    // Create default theme if it doesn't exist
    if (!std::filesystem::exists(theme_directory + "/default.json")) {
        create_default_theme();
    }
    
    // Load default theme by default
    load_theme("default");
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
    
    // Write to file
    std::ofstream file(theme_directory + "/default.json");
    file << default_theme.dump(4);  // Pretty print with 4 spaces
    file.close();
}

bool Theme::load_theme(const std::string& theme_name) {
    std::string theme_file = theme_directory + "/" + theme_name + ".json";
    
    if (!std::filesystem::exists(theme_file)) {
        std::cerr << "Theme '" << theme_name << "' not found. Using default theme." << std::endl;
        load_theme("default");
        return false;
    }
    
    std::ifstream file(theme_file);
    nlohmann::json theme_json;
    file >> theme_json;
    file.close();
    
    // Store the theme formats
    ps1_format = theme_json["ps1_prompt"];
    git_format = theme_json["git_prompt"];
    ai_format = theme_json["ai_prompt"];
    return true;
}

std::vector<std::string> Theme::list_themes() {
    std::vector<std::string> themes;
    for (const auto& entry : std::filesystem::directory_iterator(theme_directory)) {
        if (entry.path().extension() == ".json") {
            themes.push_back(entry.path().stem().string());
        }
    }
    return themes;
}

std::string Theme::get_ps1_prompt_format() {
    return process_color_tags(ps1_format);
}

std::string Theme::get_git_prompt_format() {
    return process_color_tags(git_format);
}

std::string Theme::get_ai_prompt_format() {
    return process_color_tags(ai_format);
}

std::string Theme::process_color_tags(const std::string& format) {
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
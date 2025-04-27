#include "theme.h"
#include <fstream>
#include <iostream>
#include <filesystem>

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
    std::unordered_map<std::string, std::string> color_map = {
        {"BLACK", "\033[30m"},
        {"RED", "\033[31m"},
        {"GREEN", "\033[32m"},
        {"YELLOW", "\033[33m"},
        {"BLUE", "\033[34m"},
        {"MAGENTA", "\033[35m"},
        {"CYAN", "\033[36m"},
        {"WHITE", "\033[37m"},
        {"BLACK_BRIGHT", "\033[90m"},
        {"RED_BRIGHT", "\033[91m"},
        {"GREEN_BRIGHT", "\033[92m"},
        {"YELLOW_BRIGHT", "\033[93m"},
        {"BLUE_BRIGHT", "\033[94m"},
        {"MAGENTA_BRIGHT", "\033[95m"},
        {"CYAN_BRIGHT", "\033[96m"},
        {"WHITE_BRIGHT", "\033[97m"},
        {"BLACK_BOLD", "\033[1;30m"},
        {"RED_BOLD", "\033[1;31m"},
        {"GREEN_BOLD", "\033[1;32m"},
        {"YELLOW_BOLD", "\033[1;33m"},
        {"BLUE_BOLD", "\033[1;34m"},
        {"MAGENTA_BOLD", "\033[1;35m"},
        {"CYAN_BOLD", "\033[1;36m"},
        {"WHITE_BOLD", "\033[1;37m"},
        {"BLACK_UNDERLINE", "\033[4;30m"},
        {"RED_UNDERLINE", "\033[4;31m"},
        {"GREEN_UNDERLINE", "\033[4;32m"},
        {"YELLOW_UNDERLINE", "\033[4;33m"},
        {"BLUE_UNDERLINE", "\033[4;34m"},
        {"MAGENTA_UNDERLINE", "\033[4;35m"},
        {"CYAN_UNDERLINE", "\033[4;36m"},
        {"WHITE_UNDERLINE", "\033[4;37m"},
        {"BLACK_DARK", "\033[2;30m"},
        {"RED_DARK", "\033[2;31m"},
        {"GREEN_DARK", "\033[2;32m"},
        {"YELLOW_DARK", "\033[2;33m"},
        {"BLUE_DARK", "\033[2;34m"},
        {"MAGENTA_DARK", "\033[2;35m"},
        {"CYAN_DARK", "\033[2;36m"},
        {"BG_BLACK", "\033[40m"},
        {"BG_RED", "\033[41m"},
        {"BG_GREEN", "\033[42m"},
        {"BG_YELLOW", "\033[43m"},
        {"BG_BLUE", "\033[44m"},
        {"BG_MAGENTA", "\033[45m"},
        {"BG_CYAN", "\033[46m"},
        {"BG_WHITE", "\033[47m"},
        {"BG_BLACK_BRIGHT", "\033[100m"},
        {"BG_RED_BRIGHT", "\033[101m"},
        {"BG_GREEN_BRIGHT", "\033[102m"},
        {"BG_YELLOW_BRIGHT", "\033[103m"},
        {"BG_BLUE_BRIGHT", "\033[104m"},
        {"BG_MAGENTA_BRIGHT", "\033[105m"},
        {"BG_CYAN_BRIGHT", "\033[106m"},
        {"BG_WHITE_BRIGHT", "\033[107m"},
        {"BOLD", "\033[1m"},
        {"ITALIC", "\033[3m"},
        {"UNDERLINE", "\033[4m"},
        {"BLINK", "\033[5m"},
        {"REVERSE", "\033[7m"},
        {"HIDDEN", "\033[8m"},
        {"RESET", "\033[0m"}
    };
    
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
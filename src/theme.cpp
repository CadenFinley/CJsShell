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
    
    // Basic ANSI colors
    std::unordered_map<std::string, std::string> color_map = {
        {"BLACK", colors::ansi::FG_BLACK},
        {"RED", colors::ansi::FG_RED},
        {"GREEN", colors::ansi::FG_GREEN},
        {"YELLOW", colors::ansi::FG_YELLOW},
        {"BLUE", colors::ansi::FG_BLUE},
        {"MAGENTA", colors::ansi::FG_MAGENTA},
        {"CYAN", colors::ansi::FG_CYAN},
        {"WHITE", colors::ansi::FG_WHITE},
        {"BLACK_BRIGHT", colors::ansi::FG_BRIGHT_BLACK},
        {"RED_BRIGHT", colors::ansi::FG_BRIGHT_RED},
        {"GREEN_BRIGHT", colors::ansi::FG_BRIGHT_GREEN},
        {"YELLOW_BRIGHT", colors::ansi::FG_BRIGHT_YELLOW},
        {"BLUE_BRIGHT", colors::ansi::FG_BRIGHT_BLUE},
        {"MAGENTA_BRIGHT", colors::ansi::FG_BRIGHT_MAGENTA},
        {"CYAN_BRIGHT", colors::ansi::FG_BRIGHT_CYAN},
        {"WHITE_BRIGHT", colors::ansi::FG_BRIGHT_WHITE},
        
        // Pastel color palette
        {"PASTEL_BLUE", "\033[38;5;117m"},
        {"PASTEL_PEACH", "\033[38;5;222m"},
        {"PASTEL_CYAN", "\033[38;5;159m"},
        {"PASTEL_MINT", "\033[38;5;122m"},
        {"PASTEL_LAVENDER", "\033[38;5;183m"},
        {"PASTEL_CORAL", "\033[38;5;203m"},
        
        // Named colors from colors.h
        {"ALICE_BLUE", colors::fg_color(colors::named::ALICE_BLUE)},
        {"ANTIQUE_WHITE", colors::fg_color(colors::named::ANTIQUE_WHITE)},
        {"AQUA", colors::fg_color(colors::named::AQUA)},
        {"AQUAMARINE", colors::fg_color(colors::named::AQUAMARINE)},
        {"AZURE", colors::fg_color(colors::named::AZURE)},
        {"BEIGE", colors::fg_color(colors::named::BEIGE)},
        {"BISQUE", colors::fg_color(colors::named::BISQUE)},
        {"BLANCHED_ALMOND", colors::fg_color(colors::named::BLANCHED_ALMOND)},
        {"BLUE_VIOLET", colors::fg_color(colors::named::BLUE_VIOLET)},
        {"BROWN", colors::fg_color(colors::named::BROWN)},
        {"BURLYWOOD", colors::fg_color(colors::named::BURLYWOOD)},
        {"CADET_BLUE", colors::fg_color(colors::named::CADET_BLUE)},
        {"CHARTREUSE", colors::fg_color(colors::named::CHARTREUSE)},
        {"CHOCOLATE", colors::fg_color(colors::named::CHOCOLATE)},
        {"CORAL", colors::fg_color(colors::named::CORAL)},
        {"CORNFLOWER_BLUE", colors::fg_color(colors::named::CORNFLOWER_BLUE)},
        {"CORNSILK", colors::fg_color(colors::named::CORNSILK)},
        {"CRIMSON", colors::fg_color(colors::named::CRIMSON)},
        {"DARK_BLUE", colors::fg_color(colors::named::DARK_BLUE)},
        {"DARK_CYAN", colors::fg_color(colors::named::DARK_CYAN)},
        {"DARK_GOLDENROD", colors::fg_color(colors::named::DARK_GOLDENROD)},
        {"DARK_GRAY", colors::fg_color(colors::named::DARK_GRAY)},
        {"DARK_GREEN", colors::fg_color(colors::named::DARK_GREEN)},
        {"DARK_KHAKI", colors::fg_color(colors::named::DARK_KHAKI)},
        {"DARK_MAGENTA", colors::fg_color(colors::named::DARK_MAGENTA)},
        {"DARK_OLIVE_GREEN", colors::fg_color(colors::named::DARK_OLIVE_GREEN)},
        {"DARK_ORANGE", colors::fg_color(colors::named::DARK_ORANGE)},
        {"DARK_ORCHID", colors::fg_color(colors::named::DARK_ORCHID)},
        {"DARK_RED", colors::fg_color(colors::named::DARK_RED)},
        {"DARK_SALMON", colors::fg_color(colors::named::DARK_SALMON)},
        {"DARK_SEA_GREEN", colors::fg_color(colors::named::DARK_SEA_GREEN)},
        {"DARK_SLATE_BLUE", colors::fg_color(colors::named::DARK_SLATE_BLUE)},
        {"DARK_SLATE_GRAY", colors::fg_color(colors::named::DARK_SLATE_GRAY)},
        {"DARK_TURQUOISE", colors::fg_color(colors::named::DARK_TURQUOISE)},
        {"DARK_VIOLET", colors::fg_color(colors::named::DARK_VIOLET)},
        {"DEEP_PINK", colors::fg_color(colors::named::DEEP_PINK)},
        {"DEEP_SKY_BLUE", colors::fg_color(colors::named::DEEP_SKY_BLUE)},
        {"DIM_GRAY", colors::fg_color(colors::named::DIM_GRAY)},
        {"DODGER_BLUE", colors::fg_color(colors::named::DODGER_BLUE)},
        {"FIREBRICK", colors::fg_color(colors::named::FIREBRICK)},
        {"FOREST_GREEN", colors::fg_color(colors::named::FOREST_GREEN)},
        {"GAINSBORO", colors::fg_color(colors::named::GAINSBORO)},
        {"GOLD", colors::fg_color(colors::named::GOLD)},
        {"GOLDENROD", colors::fg_color(colors::named::GOLDENROD)},
        {"GRAY", colors::fg_color(colors::named::GRAY)},
        {"HOT_PINK", colors::fg_color(colors::named::HOT_PINK)},
        {"INDIAN_RED", colors::fg_color(colors::named::INDIAN_RED)},
        {"INDIGO", colors::fg_color(colors::named::INDIGO)},
        {"IVORY", colors::fg_color(colors::named::IVORY)},
        {"KHAKI", colors::fg_color(colors::named::KHAKI)},
        {"LAVENDER", colors::fg_color(colors::named::LAVENDER)},
        {"LAWN_GREEN", colors::fg_color(colors::named::LAWN_GREEN)},
        {"LIGHT_BLUE", colors::fg_color(colors::named::LIGHT_BLUE)},
        {"LIGHT_CORAL", colors::fg_color(colors::named::LIGHT_CORAL)},
        {"LIGHT_CYAN", colors::fg_color(colors::named::LIGHT_CYAN)},
        {"LIGHT_GOLDENROD", colors::fg_color(colors::named::LIGHT_GOLDENROD)},
        {"LIGHT_GRAY", colors::fg_color(colors::named::LIGHT_GRAY)},
        {"LIGHT_GREEN", colors::fg_color(colors::named::LIGHT_GREEN)},
        {"LIGHT_PINK", colors::fg_color(colors::named::LIGHT_PINK)},
        {"LIGHT_SALMON", colors::fg_color(colors::named::LIGHT_SALMON)},
        {"LIGHT_SEA_GREEN", colors::fg_color(colors::named::LIGHT_SEA_GREEN)},
        {"LIGHT_SKY_BLUE", colors::fg_color(colors::named::LIGHT_SKY_BLUE)},
        {"LIGHT_SLATE_GRAY", colors::fg_color(colors::named::LIGHT_SLATE_GRAY)},
        {"LIGHT_STEEL_BLUE", colors::fg_color(colors::named::LIGHT_STEEL_BLUE)},
        {"LIGHT_YELLOW", colors::fg_color(colors::named::LIGHT_YELLOW)},
        {"LIME", colors::fg_color(colors::named::LIME)},
        {"LIME_GREEN", colors::fg_color(colors::named::LIME_GREEN)},
        {"LINEN", colors::fg_color(colors::named::LINEN)},
        {"MAROON", colors::fg_color(colors::named::MAROON)},
        {"MEDIUM_AQUAMARINE", colors::fg_color(colors::named::MEDIUM_AQUAMARINE)},
        {"MEDIUM_BLUE", colors::fg_color(colors::named::MEDIUM_BLUE)},
        {"MEDIUM_ORCHID", colors::fg_color(colors::named::MEDIUM_ORCHID)},
        {"MEDIUM_PURPLE", colors::fg_color(colors::named::MEDIUM_PURPLE)},
        {"MEDIUM_SEA_GREEN", colors::fg_color(colors::named::MEDIUM_SEA_GREEN)},
        {"MEDIUM_SLATE_BLUE", colors::fg_color(colors::named::MEDIUM_SLATE_BLUE)},
        {"MEDIUM_SPRING_GREEN", colors::fg_color(colors::named::MEDIUM_SPRING_GREEN)},
        {"MEDIUM_TURQUOISE", colors::fg_color(colors::named::MEDIUM_TURQUOISE)},
        {"MEDIUM_VIOLET_RED", colors::fg_color(colors::named::MEDIUM_VIOLET_RED)},
        {"MIDNIGHT_BLUE", colors::fg_color(colors::named::MIDNIGHT_BLUE)},
        {"MINT_CREAM", colors::fg_color(colors::named::MINT_CREAM)},
        {"MISTY_ROSE", colors::fg_color(colors::named::MISTY_ROSE)},
        {"MOCCASIN", colors::fg_color(colors::named::MOCCASIN)},
        {"NAVAJO_WHITE", colors::fg_color(colors::named::NAVAJO_WHITE)},
        {"NAVY", colors::fg_color(colors::named::NAVY)},
        {"OLD_LACE", colors::fg_color(colors::named::OLD_LACE)},
        {"OLIVE", colors::fg_color(colors::named::OLIVE)},
        {"OLIVE_DRAB", colors::fg_color(colors::named::OLIVE_DRAB)},
        {"ORANGE", colors::fg_color(colors::named::ORANGE)},
        {"ORANGE_RED", colors::fg_color(colors::named::ORANGE_RED)},
        {"ORCHID", colors::fg_color(colors::named::ORCHID)},
        {"PALE_GOLDENROD", colors::fg_color(colors::named::PALE_GOLDENROD)},
        {"PALE_GREEN", colors::fg_color(colors::named::PALE_GREEN)},
        {"PALE_TURQUOISE", colors::fg_color(colors::named::PALE_TURQUOISE)},
        {"PALE_VIOLET_RED", colors::fg_color(colors::named::PALE_VIOLET_RED)},
        {"PAPAYA_WHIP", colors::fg_color(colors::named::PAPAYA_WHIP)},
        {"PEACH_PUFF", colors::fg_color(colors::named::PEACH_PUFF)},
        {"PERU", colors::fg_color(colors::named::PERU)},
        {"PINK", colors::fg_color(colors::named::PINK)},
        {"PLUM", colors::fg_color(colors::named::PLUM)},
        {"MEDIUM_TURQUOISE", colors::fg_color(colors::named::MEDIUM_TURQUOISE)},
        {"MEDIUM_VIOLET_RED", colors::fg_color(colors::named::MEDIUM_VIOLET_RED)},
        {"MIDNIGHT_BLUE", colors::fg_color(colors::named::MIDNIGHT_BLUE)},
        {"MINT_CREAM", colors::fg_color(colors::named::MINT_CREAM)},
        {"ROYAL_BLUE", colors::fg_color(colors::named::ROYAL_BLUE)},
        {"SADDLE_BROWN", colors::fg_color(colors::named::SADDLE_BROWN)},
        {"SALMON", colors::fg_color(colors::named::SALMON)},
        {"SANDY_BROWN", colors::fg_color(colors::named::SANDY_BROWN)},
        {"SEA_GREEN", colors::fg_color(colors::named::SEA_GREEN)},
        {"SEASHELL", colors::fg_color(colors::named::SEASHELL)},
        {"SIENNA", colors::fg_color(colors::named::SIENNA)},
        {"SILVER", colors::fg_color(colors::named::SILVER)},
        {"SKY_BLUE", colors::fg_color(colors::named::SKY_BLUE)},
        {"SLATE_BLUE", colors::fg_color(colors::named::SLATE_BLUE)},
        {"SLATE_GRAY", colors::fg_color(colors::named::SLATE_GRAY)},
        {"SNOW", colors::fg_color(colors::named::SNOW)},
        {"SPRING_GREEN", colors::fg_color(colors::named::SPRING_GREEN)},
        {"STEEL_BLUE", colors::fg_color(colors::named::STEEL_BLUE)},
        {"TAN", colors::fg_color(colors::named::TAN)},
        {"TEAL", colors::fg_color(colors::named::TEAL)},
        {"THISTLE", colors::fg_color(colors::named::THISTLE)},
        {"TOMATO", colors::fg_color(colors::named::TOMATO)},
        {"TURQUOISE", colors::fg_color(colors::named::TURQUOISE)},
        {"VIOLET", colors::fg_color(colors::named::VIOLET)},
        {"WHEAT", colors::fg_color(colors::named::WHEAT)},
        {"WHITE_SMOKE", colors::fg_color(colors::named::WHITE_SMOKE)},
        {"YELLOW_GREEN", colors::fg_color(colors::named::YELLOW_GREEN)},
        {"ELECTRIC_PURPLE", colors::fg_color(colors::named::ELECTRIC_PURPLE)},

        // Style tags
        {"BOLD", colors::ansi::BOLD},
        {"ITALIC", colors::ansi::ITALIC},
        {"UNDERLINE", colors::ansi::UNDERLINE},
        {"BLINK", colors::ansi::BLINK},
        {"REVERSE", colors::ansi::REVERSE},
        {"HIDDEN", colors::ansi::HIDDEN},
        {"RESET", colors::ansi::RESET}
    };
    
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
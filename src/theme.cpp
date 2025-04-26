#include "theme.h"

namespace {
    /**
     * @brief Converts all occurrences of the string literal "\033" in the input to the ASCII escape character.
     *
     * @param input The input string potentially containing "\033" sequences.
     * @return std::string The input string with all "\033" sequences replaced by the escape character (0x1B).
     */
    std::string parse_ansi_codes(const std::string &input) {
        std::string output = input;
        std::string pattern = "\\033";
        std::string replacement(1, '\x1B');
        size_t pos = 0;
        while ((pos = output.find(pattern, pos)) != std::string::npos) {
            output.replace(pos, pattern.length(), replacement);
            pos += replacement.length();
        }
        return output;
    }
}

/**
 * @brief Initializes the Theme manager with a specified themes directory.
 *
 * Sets the directory for theme storage, ensures a default theme exists, discovers all available themes in the directory, and loads the default theme as the current theme.
 *
 * @param themes_dir Path to the directory containing theme JSON files.
 */
Theme::Theme(const std::filesystem::path& themes_dir) {
    themes_directory = themes_dir;
    create_default_theme();
    discover_available_themes();
    load_theme("default");
}

/**
 * @brief Destroys the Theme object.
 *
 * Default destructor. No special cleanup is performed.
 */
Theme::~Theme() {
    
}

/**
 * @brief Creates and saves the default theme with predefined color keys and prompt format.
 *
 * Initializes a default theme with empty color values and a standard prompt format, saves it as "default", and adds it to the available themes.
 */
void Theme::create_default_theme() {
    std::map<std::string, std::string> default_colors = {
        {"GREEN_COLOR_BOLD", ""},
        {"RED_COLOR_BOLD", ""},
        {"PURPLE_COLOR_BOLD", ""},
        {"BLUE_COLOR_BOLD", ""},
        {"YELLOW_COLOR_BOLD", ""},
        {"CYAN_COLOR_BOLD", ""},
        {"SHELL_COLOR", ""},
        {"DIRECTORY_COLOR", ""},
        {"BRANCH_COLOR", ""},
        {"GIT_COLOR", ""},
        {"RESET_COLOR", ""},
        {"PROMPT_FORMAT", "cjsh \\w"}
    };
    
    save_theme("default", default_colors);
    available_themes["default"] = default_colors;
}

/**
 * @brief Scans the themes directory for JSON files and loads available themes into memory.
 *
 * Clears the current list of available themes, ensures the default theme exists, and then iterates through all JSON files in the themes directory. Each valid theme file is parsed, ANSI escape codes are normalized, and the resulting theme is added to the available themes map. Errors during loading are logged to standard error.
 */
void Theme::discover_available_themes() {
    available_themes.clear();
    
    if (available_themes.find("default") == available_themes.end()) {
        create_default_theme();
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(themes_directory)) {
        if (entry.path().extension() == ".json") {
            std::string theme_name = entry.path().stem().string();
            try {
                std::ifstream theme_file(entry.path());
                json theme_data;
                theme_file >> theme_data;
                
                std::map<std::string, std::string> theme_colors;
                for (auto& [key, value] : theme_data.items()) {
                    if (value.is_string()) {
                        theme_colors[key] = parse_ansi_codes(value.get<std::string>());
                    }
                }
                
                available_themes[theme_name] = theme_colors;
                
            } catch (const std::exception& e) {
                std::cerr << "Error loading theme " << theme_name << ": " << e.what() << std::endl;
            }
        }
    }
}

/**
 * @brief Loads a theme by name from the available themes or from disk.
 *
 * If the theme is already loaded, sets it as the current theme. Otherwise, attempts to read and parse the theme's JSON file from the themes directory, normalizing any ANSI escape sequences. On success, updates the current theme and caches it. Returns true if the theme was successfully loaded, false if the theme does not exist or an error occurs during loading.
 *
 * @param theme_name Name of the theme to load.
 * @return true if the theme was loaded successfully, false otherwise.
 */
bool Theme::load_theme(const std::string& theme_name) {
    if (available_themes.find(theme_name) != available_themes.end()) {
        current_theme_name = theme_name;
        current_theme_colors = available_themes[theme_name];
        return true;
    }
    
    std::filesystem::path theme_path = themes_directory / (theme_name + ".json");
    if (std::filesystem::exists(theme_path)) {
        try {
            std::ifstream theme_file(theme_path);
            json theme_data;
            theme_file >> theme_data;
            
            std::map<std::string, std::string> theme_colors;
            for (auto& [key, value] : theme_data.items()) {
                if (value.is_string()) {
                    theme_colors[key] = parse_ansi_codes(value.get<std::string>());
                }
            }
            
            available_themes[theme_name] = theme_colors;
            current_theme_name = theme_name;
            current_theme_colors = theme_colors;
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Error loading theme " << theme_name << ": " << e.what() << std::endl;
            return false;
        }
    }
    
    std::cerr << "Theme " << theme_name << " not found." << std::endl;
    return false;
}

/**
 * @brief Saves a theme's color definitions to a JSON file in the themes directory.
 *
 * @param theme_name Name of the theme to save.
 * @param colors Map of color keys to their string values for the theme.
 * @return true if the theme was saved successfully, false if an error occurred.
 */
bool Theme::save_theme(const std::string& theme_name, const std::map<std::string, std::string>& colors) {
    try {
        std::filesystem::path theme_path = themes_directory / (theme_name + ".json");
        std::ofstream theme_file(theme_path);
        
        json theme_data;
        for (const auto& [key, value] : colors) {
            theme_data[key] = value;
        }
        
        theme_file << theme_data.dump(4);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving theme " << theme_name << ": " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Deletes a theme by name from the themes directory.
 *
 * Prevents deletion of the "default" theme. Removes the theme file and its entry from the available themes. If the deleted theme is currently loaded, reverts to the default theme.
 *
 * @param theme_name Name of the theme to delete.
 * @return true if the theme was successfully deleted; false if the theme does not exist or is "default".
 */
bool Theme::delete_theme(const std::string& theme_name) {
    if (theme_name == "default") {
        std::cerr << "Cannot delete default theme." << std::endl;
        return false;
    }
    
    std::filesystem::path theme_path = themes_directory / (theme_name + ".json");
    if (std::filesystem::exists(theme_path)) {
        std::filesystem::remove(theme_path);
        available_themes.erase(theme_name);
        
        if (current_theme_name == theme_name) {
            load_theme("default");
        }
        
        return true;
    }
    
    return false;
}

/**
 * @brief Returns the names of all available themes.
 *
 * @return A vector containing the names of themes currently loaded in memory.
 */
std::vector<std::string> Theme::get_available_theme_names() const {
    std::vector<std::string> theme_names;
    for (const auto& [name, _] : available_themes) {
        theme_names.push_back(name);
    }
    return theme_names;
}

/**
 * @brief Retrieves the color value for a given color name from the current theme.
 *
 * If the specified color name is not found, returns the value for "RESET_COLOR" if available; otherwise, returns an empty string.
 *
 * @param color_name The key representing the desired color.
 * @return The color value as a string, or a fallback/reset value, or an empty string if not found.
 */
std::string Theme::get_color(const std::string& color_name) const {
    if (current_theme_colors.find(color_name) != current_theme_colors.end()) {
        return current_theme_colors.at(color_name);
    }
    
    if (current_theme_colors.find("RESET_COLOR") != current_theme_colors.end()) {
        return current_theme_colors.at("RESET_COLOR");
    }
    return "";
}

/**
 * @brief Sets or updates the color value for a specified color key in the current theme.
 *
 * @param color_name The name of the color key to set.
 * @param color_value The color value to assign to the key.
 */
void Theme::set_color(const std::string& color_name, const std::string& color_value) {
    current_theme_colors[color_name] = color_value;
}

#include "theme.h"

namespace {
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

Theme::Theme(const std::filesystem::path& themes_dir) {
    themes_directory = themes_dir;
    create_default_theme();
    discover_available_themes();
    load_theme("default");
}

Theme::~Theme() {
    
}

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

std::vector<std::string> Theme::get_available_theme_names() const {
    std::vector<std::string> theme_names;
    for (const auto& [name, _] : available_themes) {
        theme_names.push_back(name);
    }
    return theme_names;
}

std::string Theme::get_color(const std::string& color_name) const {
    if (current_theme_colors.find(color_name) != current_theme_colors.end()) {
        return current_theme_colors.at(color_name);
    }
    
    if (current_theme_colors.find("RESET_COLOR") != current_theme_colors.end()) {
        return current_theme_colors.at("RESET_COLOR");
    }
    return "";
}

void Theme::set_color(const std::string& color_name, const std::string& color_value) {
    current_theme_colors[color_name] = color_value;
}

#include "thememanager.h"

namespace {
    std::string parseAnsiCodes(const std::string &input) {
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

ThemeManager::ThemeManager(const std::filesystem::path& themesDir) : themesDirectory(themesDir), currentThemeName("default") {
    if (!std::filesystem::exists(themesDirectory)) {
        std::filesystem::create_directory(themesDirectory);
    }
    
    createDefaultTheme();
    discoverAvailableThemes();
    loadTheme("default");
}

ThemeManager::~ThemeManager() {
    // No dynamic memory to clean up
}

void ThemeManager::createDefaultTheme() {
    std::map<std::string, std::string> defaultColors = {
        {"GREEN_COLOR_BOLD", "\033[1;32m"},
        {"RESET_COLOR", "\033[0m"},
        {"RED_COLOR_BOLD", "\033[1;31m"},
        {"PURPLE_COLOR_BOLD", "\033[1;35m"},
        {"BLUE_COLOR_BOLD", "\033[1;34m"},
        {"YELLOW_COLOR_BOLD", "\033[1;33m"},
        {"CYAN_COLOR_BOLD", "\033[1;36m"},
        {"TERMINAL_TEXT", "\033[0;37m"},
        {"COMMAND_PROMPT", "\033[1;32m"},
        {"ERROR_TEXT", "\033[1;31m"},
        {"SUCCESS_TEXT", "\033[1;32m"},
        {"HIGHLIGHT_TEXT", "\033[1;36m"}
    };
    
    saveTheme("default", defaultColors);
    availableThemes["default"] = defaultColors;
}

void ThemeManager::discoverAvailableThemes() {
    availableThemes.clear();
    
    if (availableThemes.find("default") == availableThemes.end()) {
        createDefaultTheme();
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(themesDirectory)) {
        if (entry.path().extension() == ".json") {
            std::string themeName = entry.path().stem().string();
            try {
                std::ifstream themeFile(entry.path());
                json themeData;
                themeFile >> themeData;
                
                std::map<std::string, std::string> themeColors;
                for (auto& [key, value] : themeData.items()) {
                    if (value.is_string()) {
                        themeColors[key] = parseAnsiCodes(value.get<std::string>());
                    }
                }
                
                availableThemes[themeName] = themeColors;
                
            } catch (const std::exception& e) {
                std::cerr << "Error loading theme " << themeName << ": " << e.what() << std::endl;
            }
        }
    }
    std::vector<std::string> themeNames = getAvailableThemeNames();
    if (!themeNames.empty()) {
        std::cout << "Available themes: ";
        for (const auto& [name, _] : availableThemes) {
            if(name != "default"){
                std::cout << name << ", ";
            }
        }
        std::cout << std::endl;
    }
}

bool ThemeManager::loadTheme(const std::string& themeName) {
    if (availableThemes.find(themeName) != availableThemes.end()) {
        currentThemeName = themeName;
        currentThemeColors = availableThemes[themeName];
        return true;
    }
    
    std::filesystem::path themePath = themesDirectory / (themeName + ".json");
    if (std::filesystem::exists(themePath)) {
        try {
            std::ifstream themeFile(themePath);
            json themeData;
            themeFile >> themeData;
            
            std::map<std::string, std::string> themeColors;
            for (auto& [key, value] : themeData.items()) {
                if (value.is_string()) {
                    themeColors[key] = parseAnsiCodes(value.get<std::string>());
                }
            }
            
            availableThemes[themeName] = themeColors;
            currentThemeName = themeName;
            currentThemeColors = themeColors;
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Error loading theme " << themeName << ": " << e.what() << std::endl;
            return false;
        }
    }
    
    std::cerr << "Theme " << themeName << " not found." << std::endl;
    return false;
}

bool ThemeManager::saveTheme(const std::string& themeName, const std::map<std::string, std::string>& colors) {
    try {
        std::filesystem::path themePath = themesDirectory / (themeName + ".json");
        std::ofstream themeFile(themePath);
        
        json themeData;
        for (const auto& [key, value] : colors) {
            themeData[key] = value;
        }
        
        themeFile << themeData.dump(4);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving theme " << themeName << ": " << e.what() << std::endl;
        return false;
    }
}

bool ThemeManager::deleteTheme(const std::string& themeName) {
    if (themeName == "default") {
        std::cerr << "Cannot delete default theme." << std::endl;
        return false;
    }
    
    std::filesystem::path themePath = themesDirectory / (themeName + ".json");
    if (std::filesystem::exists(themePath)) {
        std::filesystem::remove(themePath);
        availableThemes.erase(themeName);
        
        if (currentThemeName == themeName) {
            loadTheme("default");
        }
        
        return true;
    }
    
    return false;
}

std::vector<std::string> ThemeManager::getAvailableThemeNames() const {
    std::vector<std::string> themeNames;
    for (const auto& [name, _] : availableThemes) {
        themeNames.push_back(name);
    }
    return themeNames;
}

std::string ThemeManager::getColor(const std::string& colorName) const {
    if (currentThemeColors.find(colorName) != currentThemeColors.end()) {
        return currentThemeColors.at(colorName);
    }
    
    if (currentThemeColors.find("RESET_COLOR") != currentThemeColors.end()) {
        return currentThemeColors.at("RESET_COLOR");
    }
    
    return "\033[0m";
}

void ThemeManager::setColor(const std::string& colorName, const std::string& colorValue) {
    currentThemeColors[colorName] = colorValue;
}

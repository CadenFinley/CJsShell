#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <string>
#include <map>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <iostream>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

class ThemeManager {
private:
    std::string currentThemeName;
    std::filesystem::path themesDirectory;
    std::map<std::string, std::map<std::string, std::string>> availableThemes;
    std::map<std::string, std::string> currentThemeColors;

    void createDefaultTheme();

public:
    ThemeManager(const std::filesystem::path& themesDir);
    ~ThemeManager();

    // Theme management
    void discoverAvailableThemes();
    bool loadTheme(const std::string& themeName);
    bool saveTheme(const std::string& themeName, const std::map<std::string, std::string>& colors);
    bool deleteTheme(const std::string& themeName);
    
    // Getters
    std::string getCurrentThemeName() const { return currentThemeName; }
    std::map<std::string, std::map<std::string, std::string>> getAvailableThemes() const { return availableThemes; }
    std::vector<std::string> getAvailableThemeNames() const;
    std::string getColor(const std::string& colorName) const;
    
    // Setters
    void setColor(const std::string& colorName, const std::string& colorValue);
};

#endif // THEMEMANAGER_H

#pragma once

#include <string>
#include <map>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Theme {
private:
    std::string currentThemeName;
    std::filesystem::path themesDirectory;
    std::map<std::string, std::map<std::string, std::string>> availableThemes;
    std::map<std::string, std::string> currentThemeColors;

    void createDefaultTheme();

public:
    Theme(const std::filesystem::path& themesDir);
    ~Theme();

    void discoverAvailableThemes();
    bool loadTheme(const std::string& themeName);
    bool saveTheme(const std::string& themeName, const std::map<std::string, std::string>& colors);
    bool deleteTheme(const std::string& themeName);
    
    std::string getCurrentThemeName() const { return currentThemeName; }
    std::map<std::string, std::map<std::string, std::string>> getAvailableThemes() const { return availableThemes; }
    std::vector<std::string> getAvailableThemeNames() const;
    std::string getColor(const std::string& colorName) const;
    
    void setColor(const std::string& colorName, const std::string& colorValue);
};



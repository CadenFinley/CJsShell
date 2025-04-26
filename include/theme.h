#pragma once

#include <string>
#include <map>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

using json = nlohmann::json;

class Theme {
private:
    std::string current_theme_name;
    std::filesystem::path themes_directory;
    std::map<std::string, std::map<std::string, std::string>> available_themes;
    std::map<std::string, std::string> current_theme_colors;

    void create_default_theme();

public:
    Theme(const std::filesystem::path& themes_dir);
    ~Theme();

    void discover_available_themes();
    bool load_theme(const std::string& theme_name);
    bool save_theme(const std::string& theme_name, const std::map<std::string, std::string>& colors);
    bool delete_theme(const std::string& theme_name);
    
    std::string get_current_theme_name() const { return current_theme_name; }
    std::map<std::string, std::map<std::string, std::string>> get_available_themes() const { return available_themes; }
    std::vector<std::string> get_available_theme_names() const;
    std::string get_color(const std::string& color_name) const;
    
    void set_color(const std::string& color_name, const std::string& color_value);
};



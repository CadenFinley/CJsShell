#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

class Theme {
private:
    std::string theme_directory;
    bool is_enabled;
    
    std::string ps1_format;
    std::string git_format;
    std::string ai_format;
    std::string newline_format;
    std::string terminal_title_format;
    
    std::string processed_ps1_format;
    std::string processed_git_format;
    std::string processed_ai_format;
    std::string processed_newline_format;
    
    // Newline setting
    bool use_newline;
    
    void process_all_formats();
    std::string replace_placeholders(const std::string& format, const std::unordered_map<std::string, std::string>& vars);
    
public:
    std::vector<nlohmann::json> ps1_segments;
    std::vector<nlohmann::json> git_segments;
    std::vector<nlohmann::json> ai_segments;
    std::vector<nlohmann::json> newline_segments;
    
    Theme(std::string theme_dir, bool enabled);
    ~Theme();
    
    void create_default_theme();
    bool load_theme(const std::string& theme_name);
    std::vector<std::string> list_themes();
    
    std::string get_ps1_prompt_format(const std::unordered_map<std::string, std::string>& vars = {});
    std::string get_git_prompt_format(const std::unordered_map<std::string, std::string>& vars = {});
    std::string get_ai_prompt_format(const std::unordered_map<std::string, std::string>& vars = {});
    std::string get_newline_prompt_format(const std::unordered_map<std::string, std::string>& vars = {});
    std::string get_terminal_title_format(const std::unordered_map<std::string, std::string>& vars = {});
    
    std::string render_ps1_segments(const std::unordered_map<std::string, std::string>& vars);
    std::string render_git_segments(const std::unordered_map<std::string, std::string>& vars);
    std::string render_ai_segments(const std::unordered_map<std::string, std::string>& vars);
    std::string render_newline_segments(const std::unordered_map<std::string, std::string>& vars);
    
    std::string process_color_tags(const std::string& format);
    std::string remove_color_tags(const std::string& format);
    
    std::string render_segment(const nlohmann::json& segment, const std::unordered_map<std::string, std::string>& vars);
    
    bool is_segmented_style() const;
    bool uses_newline() const;
    std::string get_newline_prompt() const;
    bool has_newline_segments() const;
};
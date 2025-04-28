#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

class Theme {
private:
    std::string theme_directory;
    bool is_enabled;
    void create_default_theme();
    
    std::string terminal_title_format;

    void prerender_segments();
    std::string prerender_line(const std::vector<nlohmann::json>& segments) const; // will render full line with placeholders

    std::string prerendered_ps1_format;
    std::string prerendered_git_format;
    std::string prerendered_ai_format;
    std::string prerendered_newline_format;

    std::string render_line(const std::string& line, const std::unordered_map<std::string, std::string>& vars) const;
    
    std::string execute_script(const std::string& script_path) const;
    
public:
    
    Theme(std::string theme_dir, bool enabled);
    ~Theme();

    std::vector<nlohmann::json> ps1_segments;
    std::vector<nlohmann::json> git_segments;
    std::vector<nlohmann::json> ai_segments;
    std::vector<nlohmann::json> newline_segments;
    
    bool load_theme(const std::string& theme_name);
    std::vector<std::string> list_themes();
    
    bool uses_newline() const;

    std::string get_terminal_title_format() const;

    std::string get_rendered_ps1_format() const {
        return prerendered_ps1_format;
    }
    std::string get_rendered_git_format() const {
        return prerendered_git_format;
    }
    std::string get_rendered_ai_format() const {
        return prerendered_ai_format;
    }
    std::string get_rendered_newline_format() const {
        return prerendered_newline_format;
    }

    std::string get_newline_prompt(const std::unordered_map<std::string, std::string>& vars) const;
    std::string get_ps1_prompt_format(const std::unordered_map<std::string, std::string>& vars) const;
    std::string get_git_prompt_format(const std::unordered_map<std::string, std::string>& vars) const;
    std::string get_ai_prompt_format(const std::unordered_map<std::string, std::string>& vars) const;
};
#include "theme.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include "colors.h"

Theme::Theme(std::string theme_dir, bool enabled) : theme_directory(theme_dir), is_enabled(enabled) {
    if (!std::filesystem::exists(theme_directory + "/default.json")) {
        create_default_theme();
    }
    
    load_theme("default");
}

Theme::~Theme() {
}

void Theme::create_default_theme() {
    nlohmann::json default_theme;
    
    default_theme["terminal_title"] = "{SHELL} {USERNAME}@{HOSTNAME}: {DIRECTORY}";
    default_theme["ps1_segments"] = nlohmann::json::array();
    default_theme["ps1_segments"].push_back({
        {"tag", "userseg"},
        {"content", " {USERNAME}@{HOSTNAME} "},
        {"bg_color", "P10K_USER_BG"},
        {"fg_color", "P10K_USER_FG"},
        {"separator", "\uE0B0"},
        {"separator_fg", "P10K_USER_BG"},
        {"separator_bg", "P10K_DIR_BG"}
    });
    default_theme["ps1_segments"].push_back({
        {"tag", "pathseg"},
        {"content", " {PATH} "},
        {"bg_color", "P10K_DIR_BG"},
        {"fg_color", "P10K_DIR_FG"},
        {"separator", "\uE0B0"},
        {"separator_fg", "P10K_DIR_BG"},
        {"separator_bg", "RESET"}
    });
    default_theme["git_segments"] = nlohmann::json::array();
    default_theme["git_segments"].push_back({
        {"tag", "userseg"},
        {"content", " {USERNAME} "},
        {"bg_color", "P10K_USER_BG"},
        {"fg_color", "P10K_USER_FG"},
        {"separator", "\uE0B0"},
        {"separator_fg", "P10K_USER_BG"},
        {"separator_bg", "P10K_DIR_BG"}
    });
    default_theme["git_segments"].push_back({
        {"tag", "directoryseg"},
        {"content", " {DIRECTORY} "},
        {"bg_color", "P10K_DIR_BG"},
        {"fg_color", "P10K_DIR_FG"},
        {"separator", "\uE0B0"},
        {"separator_fg", "P10K_DIR_BG"},
        {"separator_bg", "P10K_GIT_STATUS_BG"}
    });
    default_theme["git_segments"].push_back({
        {"tag", "gitbranchseg"},
        {"content", " {GIT_BRANCH}"},
        {"bg_color", "P10K_GIT_STATUS_BG"},
        {"fg_color", "P10K_GIT_STATUS_FG"},
        {"separator", ""}
    });
    default_theme["git_segments"].push_back({
        {"tag", "gitstatusseg"},
        {"content", "{GIT_STATUS} "},
        {"bg_color", "P10K_GIT_STATUS_BG"},
        {"fg_color", "P10K_GIT_STATUS_FG"},
        {"separator", "\uE0B0"},
        {"separator_fg", "P10K_GIT_STATUS_BG"},
        {"separator_bg", "RESET"}
    });
    default_theme["ai_segments"] = nlohmann::json::array();
    default_theme["ai_segments"].push_back({
        {"tag", "aimodelseg"},
        {"content", " {AI_MODEL} "},
        {"bg_color", "P10K_AI_MODEL_BG"},
        {"fg_color", "P10K_AI_MODEL_FG"},
        {"separator", "\uE0B0"},
        {"separator_fg", "P10K_AI_MODEL_BG"},
        {"separator_bg", "P10K_AI_AGENT_BG"}
    });
    default_theme["ai_segments"].push_back({
        {"tag", "aiagentseg"},
        {"content", " {AI_AGENT_TYPE} "},
        {"bg_color", "P10K_AI_AGENT_BG"},
        {"fg_color", "P10K_AI_AGENT_FG"},
        {"separator", "\uE0B0"},
        {"separator_fg", "P10K_AI_AGENT_BG"},
        {"separator_bg", "RESET"}
    });
    default_theme["ai_segments"].push_back({
        {"tag", "aidivseg"},
        {"content", " {AI_DIVIDER} "},
        {"bg_color", "RESET"},
        {"fg_color", "P10K_DIVIDER_FG"},
        {"separator", ""}
    });
    
    std::ofstream file(theme_directory + "/default.json");
    file << default_theme.dump(4);
    file.close();
}

bool Theme::load_theme(const std::string& theme_name) {
    if (!is_enabled) {
        return false;
    }
    std::string theme_file = theme_directory + "/" + theme_name + ".json";
    
    if (!std::filesystem::exists(theme_file)) {
        return false;
    }
    
    std::ifstream file(theme_file);
    nlohmann::json theme_json;
    file >> theme_json;
    file.close();

    ps1_segments.clear();
    git_segments.clear();
    ai_segments.clear();
    newline_segments.clear();
    
    if (theme_json.contains("ps1_segments") && theme_json["ps1_segments"].is_array()) {
        ps1_segments = theme_json["ps1_segments"];
    }
    
    if (theme_json.contains("git_segments") && theme_json["git_segments"].is_array()) {
        git_segments = theme_json["git_segments"];
    }
    
    if (theme_json.contains("ai_segments") && theme_json["ai_segments"].is_array()) {
        ai_segments = theme_json["ai_segments"];
    }
    
    if (theme_json.contains("newline_segments") && theme_json["newline_segments"].is_array()) {
        newline_segments = theme_json["newline_segments"];
    }

    if (theme_json.contains("terminal_title")) {
        terminal_title_format = theme_json["terminal_title"];
    }

    prerender_segments();
    
    return true;
}

std::string Theme::prerender_line(const std::vector<nlohmann::json>& segments) const {
    if (!is_enabled || segments.empty()) {
        return "";
    }
    
    std::string result;
    auto color_map = colors::get_color_map();
    
    for (const auto& segment : segments) {
        std::string segment_result;
        std::string content = segment.value("content", "");
        std::string bg_color_name = segment.value("bg_color", "RESET");
        std::string fg_color_name = segment.value("fg_color", "RESET");
        std::string separator = segment.value("separator", "");
        std::string separator_fg_name = segment.value("separator_fg", "RESET");
        std::string separator_bg_name = segment.value("separator_bg", "RESET");
        
        if (segment.contains("forward_separator") && !segment["forward_separator"].empty()) {
            std::string forward_separator = segment["forward_separator"];
            std::string forward_separator_fg_name = segment.value("forward_separator_fg", "RESET");
            std::string forward_separator_bg_name = segment.value("forward_separator_bg", "RESET");
            
            if (forward_separator_bg_name != "RESET") {
                auto fw_sep_bg_rgb = colors::get_color_by_name(forward_separator_bg_name);
                segment_result += colors::bg_color(fw_sep_bg_rgb);
            } else {
                segment_result += colors::ansi::BG_RESET;
            }
            
            if (forward_separator_fg_name != "RESET") {
                auto fw_sep_fg_rgb = colors::get_color_by_name(forward_separator_fg_name);
                segment_result += colors::fg_color(fw_sep_fg_rgb);
            } else if (bg_color_name != "RESET") {
                auto bg_rgb = colors::get_color_by_name(bg_color_name);
                segment_result += colors::fg_color(bg_rgb);
            }
            
            segment_result += forward_separator;
        }
        
        if (bg_color_name != "RESET") {
            auto bg_rgb = colors::get_color_by_name(bg_color_name);
            segment_result += colors::bg_color(bg_rgb);
        } else {
            segment_result += colors::ansi::BG_RESET;
        }
        
        if (fg_color_name != "RESET") {
            auto fg_rgb = colors::get_color_by_name(fg_color_name);
            segment_result += colors::fg_color(fg_rgb);
        }
        
        segment_result += content;
        
        if (!separator.empty()) {
            if (separator_fg_name != "RESET") {
                auto sep_fg_rgb = colors::get_color_by_name(separator_fg_name);
                segment_result += colors::fg_color(sep_fg_rgb);
            } else if (bg_color_name != "RESET") {
                auto bg_rgb = colors::get_color_by_name(bg_color_name);
                segment_result += colors::fg_color(bg_rgb);
            }
            
            if (separator_bg_name != "RESET") {
                auto sep_bg_rgb = colors::get_color_by_name(separator_bg_name);
                segment_result += colors::bg_color(sep_bg_rgb);
            } else {
                segment_result += colors::ansi::BG_RESET;
            }
            
            segment_result += separator;
        }
        
        result += segment_result;
    }
    
    result += colors::ansi::RESET;
    return result;
}

void Theme::prerender_segments() {
    prerendered_ps1_format = prerender_line(ps1_segments);
    prerendered_git_format = prerender_line(git_segments);
    prerendered_ai_format = prerender_line(ai_segments);
    prerendered_newline_format = prerender_line(newline_segments);
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

bool Theme::uses_newline() const {
    return !newline_segments.empty();
}

std::string Theme::get_terminal_title_format() const {
    return terminal_title_format;
}

std::string Theme::get_newline_prompt(const std::unordered_map<std::string, std::string>& vars) const {
    return render_line(prerendered_newline_format, vars);
}

std::string Theme::get_ps1_prompt_format(const std::unordered_map<std::string, std::string>& vars) const {
    return render_line(prerendered_ps1_format, vars);
}

std::string Theme::get_git_prompt_format(const std::unordered_map<std::string, std::string>& vars) const {
    return render_line(prerendered_git_format, vars);
}

std::string Theme::get_ai_prompt_format(const std::unordered_map<std::string, std::string>& vars) const {
    return render_line(prerendered_ai_format, vars);
}

std::string Theme::render_line(const std::string& line, const std::unordered_map<std::string, std::string>& vars) const {
    if (line.empty()) {
        return "";
    }
    
    std::string result = line;
    size_t start_pos = 0;

    while ((start_pos = result.find('{', start_pos)) != std::string::npos) {
        size_t end_pos = result.find('}', start_pos);
        if (end_pos == std::string::npos) {
            break;
        }
        
        std::string placeholder = result.substr(start_pos + 1, end_pos - start_pos - 1);
        
        auto it = vars.find(placeholder);
        if (it != vars.end()) {
            result.replace(start_pos, end_pos - start_pos + 1, it->second);
            start_pos += it->second.length();
        } else {
            start_pos = end_pos + 1;
        }
    }
    
    return result;
}
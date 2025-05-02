#include "theme.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <unordered_set>
#include "colors.h"

//add the ability to allign the segments to the left or right

Theme::Theme(std::string theme_dir, bool enabled) : theme_directory(theme_dir), is_enabled(enabled) {
    if (!std::filesystem::exists(theme_directory + "/default.json")) {
        create_default_theme();
    }
    is_enabled = enabled;
    
    load_theme("default");
}

Theme::~Theme() {
}

void Theme::create_default_theme() {
    nlohmann::json default_theme;
    
    default_theme["terminal_title"] = "{USERNAME}@{HOSTNAME}: {DIRECTORY}";
    default_theme["ps1_segments"] = nlohmann::json::array();
    default_theme["ps1_segments"].push_back({
        {"tag", "basicseg"},
        {"content", "{USERNAME}@{HOSTNAME}:{DIRECTORY}$ "},
        {"bg_color", "RESET"},
        {"fg_color", "WHITE_BRIGHT"},
        {"separator", ""},
        {"separator_fg", "RESET"},
        {"separator_bg", "RESET"}
    });
    
    default_theme["git_segments"] = nlohmann::json::array();
    default_theme["git_segments"].push_back({
        {"tag", "basicseg"},
        {"content", "{DIRECTORY} [{GIT_BRANCH}{GIT_STATUS}]$ "},
        {"bg_color", "RESET"},
        {"fg_color", "WHITE_BRIGHT"},
        {"separator", ""},
        {"separator_fg", "RESET"},
        {"separator_bg", "RESET"}
    });
    
    default_theme["ai_segments"] = nlohmann::json::array();
    default_theme["ai_segments"].push_back({
        {"tag", "aiseg"},
        {"content", "[{AI_MODEL}] "},
        {"bg_color", "RESET"},
        {"fg_color", "WHITE_BRIGHT"},
        {"separator", ""},
        {"separator_fg", "RESET"},
        {"separator_bg", "RESET"}
    });
    
    std::ofstream file(theme_directory + "/default.json");
    file << default_theme.dump(4);
    file.close();
}

bool Theme::load_theme(const std::string& theme_name) {
    std::string theme_name_to_use = theme_name;
    if (!is_enabled) {
        theme_name_to_use = "default";
    }

    std::string theme_file = theme_directory + "/" + theme_name_to_use + ".json";
    
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

    auto has_duplicate_tags = [](const std::vector<nlohmann::json>& segs) {
        std::unordered_set<std::string> seen;
        for (const auto& s : segs) {
            std::string tag = s.value("tag", "");
            if (!seen.insert(tag).second) {
                return true;
            }
        }
        return false;
    };

    if (has_duplicate_tags(ps1_segments) ||
        has_duplicate_tags(git_segments) ||
        has_duplicate_tags(ai_segments) ||
        has_duplicate_tags(newline_segments)) {
        return false;
    }

    if (theme_json.contains("terminal_title")) {
        terminal_title_format = theme_json["terminal_title"];
    }

    prerender_segments();
    
    return true;
}

std::string Theme::prerender_line(const std::vector<nlohmann::json>& segments) const {
    if (segments.empty()) {
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
                auto fw_sep_bg_rgb = colors::parse_color_value(forward_separator_bg_name);
                segment_result += colors::bg_color(fw_sep_bg_rgb);
            } else {
                segment_result += colors::ansi::BG_RESET;
            }
            
            if (forward_separator_fg_name != "RESET") {
                auto fw_sep_fg_rgb = colors::parse_color_value(forward_separator_fg_name);
                segment_result += colors::fg_color(fw_sep_fg_rgb);
            }
            
            segment_result += forward_separator;
        }
        
        if (bg_color_name != "RESET") {
            auto bg_rgb = colors::parse_color_value(bg_color_name);
            segment_result += colors::bg_color(bg_rgb);
        } else {
            segment_result += colors::ansi::BG_RESET;
        }
        
        if (fg_color_name != "RESET") {
            auto fg_rgb = colors::parse_color_value(fg_color_name);
            segment_result += colors::fg_color(fg_rgb);
        }
        
        segment_result += content;
        
        if (!separator.empty()) {
            if (separator_fg_name != "RESET") {
                auto sep_fg_rgb = colors::parse_color_value(separator_fg_name);
                segment_result += colors::fg_color(sep_fg_rgb);
            }
            
            if (separator_bg_name != "RESET") {
                auto sep_bg_rgb = colors::parse_color_value(separator_bg_name);
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

std::string Theme::execute_script(const std::string& script_path) const {
    static std::unordered_map<std::string, std::pair<std::string, std::time_t>> script_cache;
    static const int CACHE_EXPIRY_SECONDS = 5;
    auto now = std::time(nullptr);
    auto cache_it = script_cache.find(script_path);
    if (cache_it != script_cache.end()) {
        if (now - cache_it->second.second < CACHE_EXPIRY_SECONDS) {
            return cache_it->second.first;
        }
    }
    
    std::string result;
    // Use bash explicitly to ensure proper escape sequence interpretation
    std::string command = "bash -c \"" + script_path + "\"";
    FILE* pipe = popen(command.c_str(), "r");
    
    if (!pipe) {
        return "Error executing script: " + script_path;
    }
    
    char buffer[4096];
    while (!feof(pipe)) {
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
    }
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    
    pclose(pipe);
    script_cache[script_path] = {result, now};
    
    return result;
}

void Theme::clear_script_cache() {
    static std::unordered_map<std::string, std::pair<std::string, std::time_t>> script_cache;
    script_cache.clear();
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
        if (placeholder.compare(0, 7, "SCRIPT$") == 0 && placeholder.length() > 7) {
            std::string script_path = placeholder.substr(7);
            std::string script_output = execute_script(script_path);
            result.replace(start_pos, end_pos - start_pos + 1, script_output);
            start_pos += script_output.length();
        } else {
            auto it = vars.find(placeholder);
            if (it != vars.end()) {
                result.replace(start_pos, end_pos - start_pos + 1, it->second);
                start_pos += it->second.length();
            } else {
                start_pos = end_pos + 1;
            }
        }
    }
    
    return result;
}
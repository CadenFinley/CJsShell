#include "theme.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include "colors.h"

Theme::Theme(std::string theme_dir, bool enabled) : theme_directory(theme_dir), is_enabled(enabled), use_newline(false) {
    
    if (!std::filesystem::exists(theme_directory + "/default.json")) {
        create_default_theme();
    }
    
    if (is_enabled) {
        load_theme("default");
    } else {
        terminal_title_format = "{USERNAME}@{HOSTNAME}: {DIRECTORY}";
        use_newline = false;
        newline_format = " > ";
    }
}

Theme::~Theme() {
}

void Theme::create_default_theme() {
    nlohmann::json default_theme;
    
    default_theme["terminal_title"] = "{SHELL} {USERNAME}@{HOSTNAME}: {DIRECTORY}";
    default_theme["use_newline"] = false;
    
    // Create PS1 segments
    nlohmann::json ps1_segments = nlohmann::json::array();
    
    nlohmann::json username_segment;
    username_segment["type"] = "text";
    username_segment["content"] = " {USERNAME}";
    username_segment["bg_color"] = "RESET";
    username_segment["fg_color"] = "RED";
    username_segment["separator"] = "";
    ps1_segments.push_back(username_segment);
    
    nlohmann::json at_segment;
    at_segment["type"] = "text";
    at_segment["content"] = "@";
    at_segment["bg_color"] = "";
    at_segment["fg_color"] = "WHITE";
    at_segment["separator"] = "";
    ps1_segments.push_back(at_segment);
    
    nlohmann::json hostname_segment;
    hostname_segment["type"] = "text";
    hostname_segment["content"] = "{HOSTNAME} ";
    hostname_segment["bg_color"] = "";
    hostname_segment["fg_color"] = "GREEN";
    hostname_segment["separator"] = "";
    ps1_segments.push_back(hostname_segment);
    
    nlohmann::json path_segment;
    path_segment["type"] = "text";
    path_segment["content"] = "{PATH} ";
    path_segment["bg_color"] = "";
    path_segment["fg_color"] = "BLUE";
    path_segment["separator"] = "";
    ps1_segments.push_back(path_segment);
    
    nlohmann::json prompt_segment;
    prompt_segment["type"] = "text";
    prompt_segment["content"] = "$";
    prompt_segment["bg_color"] = "";
    prompt_segment["fg_color"] = "WHITE";
    prompt_segment["separator"] = "";
    ps1_segments.push_back(prompt_segment);
    
    default_theme["ps1_segments"] = ps1_segments;
    
    // Create Git segments
    nlohmann::json git_segments = nlohmann::json::array();
    
    nlohmann::json git_username_segment;
    git_username_segment["type"] = "text";
    git_username_segment["content"] = " {USERNAME} ";
    git_username_segment["bg_color"] = "RESET";
    git_username_segment["fg_color"] = "RED";
    git_username_segment["separator"] = "";
    git_segments.push_back(git_username_segment);
    
    nlohmann::json git_dir_segment;
    git_dir_segment["type"] = "text";
    git_dir_segment["content"] = "{DIRECTORY} ";
    git_dir_segment["bg_color"] = "";
    git_dir_segment["fg_color"] = "BLUE";
    git_dir_segment["separator"] = "";
    git_segments.push_back(git_dir_segment);
    
    nlohmann::json git_prefix_segment;
    git_prefix_segment["type"] = "text";
    git_prefix_segment["content"] = "git:(";
    git_prefix_segment["bg_color"] = "";
    git_prefix_segment["fg_color"] = "GREEN";
    git_prefix_segment["separator"] = "";
    git_segments.push_back(git_prefix_segment);
    
    nlohmann::json git_branch_segment;
    git_branch_segment["type"] = "text";
    git_branch_segment["content"] = "{GIT_BRANCH}";
    git_branch_segment["bg_color"] = "";
    git_branch_segment["fg_color"] = "YELLOW";
    git_branch_segment["separator"] = "";
    git_segments.push_back(git_branch_segment);
    
    nlohmann::json git_status_segment;
    git_status_segment["type"] = "text";
    git_status_segment["content"] = "{GIT_STATUS}";
    git_status_segment["bg_color"] = "";
    git_status_segment["fg_color"] = "YELLOW";
    git_status_segment["separator"] = "";
    git_segments.push_back(git_status_segment);
    
    nlohmann::json git_suffix_segment;
    git_suffix_segment["type"] = "text";
    git_suffix_segment["content"] = ")";
    git_suffix_segment["bg_color"] = "RESET";
    git_suffix_segment["fg_color"] = "GREEN";
    git_suffix_segment["separator"] = "";
    git_segments.push_back(git_suffix_segment);
    
    default_theme["git_segments"] = git_segments;
    
    // Create AI segments
    nlohmann::json ai_segments = nlohmann::json::array();
    
    nlohmann::json ai_model_segment;
    ai_model_segment["type"] = "text";
    ai_model_segment["content"] = " {AI_MODEL} ";
    ai_model_segment["bg_color"] = "RESET";
    ai_model_segment["fg_color"] = "BLUE";
    ai_model_segment["separator"] = "";
    ai_segments.push_back(ai_model_segment);
    
    nlohmann::json ai_agent_segment;
    ai_agent_segment["type"] = "text";
    ai_agent_segment["content"] = "{AI_AGENT_TYPE} ";
    ai_agent_segment["bg_color"] = "";
    ai_agent_segment["fg_color"] = "YELLOW";
    ai_agent_segment["separator"] = "";
    ai_segments.push_back(ai_agent_segment);
    
    nlohmann::json ai_divider_segment;
    ai_divider_segment["type"] = "text";
    ai_divider_segment["content"] = "{AI_DIVIDER}";
    ai_divider_segment["bg_color"] = "";
    ai_divider_segment["fg_color"] = "WHITE";
    ai_divider_segment["separator"] = "";
    ai_segments.push_back(ai_divider_segment);
    
    default_theme["ai_segments"] = ai_segments;
    
    // Create newline segments
    nlohmann::json newline_segments = nlohmann::json::array();
    
    nlohmann::json newline_segment;
    newline_segment["type"] = "text";
    newline_segment["content"] = " > ";
    newline_segment["bg_color"] = "RESET";
    newline_segment["fg_color"] = "WHITE";
    newline_segment["separator"] = "";
    newline_segments.push_back(newline_segment);
    
    default_theme["newline_segments"] = newline_segments;
    
    std::ofstream file(theme_directory + "/default.json");
    file << default_theme.dump(4);
    file.close();
}

bool Theme::load_theme(const std::string& theme_name) {
    if (!is_enabled) {
      std::cout << "Themes are disabled. Using default values." << std::endl;
      return true;
    }
    
    std::string theme_file = theme_directory + "/" + theme_name + ".json";
    
    if (!std::filesystem::exists(theme_file)) {
        return false;
    }
    
    std::ifstream file(theme_file);
    nlohmann::json theme_json;
    file >> theme_json;
    file.close();
    
    if (theme_json.contains("use_newline")) {
        use_newline = theme_json["use_newline"];
    } else {
        use_newline = false;
    }
    
    if (theme_json.contains("newline_prompt")) {
        newline_format = theme_json["newline_prompt"];
    } else {
        newline_format = " > ";
    }
    
    if (theme_json.contains("terminal_title")) {
        terminal_title_format = theme_json["terminal_title"];
    } else {
        terminal_title_format = "{USERNAME}@{HOSTNAME}: {DIRECTORY}";
    }
    
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
    } else {
        nlohmann::json default_segment;
        default_segment["type"] = "text";
        default_segment["content"] = newline_format;
        default_segment["bg_color"] = "RESET";
        default_segment["fg_color"] = "WHITE";
        default_segment["separator"] = "none";
        newline_segments.push_back(default_segment);
    }
    
    if (!newline_segments.empty() && newline_segments[0].contains("content")) {
        newline_format = newline_segments[0]["content"];
    }
    
    process_all_formats();
    
    return true;
}

void Theme::process_all_formats() {
  // Process the color tags in newline format for backward compatibility
  processed_newline_format = process_color_tags(newline_format);
}

std::string Theme::replace_placeholders(const std::string& format, const std::unordered_map<std::string, std::string>& vars) {
    std::string result = format;
    
    for (const auto& [key, value] : vars) {
        std::string placeholder = "{" + key + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }
    
    return result;
}

std::string Theme::get_ps1_prompt_format(const std::unordered_map<std::string, std::string>& vars) {
    return render_ps1_segments(vars);
}

std::string Theme::get_git_prompt_format(const std::unordered_map<std::string, std::string>& vars) {
    return render_git_segments(vars);
}

std::string Theme::get_ai_prompt_format(const std::unordered_map<std::string, std::string>& vars) {
    return render_ai_segments(vars);
}

std::string Theme::get_newline_prompt_format(const std::unordered_map<std::string, std::string>& vars) {
    return render_newline_segments(vars);
}

std::string Theme::get_terminal_title_format(const std::unordered_map<std::string, std::string>& vars) {
    return replace_placeholders(terminal_title_format, vars);
}

bool Theme::has_newline_segments() const {
    return !newline_segments.empty();
}

std::string Theme::get_newline_prompt() const {
    return processed_newline_format;
}

std::string Theme::render_segment(const nlohmann::json& segment, const std::unordered_map<std::string, std::string>& vars) {
    if (!segment.contains("type") || !segment.contains("content")) {
        return "";
    }
    
    std::string content = segment["content"];
    
    for (const auto& [key, value] : vars) {
        std::string placeholder = "{" + key + "}";
        size_t pos = 0;
        while ((pos = content.find(placeholder, pos)) != std::string::npos) {
            content.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }
    
    std::string bg_color = segment.value("bg_color", "RESET");
    std::string fg_color = segment.value("fg_color", "WHITE");
    
    colors::RGB bg = colors::get_color_by_name(bg_color);
    colors::RGB fg = colors::get_color_by_name(fg_color);
    
    std::string result = "";
    
    if (segment.contains("forward_separator") && segment["forward_separator"] != "none" && !segment["forward_separator"].empty()) {
        std::string forward_separator = segment["forward_separator"];
        
        std::string forward_sep_fg = segment.value("forward_separator_fg", "WHITE");
        std::string forward_sep_bg = segment.value("forward_separator_bg", "RESET");
        
        colors::RGB forward_sep_fg_color = colors::get_color_by_name(forward_sep_fg);
        colors::RGB forward_sep_bg_color = colors::get_color_by_name(forward_sep_bg);
        
        if (forward_separator == "gradient_right" || forward_separator == "gradient_left" || forward_separator == "gradient_curve") {
            int num_steps = segment.value("forward_gradient_steps", 4);
            std::string gradient_sep;
            
            colors::RGB start_bg = bg;
            
            for (int i = 0; i < num_steps; ++i) {
                float factor = static_cast<float>(i) / (num_steps - 1);
                colors::RGB blend_bg = colors::blend(start_bg, forward_sep_bg_color, factor);
                
                if (i > 0 && i < num_steps - 1) {
                    float brightness_adjust = (i % 2 == 0) ? 0.9f : 1.1f;
                    blend_bg.r = std::min(255, static_cast<int>(blend_bg.r * brightness_adjust));
                    blend_bg.g = std::min(255, static_cast<int>(blend_bg.g * brightness_adjust));
                    blend_bg.b = std::min(255, static_cast<int>(blend_bg.b * brightness_adjust));
                }
                
                gradient_sep += colors::bg_color(blend_bg) + " ";
            }
            
            result += gradient_sep;
        } else {
            if (forward_separator == "right_arrow" || forward_separator == "left_arrow") {
                forward_sep_fg_color = bg;
            }
            
            result += colors::fg_color(forward_sep_fg_color) + 
                      colors::bg_color(forward_sep_bg_color) + 
                      forward_separator;
        }
    }
    
    result += colors::bg_color(bg) + colors::fg_color(fg) + content;
    
    if (segment.contains("separator") && segment["separator"] != "none" && !segment["separator"].empty()) {
        std::string separator = segment["separator"];
        
        std::string sep_fg = segment.value("separator_fg", "WHITE");
        std::string sep_bg = segment.value("separator_bg", "RESET");
        
        colors::RGB sep_fg_color = colors::get_color_by_name(sep_fg);
        colors::RGB sep_bg_color = colors::get_color_by_name(sep_bg);
        
        if (separator == "gradient_right" || separator == "gradient_left" || separator == "gradient_curve") {
            if (separator == "gradient_curve") {
                int num_steps = segment.value("gradient_steps", 4);
                std::string gradient_sep;
                
                colors::RGB start_bg = bg;
                
                for (int i = 0; i < num_steps; ++i) {
                    float factor = static_cast<float>(i) / (num_steps - 1);
                    colors::RGB blend_bg = colors::blend(start_bg, sep_bg_color, factor);
                    
                    if (i > 0 && i < num_steps - 1) {
                        float brightness_adjust = (i % 2 == 0) ? 0.9f : 1.1f;
                        blend_bg.r = std::min(255, static_cast<int>(blend_bg.r * brightness_adjust));
                        blend_bg.g = std::min(255, static_cast<int>(blend_bg.g * brightness_adjust));
                        blend_bg.b = std::min(255, static_cast<int>(blend_bg.b * brightness_adjust));
                    }
                    
                    gradient_sep += colors::bg_color(blend_bg) + " ";
                }
                
                result += gradient_sep;
            } else {
                int width = segment.value("gradient_steps", 4);
                std::string gradient_sep;
                
                colors::RGB start_bg = bg;
                
                for (int i = 0; i < width; i++) {
                    float factor = static_cast<float>(i) / (width - 1);
                    colors::RGB blend_bg = colors::blend(start_bg, sep_bg_color, factor);
                    
                    if (i > 0 && i < width - 1) {
                        float brightness_adjust = (i % 2 == 0) ? 0.92f : 1.08f;
                        blend_bg.r = std::min(255, static_cast<int>(blend_bg.r * brightness_adjust));
                        blend_bg.g = std::min(255, static_cast<int>(blend_bg.g * brightness_adjust));
                        blend_bg.b = std::min(255, static_cast<int>(blend_bg.b * brightness_adjust));
                    }
                    
                    gradient_sep += colors::bg_color(blend_bg) + " ";
                }
                
                result += gradient_sep;
            }
        } else {
            if (separator == "right_arrow" || separator == "left_arrow") {
                sep_fg_color = bg;
            }
            
            result += colors::fg_color(sep_fg_color) + colors::bg_color(sep_bg_color) + separator;
        }
    }
    
    return result;
}

std::string Theme::render_ps1_segments(const std::unordered_map<std::string, std::string>& vars) {
    if (ps1_segments.empty()) {
        return "";
    }
    
    std::string result;
    for (const auto& segment : ps1_segments) {
        result += render_segment(segment, vars);
    }
    
    result += colors::style_reset();
    return result;
}

std::string Theme::render_git_segments(const std::unordered_map<std::string, std::string>& vars) {
    if (git_segments.empty()) {
        return "";
    }
    
    std::string result;
    for (const auto& segment : git_segments) {
        result += render_segment(segment, vars);
    }
    
    result += colors::style_reset();
    return result;
}

std::string Theme::render_ai_segments(const std::unordered_map<std::string, std::string>& vars) {
    if (ai_segments.empty()) {
        return "";
    }
    
    std::string result;
    for (const auto& segment : ai_segments) {
        result += render_segment(segment, vars);
    }
    
    result += colors::style_reset();
    return result;
}

std::string Theme::render_newline_segments(const std::unordered_map<std::string, std::string>& vars) {
    if (newline_segments.empty()) {
        return "";
    }
    
    std::string result;
    for (const auto& segment : newline_segments) {
        result += render_segment(segment, vars);
    }
    
    result += colors::style_reset();
    return result;
}

std::vector<std::string> Theme::list_themes() {
    std::vector<std::string> themes;
    
    if (!is_enabled) {
        themes.push_back("default");
        return themes;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(theme_directory)) {
        if (entry.path().extension() == ".json") {
            themes.push_back(entry.path().stem().string());
        }
    }
    return themes;
}

std::string Theme::process_color_tags(const std::string& format) {
    if (colors::g_color_capability == colors::ColorCapability::NO_COLOR) {
        std::string white_text = "\033[37m"; 
        return white_text + remove_color_tags(format);
    }
    
    std::string result = format;
    
    std::unordered_map<std::string, std::string> color_map = colors::get_color_map();
    
    size_t pos = 0;
    while ((pos = result.find('[', pos)) != std::string::npos) {
        size_t end_pos = result.find(']', pos);
        if (end_pos == std::string::npos) {
            break;
        }
        
        std::string color_name = result.substr(pos + 1, end_pos - pos - 1);
        if (color_map.find(color_name) != color_map.end()) {
            result.replace(pos, end_pos - pos + 1, color_map[color_name]);
        } else {
            pos = end_pos + 1;
        }
    }
    
    return result;
}

std::string Theme::remove_color_tags(const std::string& format) {
    std::string result = format;
    
    size_t pos = 0;
    while ((pos = result.find('[', pos)) != std::string::npos) {
        size_t end_pos = result.find(']', pos);
        if (end_pos == std::string::npos) {
            break;
        }
        
        result.erase(pos, end_pos - pos + 1);
    }
    
    return result;
}

bool Theme::uses_newline() const {
    return use_newline;
}
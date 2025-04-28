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
        ps1_format = "{USERNAME}@{HOSTNAME} {PATH} $";
        git_format = "{USERNAME} {DIRECTORY} git:({GIT_BRANCH} {GIT_STATUS})";
        ai_format = "{AI_MODEL} {AI_AGENT_TYPE} {AI_DIVIDER}";
        terminal_title_format = "{USERNAME}@{HOSTNAME}: {DIRECTORY}";
        use_newline = false;
        newline_format = " > ";
        
        process_all_formats();
    }
}

Theme::~Theme() {
}

void Theme::create_default_theme() {
    nlohmann::json default_theme;
    
    // Create properties in the same order as the provided file
    default_theme["ai_prompt"] = "[BLUE]{AI_MODEL} [YELLOW]{AI_AGENT_TYPE} [WHITE]{AI_DIVIDER}[RESET]";
    default_theme["git_prompt"] = "[RED]{USERNAME} [BLUE]{DIRECTORY} [GREEN]git:([YELLOW]{GIT_BRANCH} {GIT_STATUS}[GREEN])[RESET]";
    default_theme["ps1_prompt"] = "[RED]{USERNAME}[WHITE]@[GREEN]{HOSTNAME} [BLUE]{PATH} [WHITE]$[RESET]";
    default_theme["terminal_title"] = "{SHELL} {USERNAME}@{HOSTNAME}: {DIRECTORY}";
    
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
    
    // Check for newline setting
    if (theme_json.contains("use_newline")) {
        use_newline = theme_json["use_newline"];
    } else {
        use_newline = false;
    }
    
    // Check for newline prompt setting (for backward compatibility)
    if (theme_json.contains("newline_prompt")) {
        newline_format = theme_json["newline_prompt"];
    } else {
        newline_format = " > ";
    }
    
    if (theme_json.contains("ps1_prompt")) {
        ps1_format = theme_json["ps1_prompt"];
    } else {
        ps1_format = "[RED]{USERNAME}[WHITE]@[GREEN]{HOSTNAME} [BLUE]{PATH} [WHITE]$[RESET]";
    }
    
    if (theme_json.contains("git_prompt")) {
        git_format = theme_json["git_prompt"];
    } else {
        git_format = "[RED]{USERNAME} [BLUE]{DIRECTORY} [GREEN]git:([YELLOW]{GIT_BRANCH} {GIT_STATUS}[GREEN])[RESET]";
    }
    
    if (theme_json.contains("ai_prompt")) {
        ai_format = theme_json["ai_prompt"];
    } else {
        ai_format = "[BLUE]{AI_MODEL} [YELLOW]{AI_AGENT_TYPE} [WHITE]{AI_DIVIDER}[RESET]";
    }
    
    if (theme_json.contains("terminal_title")) {
        terminal_title_format = theme_json["terminal_title"];
    } else {
        terminal_title_format = "{USERNAME}@{HOSTNAME}: {DIRECTORY}";
    }
    
    // Load segmented style if available
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
        // Create default newline segment if none exists
        nlohmann::json default_segment;
        default_segment["type"] = "text";
        default_segment["content"] = newline_format; // Use the newline_format as fallback
        default_segment["bg_color"] = "RESET";
        default_segment["fg_color"] = "WHITE";
        default_segment["separator"] = "none";
        newline_segments.push_back(default_segment);
    }
    
    // Update this - convert the newline segments content to processed_newline_format
    // for backwards compatibility
    if (!newline_segments.empty() && newline_segments[0].contains("content")) {
        newline_format = newline_segments[0]["content"];
    }
    
    process_all_formats();
    
    return true;
}

void Theme::process_all_formats() {
  processed_ps1_format = process_color_tags(ps1_format);
  processed_git_format = process_color_tags(git_format);
  processed_ai_format = process_color_tags(ai_format);
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
    return replace_placeholders(processed_ps1_format, vars);
}

std::string Theme::get_git_prompt_format(const std::unordered_map<std::string, std::string>& vars) {
    return replace_placeholders(processed_git_format, vars);
}

std::string Theme::get_ai_prompt_format(const std::unordered_map<std::string, std::string>& vars) {
    return replace_placeholders(processed_ai_format, vars);
}

std::string Theme::get_newline_prompt_format(const std::unordered_map<std::string, std::string>& vars) {
    return replace_placeholders(processed_newline_format, vars);
}

std::string Theme::get_terminal_title_format(const std::unordered_map<std::string, std::string>& vars) {
    return replace_placeholders(terminal_title_format, vars);
}

bool Theme::is_segmented_style() const {
    return !ps1_segments.empty() || !git_segments.empty() || !ai_segments.empty();
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
    
    // Replace variables in content
    for (const auto& [key, value] : vars) {
        std::string placeholder = "{" + key + "}";
        size_t pos = 0;
        while ((pos = content.find(placeholder, pos)) != std::string::npos) {
            content.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }
    
    // Apply colors and styling
    std::string bg_color = segment.value("bg_color", "RESET");
    std::string fg_color = segment.value("fg_color", "WHITE");
    
    // Get background and foreground colors
    colors::RGB bg = colors::get_color_by_name(bg_color);
    colors::RGB fg = colors::get_color_by_name(fg_color);
    
    // Build the final result
    std::string result = "";
    
    // First, add forward separator if specified (goes before content)
    if (segment.contains("forward_separator") && segment["forward_separator"] != "none" && !segment["forward_separator"].empty()) {
        std::string forward_separator = segment["forward_separator"];
        
        // Get forward separator colors
        std::string forward_sep_fg = segment.value("forward_separator_fg", "WHITE");
        std::string forward_sep_bg = segment.value("forward_separator_bg", "RESET");
        
        colors::RGB forward_sep_fg_color = colors::get_color_by_name(forward_sep_fg);
        colors::RGB forward_sep_bg_color = colors::get_color_by_name(forward_sep_bg);
        
        // Process gradient separators if specified
        if (forward_separator == "gradient_right" || forward_separator == "gradient_left" || forward_separator == "gradient_curve") {
            int num_steps = segment.value("forward_gradient_steps", 4);
            std::string gradient_sep;
            
            // Use the already resolved colors
            colors::RGB start_bg = bg;
            
            for (int i = 0; i < num_steps; ++i) {
                float factor = static_cast<float>(i) / (num_steps - 1);
                colors::RGB blend_bg = colors::blend(start_bg, forward_sep_bg_color, factor);
                
                // Adjust brightness for better visual distinction
                if (i > 0 && i < num_steps - 1) {
                    float brightness_adjust = (i % 2 == 0) ? 0.9f : 1.1f;
                    blend_bg.r = std::min(255, static_cast<int>(blend_bg.r * brightness_adjust));
                    blend_bg.g = std::min(255, static_cast<int>(blend_bg.g * brightness_adjust));
                    blend_bg.b = std::min(255, static_cast<int>(blend_bg.b * brightness_adjust));
                }
                
                gradient_sep += colors::bg_color(blend_bg) + " ";
            }
            
            // Add the gradient forward separator
            result += gradient_sep;
        } else {
            // Handle special cases like right_arrow or left_arrow
            if (forward_separator == "right_arrow" || forward_separator == "left_arrow") {
                forward_sep_fg_color = bg; // Use the segment's background color for the separator foreground
            }
            
            // Apply the styling and add the forward separator
            result += colors::fg_color(forward_sep_fg_color) + 
                      colors::bg_color(forward_sep_bg_color) + 
                      forward_separator;
        }
    }
    
    // Add styled content
    result += colors::bg_color(bg) + colors::fg_color(fg) + content;
    
    // Add backward separator if specified (goes after content)
    if (segment.contains("separator") && segment["separator"] != "none" && !segment["separator"].empty()) {
        std::string separator = segment["separator"];
        
        // Get separator colors
        std::string sep_fg = segment.value("separator_fg", "WHITE");
        std::string sep_bg = segment.value("separator_bg", "RESET");
        
        colors::RGB sep_fg_color = colors::get_color_by_name(sep_fg);
        colors::RGB sep_bg_color = colors::get_color_by_name(sep_bg);
        
        // For gradient separators, create a special gradient transition
        if (separator == "gradient_right" || separator == "gradient_left" || separator == "gradient_curve") {
            // For gradient separators, we create a smooth transition between segments
            if (separator == "gradient_curve") {
                // Get custom gradient steps if provided, otherwise use default
                int num_steps = segment.value("gradient_steps", 4); // Allow customization of gradient steps
                std::string gradient_sep;
                
                // Use the already resolved colors to avoid issues with P10K_ prefixes
                colors::RGB start_bg = bg;
                
                for (int i = 0; i < num_steps; ++i) {
                    float factor = static_cast<float>(i) / (num_steps - 1);
                    colors::RGB blend_bg = colors::blend(start_bg, sep_bg_color, factor);
                    
                    // Adjust brightness slightly for more visual distinction
                    if (i > 0 && i < num_steps - 1) {
                        // Make intermediate colors slightly brighter/darker for better visual separation
                        float brightness_adjust = (i % 2 == 0) ? 0.9f : 1.1f;
                        blend_bg.r = std::min(255, static_cast<int>(blend_bg.r * brightness_adjust));
                        blend_bg.g = std::min(255, static_cast<int>(blend_bg.g * brightness_adjust));
                        blend_bg.b = std::min(255, static_cast<int>(blend_bg.b * brightness_adjust));
                    }
                    
                    gradient_sep += colors::bg_color(blend_bg) + " ";
                }
                
                result += gradient_sep;
            } else {
                // For right or left gradient, create a smooth color transition of spaces
                int width = segment.value("gradient_steps", 4); // Allow customization of gradient width
                std::string gradient_sep;
                
                // Use the already resolved colors to avoid issues with P10K_ prefixes
                colors::RGB start_bg = bg;
                
                for (int i = 0; i < width; i++) {
                    float factor = static_cast<float>(i) / (width - 1);
                    colors::RGB blend_bg = colors::blend(start_bg, sep_bg_color, factor);
                    
                    // Adjust brightness for better visual distinction
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
            // For the right_arrow separator specifically, we need to make sure 
            // the foreground color matches the background of the segment for a seamless transition
            if (separator == "right_arrow" || separator == "left_arrow") {
                sep_fg_color = bg; // Use the segment's background color for the separator foreground
            }
            
            // Apply the styling to the separator
            result += colors::fg_color(sep_fg_color) + colors::bg_color(sep_bg_color) + separator;
        }
    }
    
    return result;
}

// Render PS1 segments
std::string Theme::render_ps1_segments(const std::unordered_map<std::string, std::string>& vars) {
    if (ps1_segments.empty()) {
        return processed_ps1_format;
    }
    
    std::string result;
    for (const auto& segment : ps1_segments) {
        result += render_segment(segment, vars);
    }
    
    result += colors::style_reset();
    return result;
}

// Render Git segments
std::string Theme::render_git_segments(const std::unordered_map<std::string, std::string>& vars) {
    if (git_segments.empty()) {
        return processed_git_format;
    }
    
    std::string result;
    for (const auto& segment : git_segments) {
        result += render_segment(segment, vars);
    }
    
    result += colors::style_reset();
    return result;
}

// Render AI segments
std::string Theme::render_ai_segments(const std::unordered_map<std::string, std::string>& vars) {
    if (ai_segments.empty()) {
        return processed_ai_format;
    }
    
    std::string result;
    for (const auto& segment : ai_segments) {
        result += render_segment(segment, vars);
    }
    
    result += colors::style_reset();
    return result;
}

// Render Newline segments
std::string Theme::render_newline_segments(const std::unordered_map<std::string, std::string>& vars) {
    if (newline_segments.empty()) {
        // If for some reason there are no segments, use the processed format
        return processed_newline_format;
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
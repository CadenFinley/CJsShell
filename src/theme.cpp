#include "theme.h"
#include "main.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <unordered_set>
#include <sys/ioctl.h>    // new
#include <unistd.h>       // new
#include "colors.h"

//add the ability to allign the segments to the left or right

Theme::Theme(std::string theme_dir, bool enabled) : theme_directory(theme_dir), is_enabled(enabled) {
    if (!std::filesystem::exists(theme_directory + "/default.json")) {
        create_default_theme();
    }
    is_enabled = enabled;
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
    
    default_theme["newline_segments"] = nlohmann::json::array();
    default_theme["fill_char"] = " ";

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

    if (theme_json.contains("fill_char") && theme_json["fill_char"].is_string()) {
        auto s = theme_json["fill_char"].get<std::string>();
        if (s.size() == 1) fill_char_ = s[0];
    }

    //prerender_segments();
    
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
    if (g_debug_mode) {
        std::cout << "Prerendered line: \n" << result << std::endl;
    }
    return result;
}

void Theme::prerender_segments() {
    prerendered_ps1_format     = prerender_line_aligned(ps1_segments);
    prerendered_git_format     = prerender_line_aligned(git_segments);
    prerendered_ai_format      = prerender_line_aligned(ai_segments);
    prerendered_newline_format = prerender_line_aligned(newline_segments);
}

size_t Theme::get_terminal_width() const {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        return w.ws_col;
    }
    return 80;
}

// build colored string for a bucket (no final ANSI reset)
static std::string build_basic_line(const std::vector<nlohmann::json>& segments) {
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
    return result;
}

std::string Theme::prerender_line_aligned(const std::vector<nlohmann::json>& segments) const {
    if (segments.empty()) return "";
    bool hasAlign = false;
    for (const auto& seg : segments) {
        if (seg.contains("align")) {
            hasAlign = true;
            break;
        }
    }
    if (!hasAlign) {
        return prerender_line(segments);
    }

    std::vector<nlohmann::json> left, center, right;
    for (auto& seg : segments) {
        auto a = seg.value("align", "left");
        if      (a == "center") center.push_back(seg);
        else if (a == "right")  right .push_back(seg);
        else                    left  .push_back(seg);
    }
    auto L = build_basic_line(left);
    auto C = build_basic_line(center);
    auto R = build_basic_line(right);

    size_t w  = get_terminal_width();
    size_t lL = calculate_raw_length(L);
    size_t lC = calculate_raw_length(C);
    size_t lR = calculate_raw_length(R);

    std::string out;
    if (!C.empty()) {
        // compute desired start of center segment = (terminal width - center length) / 2
        size_t desiredCStart = (w > lC ? (w - lC) / 2 : 0);
        // pad between left segment and center to reach desiredCStart
        size_t padL = (desiredCStart > lL ? desiredCStart - lL : 0);
        // after placing left+padL+center, compute remaining space before right
        size_t afterC = lL + padL + lC;
        size_t padR = (w > afterC + lR ? w - afterC - lR : 0);
        out = L
            + std::string(padL, fill_char_)
            + C
            + std::string(padR, fill_char_)
            + R;
    } else {
        size_t pad = (w > lL + lR ? w - lL - lR : 0);
        out = L + std::string(pad, fill_char_) + R;
    }
    out += colors::ansi::RESET;
    if (g_debug_mode) {
        std::cout << "Aligned prerendered line:\n" << out << std::endl;
    }
    return out;
}

// new combined render for left/center/right with vars
std::string Theme::render_line_aligned(
    const std::vector<nlohmann::json>& segments,
    const std::unordered_map<std::string,std::string>& vars) const
{
    if (segments.empty()) return "";
    bool hasAlign = false;
    for (auto& seg: segments)
        if (seg.contains("align")) { hasAlign = true; break; }
    if (!hasAlign) {
        // fallback to simple render+color
        std::string flat = prerender_line(segments);
        return render_line(flat, vars);
    }

    // split buckets
    std::vector<nlohmann::json> left, center, right;
    for (auto& seg: segments) {
        auto a = seg.value("align","left");
        if      (a=="center") center.push_back(seg);
        else if (a=="right")  right .push_back(seg);
        else                  left  .push_back(seg);
    }

    // build each bucket, substituting placeholders first
    auto build = [&](const std::vector<nlohmann::json>& bucket){
        std::string out;
        for (auto& segment: bucket) {
            std::string segment_result;
            // first substitute placeholders in content & separator
            std::string content   = render_line(segment.value("content",""), vars);
            std::string separator = render_line(segment.value("separator",""), vars);

            // grab color names
            std::string bg_color_name   = segment.value("bg_color","RESET");
            std::string fg_color_name   = segment.value("fg_color","RESET");
            std::string sep_fg_name     = segment.value("separator_fg","RESET");
            std::string sep_bg_name     = segment.value("separator_bg","RESET");

            // forward_separator (if any)
            if (segment.contains("forward_separator") && !segment["forward_separator"].empty()) {
                std::string fsep    = segment["forward_separator"];
                std::string fsep_fg = segment.value("forward_separator_fg","RESET");
                std::string fsep_bg = segment.value("forward_separator_bg","RESET");
                if (fsep_bg != "RESET") {
                    segment_result += colors::bg_color(colors::parse_color_value(fsep_bg));
                } else {
                    segment_result += colors::ansi::BG_RESET;
                }
                if (fsep_fg != "RESET") {
                    segment_result += colors::fg_color(colors::parse_color_value(fsep_fg));
                }
                segment_result += fsep;
            }

            // background color
            if (bg_color_name != "RESET") {
                segment_result += colors::bg_color(colors::parse_color_value(bg_color_name));
            } else {
                segment_result += colors::ansi::BG_RESET;
            }
            // foreground color
            if (fg_color_name != "RESET") {
                segment_result += colors::fg_color(colors::parse_color_value(fg_color_name));
            }
            // the content
            segment_result += content;
            // trailing separator
            if (!separator.empty()) {
                if (sep_fg_name != "RESET") {
                    segment_result += colors::fg_color(colors::parse_color_value(sep_fg_name));
                }
                if (sep_bg_name != "RESET") {
                    segment_result += colors::bg_color(colors::parse_color_value(sep_bg_name));
                } else {
                    segment_result += colors::ansi::BG_RESET;
                }
                segment_result += separator;
            }

            out += segment_result;
        }
        return out;
    };

    auto L = build(left);
    auto C = build(center);
    auto R = build(right);

    size_t w  = get_terminal_width();
    size_t lL = calculate_raw_length(L);
    size_t lC = calculate_raw_length(C);
    size_t lR = calculate_raw_length(R);

    std::string out;
    if (!C.empty()) {
        size_t desiredCStart = (w>lC ? (w-lC)/2 : 0);
        size_t padL = (desiredCStart>lL?desiredCStart-lL:0);
        size_t afterC = lL+padL+lC;
        size_t padR = (w>afterC+lR? w-afterC-lR:0);
        out = L + std::string(padL, fill_char_) + C + std::string(padR, fill_char_) + R;
    } else {
        size_t pad = (w>lL+lR? w-lL-lR:0);
        out = L + std::string(pad, fill_char_) + R;
    }
    out += colors::ansi::RESET;
    if (g_debug_mode) std::cout<<"\nCombined render:\n"<<out<<std::endl;
    return out;
}

// update getters to use the new combined render
std::string Theme::get_ps1_prompt_format(const std::unordered_map<std::string,std::string>& vars) const {
    auto        result = render_line_aligned(ps1_segments, vars);
    last_ps1_raw_length = calculate_raw_length(result);
    if (g_debug_mode) std::cout<<"Last PS1 raw length: "<<last_ps1_raw_length<<std::endl;
    return result;
}

std::string Theme::get_git_prompt_format(const std::unordered_map<std::string,std::string>& vars) const {
    auto        result = render_line_aligned(git_segments, vars);
    last_git_raw_length = calculate_raw_length(result);
    if (g_debug_mode) std::cout<<"Last Git raw length: "<<last_git_raw_length<<std::endl;
    return result;
}

std::string Theme::get_ai_prompt_format(const std::unordered_map<std::string,std::string>& vars) const {
    auto        result = render_line_aligned(ai_segments, vars);
    last_ai_raw_length = calculate_raw_length(result);
    if (g_debug_mode) std::cout<<"Last AI raw length: "<<last_ai_raw_length<<std::endl;
    return result;
}

std::string Theme::get_newline_prompt(const std::unordered_map<std::string,std::string>& vars) const {
    auto        result = render_line_aligned(newline_segments, vars);
    last_newline_raw_length = calculate_raw_length(result);
    if (g_debug_mode) std::cout<<"Last newline raw length: "<<last_newline_raw_length<<std::endl;
    return result;
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
    if(g_debug_mode) {
        std::cout << "Rendered line: \n" << result << std::endl;
    }
    return result;
}

size_t Theme::calculate_raw_length(const std::string& str) const {
    size_t raw_length = 0;
    for (size_t i = 0; i < str.size();) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if (c == '\033') {
            if (i + 1 < str.size()) {
                unsigned char next = static_cast<unsigned char>(str[i+1]);
                if (next == '[') { // CSI
                    i += 2;
                    while (i < str.size() && !(str[i] >= '@' && str[i] <= '~')) ++i;
                    if (i < str.size()) ++i;
                    continue;
                } else if (next == ']') { // OSC
                    i += 2;
                    while (i < str.size()) {
                        if (str[i] == '\a') { ++i; break; }
                        if (str[i] == '\033' && i+1 < str.size() && str[i+1] == '\\') { i += 2; break; }
                        ++i;
                    }
                    continue;
                } else { // other ESC
                    i += 2;
                    continue;
                }
            }
            ++i;
        } else {
            if ((c & 0xC0) != 0x80) ++raw_length;
            ++i;
        }
    }
    return raw_length;
}
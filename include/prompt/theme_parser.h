#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <variant>

// Forward declarations
struct ThemeSegment;
struct ThemeDefinition;

// Represents a key-value pair in the DSL
struct ThemeProperty {
    std::string key;
    std::string value;
    
    ThemeProperty(const std::string& k, const std::string& v) : key(k), value(v) {}
};

// Represents a segment in the theme
struct ThemeSegment {
    std::string name;
    std::string content;
    std::string fg_color;
    std::string bg_color;
    std::string separator;
    std::string separator_fg;
    std::string separator_bg;
    std::string forward_separator;
    std::string forward_separator_fg;
    std::string forward_separator_bg;
    std::string alignment;  // "left", "center", "right"
    
    ThemeSegment() = default;
    ThemeSegment(const std::string& n) : name(n) {}
    
    // Convert to the old json-like structure for compatibility
    std::unordered_map<std::string, std::string> to_map() const;
};

// Represents requirements for a theme
struct ThemeRequirements {
    std::vector<std::string> plugins;
    std::string colors;
    std::vector<std::string> fonts;
    std::unordered_map<std::string, std::string> custom;
};

// Represents behavior settings
struct ThemeBehavior {
    bool cleanup = false;
    bool cleanup_empty_line = false;
    bool newline_after_execution = false;
};

// Represents fill settings
struct ThemeFill {
    std::string character = "";
    std::string fg_color = "RESET";
    std::string bg_color = "RESET";
};

// Main theme definition structure
struct ThemeDefinition {
    std::string name;
    std::string terminal_title;
    
    ThemeFill fill;
    ThemeBehavior behavior;
    ThemeRequirements requirements;
    std::unordered_map<std::string, std::string> variables;
    
    std::vector<ThemeSegment> ps1_segments;
    std::vector<ThemeSegment> git_segments;
    std::vector<ThemeSegment> ai_segments;
    std::vector<ThemeSegment> newline_segments;
    std::vector<ThemeSegment> inline_right_segments;
    
    ThemeDefinition() = default;
    ThemeDefinition(const std::string& n) : name(n) {}
};

// Parser for the new DSL
class ThemeParser {
private:
    std::string content;
    size_t position;
    size_t line_number;
    
    void skip_whitespace();
    void skip_comments();
    char peek() const;
    char advance();
    bool is_at_end() const;
    
    std::string parse_string();
    std::string parse_identifier();
    std::string parse_value();
    
    ThemeProperty parse_property();
    ThemeSegment parse_segment();
    std::vector<ThemeSegment> parse_segments_block();
    ThemeFill parse_fill_block();
    ThemeBehavior parse_behavior_block();
    ThemeRequirements parse_requirements_block();
    std::unordered_map<std::string, std::string> parse_variables_block();
    
    void expect_token(const std::string& expected);
    void parse_error(const std::string& message);

public:
    ThemeParser(const std::string& theme_content);
    
    ThemeDefinition parse();
    
    // Static utility methods
    static ThemeDefinition parse_file(const std::string& filepath);
    static std::string write_theme(const ThemeDefinition& theme);
};
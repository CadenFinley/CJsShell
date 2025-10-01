#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <variant>
#include <optional>

#include "error_out.h"

class ThemeParseException : public std::runtime_error {
public:
    ThemeParseException(size_t line, std::string detail,
                        std::string source = "",
                        std::optional<ErrorInfo> error_info = std::nullopt);

    size_t line() const noexcept { return line_; }
    const std::string& detail() const noexcept { return detail_; }
    const std::string& source() const noexcept { return source_; }
    const std::optional<ErrorInfo>& error_info() const noexcept {
        return error_info_;
    }

private:
    static std::string build_message(size_t line, const std::string& detail,
                                     const std::string& source);

    size_t line_;
    std::string detail_;
    std::string source_;
    std::optional<ErrorInfo> error_info_;
};

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
    std::unordered_map<std::string, ThemeSegment> segment_variables;
    
    std::vector<ThemeSegment> ps1_segments;
    std::vector<ThemeSegment> git_segments;
    std::vector<ThemeSegment> ai_segments;
    std::vector<ThemeSegment> newline_segments;
    std::vector<ThemeSegment> inline_right_segments;
    
    ThemeDefinition() = default;
    ThemeDefinition(const std::string& n) : name(n) {}
};

struct ThemeVariableSet {
    std::unordered_map<std::string, std::string> string_variables;
    std::unordered_map<std::string, ThemeSegment> segment_variables;
};

// Parser for the new DSL
class ThemeParser {
private:
    std::string content;
    size_t position;
    size_t line_number;
    std::string source_name;
    std::unordered_map<std::string, ThemeSegment> segment_variable_definitions;
    
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
    ThemeSegment parse_segment_body(ThemeSegment segment);
    ThemeSegment parse_segment_reference();
    bool is_keyword(const std::string& keyword) const;
    std::vector<ThemeSegment> parse_segments_block();
    ThemeFill parse_fill_block();
    ThemeBehavior parse_behavior_block();
    ThemeRequirements parse_requirements_block();
    ThemeVariableSet parse_variables_block();
    
    void expect_token(const std::string& expected);
    [[noreturn]] void parse_error(const std::string& message);

public:
    ThemeParser(const std::string& theme_content,
                std::string source_name = "");
    
    ThemeDefinition parse();
    
    // Static utility methods
    static ThemeDefinition parse_file(const std::string& filepath);
    static std::string write_theme(const ThemeDefinition& theme);
};
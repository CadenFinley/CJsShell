#include "theme_parser.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <utf8proc.h>

namespace {

std::string encode_utf8(char32_t codepoint) {
    if (!utf8proc_codepoint_valid(static_cast<utf8proc_int32_t>(codepoint))) {
        throw std::runtime_error("Invalid Unicode codepoint in theme string");
    }

    utf8proc_uint8_t buffer[4];
    utf8proc_ssize_t length = utf8proc_encode_char(
        static_cast<utf8proc_int32_t>(codepoint), buffer);

    if (length < 0 || length > 4) {
        throw std::runtime_error("Failed to encode Unicode codepoint in theme string");
    }

    return std::string(reinterpret_cast<char*>(buffer),
                       reinterpret_cast<char*>(buffer) + length);
}

bool is_hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    return 10 + (c - 'A');
}

}  // namespace

std::unordered_map<std::string, std::string> ThemeSegment::to_map() const {
    std::unordered_map<std::string, std::string> result;
    result["tag"] = name;
    result["content"] = content;
    result["fg_color"] = fg_color;
    result["bg_color"] = bg_color;  
    result["separator"] = separator;
    result["separator_fg"] = separator_fg;
    result["separator_bg"] = separator_bg;
    if (!forward_separator.empty()) {
        result["forward_separator"] = forward_separator;
        result["forward_separator_fg"] = forward_separator_fg;
        result["forward_separator_bg"] = forward_separator_bg;
    }
    return result;
}

ThemeParser::ThemeParser(const std::string& theme_content) 
    : content(theme_content), position(0), line_number(1) {}

void ThemeParser::skip_whitespace() {
    while (!is_at_end() && std::isspace(peek())) {
        if (peek() == '\n') {
            line_number++;
        }
        advance();
    }
}

void ThemeParser::skip_comments() {
    while (!is_at_end() && peek() == '#') {
        // Skip to end of line
        while (!is_at_end() && peek() != '\n') {
            advance();
        }
        if (!is_at_end()) {
            advance(); // Skip the newline
            line_number++;
        }
    }
}

char ThemeParser::peek() const {
    if (is_at_end()) return '\0';
    return content[position];
}

char ThemeParser::advance() {
    if (is_at_end()) return '\0';
    return content[position++];
}

bool ThemeParser::is_at_end() const {
    return position >= content.length();
}

std::string ThemeParser::parse_string() {
    if (peek() != '"') {
        parse_error("Expected string literal");
    }
    
    advance(); // Skip opening quote
    std::string result;
    
    while (!is_at_end() && peek() != '"') {
        if (peek() == '\\') {
            advance(); // Skip backslash
            if (is_at_end()) {
                parse_error("Unterminated string literal");
            }
            
            char escaped = advance();
            switch (escaped) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                case 'u': {
                    char32_t codepoint = 0;
                    int digits_parsed = 0;

                    if (peek() == '{') {
                        advance(); // Skip '{'
                        while (!is_at_end() && peek() != '}') {
                            char hex = advance();
                            if (!is_hex_digit(hex)) {
                                parse_error("Invalid hex digit in unicode escape");
                            }
                            if (digits_parsed >= 8) {
                                parse_error("Unicode escape sequence is too long");
                            }
                            codepoint = (codepoint << 4) | hex_value(hex);
                            digits_parsed++;
                        }

                        if (is_at_end() || peek() != '}') {
                            parse_error("Unterminated unicode escape sequence");
                        }
                        advance(); // Skip '}'

                        if (digits_parsed == 0) {
                            parse_error("Empty unicode escape sequence");
                        }
                    } else {
                        for (int i = 0; i < 4; ++i) {
                            if (is_at_end()) {
                                parse_error("Incomplete unicode escape sequence");
                            }
                            char hex = advance();
                            if (!is_hex_digit(hex)) {
                                parse_error("Invalid hex digit in unicode escape");
                            }
                            codepoint = (codepoint << 4) | hex_value(hex);
                            digits_parsed++;
                        }
                    }

                    try {
                        result += encode_utf8(codepoint);
                    } catch (const std::runtime_error& err) {
                        parse_error(err.what());
                    }
                    break;
                }
                case 'U': {
                    char32_t codepoint = 0;
                    for (int i = 0; i < 8; ++i) {
                        if (is_at_end()) {
                            parse_error("Incomplete unicode escape sequence");
                        }
                        char hex = advance();
                        if (!is_hex_digit(hex)) {
                            parse_error("Invalid hex digit in unicode escape");
                        }
                        codepoint = (codepoint << 4) | hex_value(hex);
                    }

                    try {
                        result += encode_utf8(codepoint);
                    } catch (const std::runtime_error& err) {
                        parse_error(err.what());
                    }
                    break;
                }
                default: 
                    result += escaped;
                    break;
            }
        } else {
            result += advance();
        }
    }
    
    if (is_at_end()) {
        parse_error("Unterminated string literal");
    }
    
    advance(); // Skip closing quote
    return result;
}

std::string ThemeParser::parse_identifier() {
    std::string result;
    
    if (!std::isalpha(peek()) && peek() != '_') {
        parse_error("Expected identifier");
    }
    
    while (!is_at_end() && (std::isalnum(peek()) || peek() == '_')) {
        result += advance();
    }
    
    return result;
}

std::string ThemeParser::parse_value() {
    skip_whitespace();
    
    if (peek() == '"') {
        return parse_string();
    } else if (std::isalpha(peek()) || peek() == '_' || peek() == '#') {
        // Parse identifier or color code
        std::string result;
        while (!is_at_end() && !std::isspace(peek()) && peek() != ',' && peek() != '}') {
            result += advance();
        }
        return result;
    } else if (std::isdigit(peek()) || peek() == '.') {
        // Parse number
        std::string result;
        while (!is_at_end() && (std::isdigit(peek()) || peek() == '.')) {
            result += advance();
        }
        return result;
    } else if (peek() == '{') {
        // Parse complex expression (for conditionals)
        std::string result;
        int brace_count = 0;
        while (!is_at_end()) {
            char c = peek();
            if (c == '{') brace_count++;
            if (c == '}') brace_count--;
            result += advance();
            if (brace_count == 0) break;
        }
        return result;
    }
    
    parse_error("Expected value");
    return "";
}

ThemeProperty ThemeParser::parse_property() {
    skip_whitespace();
    skip_comments();
    
    std::string key = parse_identifier();
    
    skip_whitespace();
    
    std::string value;
    if (peek() == '"') {
        value = parse_string();
    } else {
        value = parse_value();
    }
    
    skip_whitespace();
    
    // Optional comma
    if (peek() == ',') {
        advance();
    }
    
    return ThemeProperty(key, value);
}

ThemeSegment ThemeParser::parse_segment() {
    skip_whitespace();
    skip_comments();
    
    expect_token("segment");
    skip_whitespace();
    
    std::string segment_name = parse_string();
    ThemeSegment segment(segment_name);
    
    skip_whitespace();
    expect_token("{");
    
    while (!is_at_end()) {
        skip_whitespace();
        skip_comments();
        
        if (peek() == '}') {
            advance();
            break;
        }
        
        ThemeProperty prop = parse_property();
        
        if (prop.key == "content") {
            segment.content = prop.value;
        } else if (prop.key == "fg") {
            segment.fg_color = prop.value;
        } else if (prop.key == "bg") {
            segment.bg_color = prop.value;
        } else if (prop.key == "separator") {
            segment.separator = prop.value;
        } else if (prop.key == "separator_fg") {
            segment.separator_fg = prop.value;
        } else if (prop.key == "separator_bg") {
            segment.separator_bg = prop.value;
        } else if (prop.key == "forward_separator") {
            segment.forward_separator = prop.value;
        } else if (prop.key == "forward_separator_fg") {
            segment.forward_separator_fg = prop.value;
        } else if (prop.key == "forward_separator_bg") {
            segment.forward_separator_bg = prop.value;
        } else if (prop.key == "alignment") {
            segment.alignment = prop.value;
        }
    }
    
    return segment;
}

std::vector<ThemeSegment> ThemeParser::parse_segments_block() {
    std::vector<ThemeSegment> segments;
    
    skip_whitespace();
    expect_token("{");
    
    while (!is_at_end()) {
        skip_whitespace();
        skip_comments();
        
        if (peek() == '}') {
            advance();
            break;
        }
        
        segments.push_back(parse_segment());
    }
    
    return segments;
}

ThemeFill ThemeParser::parse_fill_block() {
    ThemeFill fill;
    
    skip_whitespace();
    expect_token("{");
    
    while (!is_at_end()) {
        skip_whitespace();
        skip_comments();
        
        if (peek() == '}') {
            advance();
            break;
        }
        
        ThemeProperty prop = parse_property();
        
        if (prop.key == "char") {
            fill.character = prop.value;
        } else if (prop.key == "fg") {
            fill.fg_color = prop.value;
        } else if (prop.key == "bg") {
            fill.bg_color = prop.value;
        }
    }
    
    return fill;
}

ThemeBehavior ThemeParser::parse_behavior_block() {
    ThemeBehavior behavior;
    
    skip_whitespace();
    expect_token("{");
    
    while (!is_at_end()) {
        skip_whitespace();
        skip_comments();
        
        if (peek() == '}') {
            advance();
            break;
        }
        
        ThemeProperty prop = parse_property();
        
        if (prop.key == "cleanup") {
            behavior.cleanup = (prop.value == "true");
        } else if (prop.key == "cleanup_empty_line") {
            behavior.cleanup_empty_line = (prop.value == "true");
        } else if (prop.key == "newline_after_execution") {
            behavior.newline_after_execution = (prop.value == "true");
        }
    }
    
    return behavior;
}

ThemeRequirements ThemeParser::parse_requirements_block() {
    ThemeRequirements requirements;
    
    skip_whitespace();
    expect_token("{");
    
    while (!is_at_end()) {
        skip_whitespace();
        skip_comments();
        
        if (peek() == '}') {
            advance();
            break;
        }
        
        ThemeProperty prop = parse_property();
        
        if (prop.key == "plugins") {
            // Parse array of plugins (simplified for now)
            requirements.plugins.push_back(prop.value);
        } else if (prop.key == "colors") {
            requirements.colors = prop.value;
        } else if (prop.key == "fonts") {
            // Parse array of fonts (simplified for now) 
            requirements.fonts.push_back(prop.value);
        } else {
            requirements.custom[prop.key] = prop.value;
        }
    }
    
    return requirements;
}

void ThemeParser::expect_token(const std::string& expected) {
    skip_whitespace();
    
    for (char c : expected) {
        if (is_at_end() || peek() != c) {
            parse_error("Expected '" + expected + "'");
        }
        advance();
    }
}

void ThemeParser::parse_error(const std::string& message) {
    std::ostringstream oss;
    oss << "Parse error at line " << line_number << ": " << message;
    throw std::runtime_error(oss.str());
}

ThemeDefinition ThemeParser::parse() {
    skip_whitespace();
    skip_comments();
    
    // Skip shebang if present
    if (position == 0 && content.length() > 2 && content.substr(0, 2) == "#!") {
        while (!is_at_end() && peek() != '\n') {
            advance();
        }
        if (!is_at_end()) {
            advance(); // Skip newline
            line_number++;
        }
    }
    
    skip_whitespace();
    skip_comments();
    
    expect_token("theme_definition");
    skip_whitespace();
    
    std::string theme_name = parse_string();
    ThemeDefinition theme(theme_name);
    
    skip_whitespace();
    expect_token("{");
    
    while (!is_at_end()) {
        skip_whitespace();
        skip_comments();
        
        if (peek() == '}') {
            advance();
            break;
        }
        
        std::string block_name = parse_identifier();
        skip_whitespace();
        
        if (block_name == "terminal_title") {
            theme.terminal_title = parse_string();
        } else if (block_name == "fill") {
            theme.fill = parse_fill_block();
        } else if (block_name == "behavior") {
            theme.behavior = parse_behavior_block();
        } else if (block_name == "requirements") {
            theme.requirements = parse_requirements_block();
        } else if (block_name == "ps1") {
            theme.ps1_segments = parse_segments_block();
        } else if (block_name == "git_segments") {
            theme.git_segments = parse_segments_block();
        } else if (block_name == "ai_segments") {
            theme.ai_segments = parse_segments_block();
        } else if (block_name == "newline") {
            theme.newline_segments = parse_segments_block();
        } else if (block_name == "inline_right") {
            theme.inline_right_segments = parse_segments_block();
        } else {
            parse_error("Unknown block: " + block_name);
        }
    }
    
    return theme;
}

ThemeDefinition ThemeParser::parse_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open theme file: " + filepath);
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    ThemeParser parser(content);
    return parser.parse();
}

std::string ThemeParser::write_theme(const ThemeDefinition& theme) {
    std::ostringstream oss;
    oss << "#! usr/bin/env cjsh\n\n";
    oss << "theme_definition \"" << theme.name << "\" {\n";
    
    if (!theme.terminal_title.empty()) {
        oss << "  terminal_title \"" << theme.terminal_title << "\"\n\n";
    }
    
    // Write fill block
    oss << "  fill {\n";
    oss << "    char \"" << theme.fill.character << "\",\n";
    oss << "    fg " << theme.fill.fg_color << "\n";
    oss << "    bg " << theme.fill.bg_color << "\n";
    oss << "  }\n\n";
    
    // Write segments
    auto write_segments = [&](const std::string& name, const std::vector<ThemeSegment>& segments) {
        if (!segments.empty()) {
            oss << "  " << name << " {\n";
            for (const auto& segment : segments) {
                oss << "    segment \"" << segment.name << "\" {\n";
                oss << "      content \"" << segment.content << "\"\n";
                oss << "      fg \"" << segment.fg_color << "\"\n";
                oss << "      bg \"" << segment.bg_color << "\"\n";
                if (!segment.separator.empty()) {
                    oss << "      separator \"" << segment.separator << "\"\n";
                    oss << "      separator_fg \"" << segment.separator_fg << "\"\n";
                    oss << "      separator_bg \"" << segment.separator_bg << "\"\n";
                }
                if (!segment.forward_separator.empty()) {
                    oss << "      forward_separator \"" << segment.forward_separator << "\"\n";
                    oss << "      forward_separator_fg \"" << segment.forward_separator_fg << "\"\n";
                    oss << "      forward_separator_bg \"" << segment.forward_separator_bg << "\"\n";
                }
                if (!segment.alignment.empty()) {
                    oss << "      alignment \"" << segment.alignment << "\"\n";
                }
                oss << "    }\n";
            }
            oss << "  }\n\n";
        }
    };
    
    write_segments("ps1", theme.ps1_segments);
    write_segments("git_segments", theme.git_segments);
    write_segments("ai_segments", theme.ai_segments);
    write_segments("newline", theme.newline_segments);
    write_segments("inline_right", theme.inline_right_segments);
    
    // Write behavior
    oss << "  behavior {\n";
    oss << "    cleanup " << (theme.behavior.cleanup ? "true" : "false") << "\n";
    oss << "    cleanup_empty_line " << (theme.behavior.cleanup_empty_line ? "true" : "false") << "\n";
    oss << "    newline_after_execution " << (theme.behavior.newline_after_execution ? "true" : "false") << "\n";
    oss << "  }\n";
    
    oss << "}\n";
    
    return oss.str();
}
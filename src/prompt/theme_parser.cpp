#include "theme_parser.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <cctype>
#include <algorithm>
#include <utf8proc.h>

ThemeParseException::ThemeParseException(size_t line, std::string detail,
                                         std::string source)
    : std::runtime_error(
          build_message(line, detail, source)),
      line_(line),
      detail_(std::move(detail)),
      source_(std::move(source)) {}

std::string ThemeParseException::build_message(size_t line,
                                               const std::string& detail,
                                               const std::string& source) {
    std::ostringstream oss;
    oss << "Theme parse error";
    if (!source.empty()) {
        oss << " in '" << source << "'";
    }
    if (line > 0) {
        oss << " at line " << line;
    }
    if (!detail.empty()) {
        oss << ": " << detail;
    }
    return oss.str();
}

namespace {

std::string trim_copy(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

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

std::string expand_variables_in_string(
    const std::string& input,
    const std::unordered_map<std::string, std::string>& variables,
    std::unordered_map<std::string, std::string>& cache,
    std::vector<std::string>& stack);

std::string expand_variable_reference(
    const std::string& name,
    const std::unordered_map<std::string, std::string>& variables,
    std::unordered_map<std::string, std::string>& cache,
    std::vector<std::string>& stack);

std::string expand_variables_in_string(
    const std::string& input,
    const std::unordered_map<std::string, std::string>& variables,
    std::unordered_map<std::string, std::string>& cache,
    std::vector<std::string>& stack) {
    if (input.empty() || variables.empty()) {
        return input;
    }

    std::string result;
    size_t position = 0;

    while (position < input.size()) {
        size_t marker = input.find("${", position);
        if (marker == std::string::npos) {
            result.append(input.substr(position));
            break;
        }

        result.append(input.substr(position, marker - position));

        size_t close_brace = input.find('}', marker + 2);
        if (close_brace == std::string::npos) {
            throw std::runtime_error(
                "Unterminated theme variable reference in '" + input + "'");
        }

        std::string raw_name =
            input.substr(marker + 2, close_brace - (marker + 2));
        std::string name = trim_copy(raw_name);

        if (name.empty()) {
            throw std::runtime_error("Empty theme variable reference detected");
        }

        std::string replacement =
            expand_variable_reference(name, variables, cache, stack);
        result.append(replacement);

        position = close_brace + 1;
    }

    if (result.empty()) {
        return input;
    }

    return result;
}

std::string expand_variable_reference(
    const std::string& name,
    const std::unordered_map<std::string, std::string>& variables,
    std::unordered_map<std::string, std::string>& cache,
    std::vector<std::string>& stack) {
    auto cached = cache.find(name);
    if (cached != cache.end()) {
        return cached->second;
    }

    if (std::find(stack.begin(), stack.end(), name) != stack.end()) {
        throw std::runtime_error(
            "Cyclic theme variable reference detected for '" + name + "'");
    }

    auto it = variables.find(name);
    if (it == variables.end()) {
        throw std::runtime_error(
            "Undefined theme variable referenced: '" + name + "'");
    }

    stack.push_back(name);
    std::string expanded = it->second;
    if (expanded.find("${") != std::string::npos) {
        expanded = expand_variables_in_string(expanded, variables, cache, stack);
    }
    stack.pop_back();

    cache[name] = expanded;
    return expanded;
}

std::unordered_map<std::string, std::string> resolve_theme_variables(
    const std::unordered_map<std::string, std::string>& variables) {
    if (variables.empty()) {
        return {};
    }

    std::unordered_map<std::string, std::string> cache;
    std::unordered_map<std::string, std::string> resolved;

    for (const auto& [name, _] : variables) {
        std::vector<std::string> stack;
        resolved[name] = expand_variable_reference(name, variables, cache, stack);
    }

    return resolved;
}

void substitute_variables_in_string(
    std::string& target,
    const std::unordered_map<std::string, std::string>& variables) {
    if (target.empty() || variables.empty()) {
        return;
    }

    if (target.find("${") == std::string::npos) {
        return;
    }

    std::unordered_map<std::string, std::string> cache;
    std::vector<std::string> stack;
    target = expand_variables_in_string(target, variables, cache, stack);
}

void apply_variables_to_segment(
    ThemeSegment& segment,
    const std::unordered_map<std::string, std::string>& variables) {
    substitute_variables_in_string(segment.content, variables);
    substitute_variables_in_string(segment.fg_color, variables);
    substitute_variables_in_string(segment.bg_color, variables);
    substitute_variables_in_string(segment.separator, variables);
    substitute_variables_in_string(segment.separator_fg, variables);
    substitute_variables_in_string(segment.separator_bg, variables);
    substitute_variables_in_string(segment.forward_separator, variables);
    substitute_variables_in_string(segment.forward_separator_fg, variables);
    substitute_variables_in_string(segment.forward_separator_bg, variables);
    substitute_variables_in_string(segment.alignment, variables);
}

void apply_variables_to_segments(
    std::vector<ThemeSegment>& segments,
    const std::unordered_map<std::string, std::string>& variables) {
    for (auto& segment : segments) {
        apply_variables_to_segment(segment, variables);
    }
}

void apply_variables_to_theme(
    ThemeDefinition& theme,
    const std::unordered_map<std::string, std::string>& variables) {
    if (variables.empty()) {
        return;
    }

    substitute_variables_in_string(theme.terminal_title, variables);

    substitute_variables_in_string(theme.fill.character, variables);
    substitute_variables_in_string(theme.fill.fg_color, variables);
    substitute_variables_in_string(theme.fill.bg_color, variables);

    substitute_variables_in_string(theme.requirements.colors, variables);
    for (auto& plugin : theme.requirements.plugins) {
        substitute_variables_in_string(plugin, variables);
    }
    for (auto& font : theme.requirements.fonts) {
        substitute_variables_in_string(font, variables);
    }
    for (auto& custom_pair : theme.requirements.custom) {
        substitute_variables_in_string(custom_pair.second, variables);
    }

    apply_variables_to_segments(theme.ps1_segments, variables);
    apply_variables_to_segments(theme.git_segments, variables);
    apply_variables_to_segments(theme.ai_segments, variables);
    apply_variables_to_segments(theme.newline_segments, variables);
    apply_variables_to_segments(theme.inline_right_segments, variables);
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

ThemeParser::ThemeParser(const std::string& theme_content,
                                                 std::string source_name) 
        : content(theme_content),
            position(0),
            line_number(1),
            source_name(std::move(source_name)) {}

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

std::unordered_map<std::string, std::string> ThemeParser::parse_variables_block() {
    std::unordered_map<std::string, std::string> variables;

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
        if (variables.find(prop.key) != variables.end()) {
            parse_error("Duplicate variable definition: " + prop.key);
        }
        variables[prop.key] = prop.value;
    }

    return variables;
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
    throw ThemeParseException(line_number, message, source_name);
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
        } else if (block_name == "variables") {
            theme.variables = parse_variables_block();
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
    
    try {
        auto resolved_variables = resolve_theme_variables(theme.variables);
        apply_variables_to_theme(theme, resolved_variables);
    } catch (const ThemeParseException&) {
        throw;
    } catch (const std::exception& e) {
        throw ThemeParseException(0, e.what(), source_name);
    }

    return theme;
}

ThemeDefinition ThemeParser::parse_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw ThemeParseException(0, "Could not open theme file", filepath);
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    ThemeParser parser(content, filepath);
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

    if (!theme.variables.empty()) {
        std::vector<std::pair<std::string, std::string>> variables(
            theme.variables.begin(), theme.variables.end());
        std::sort(variables.begin(), variables.end(),
                  [](const auto& lhs, const auto& rhs) {
                      return lhs.first < rhs.first;
                  });

        oss << "  variables {\n";
        for (const auto& [name, value] : variables) {
            oss << "    " << name << " \"" << value << "\"\n";
        }
        oss << "  }\n\n";
    }
    
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
#include "theme_parser.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>
#include "parser/parser_utils.h"
#include "utils/string_utils.h"

extern "C" {
#include "isocline/unicode.h"
}

ThemeParseException::ThemeParseException(size_t line, std::string detail, std::string source,
                                         std::optional<ErrorInfo> error_info,
                                         std::optional<ThemeParseContext> context)
    : std::runtime_error(build_message(line, detail, source)),
      line_(line),
      detail_(std::move(detail)),
      source_(std::move(source)),
      error_info_(std::move(error_info)),
      context_(std::move(context)) {
}

std::string ThemeParseException::build_message(size_t line, const std::string& detail,
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

std::string derive_theme_name_from_source(const std::string& source_name) {
    if (source_name.empty()) {
        return "";
    }

    std::filesystem::path source_path(source_name);
    if (source_path.has_stem()) {
        return source_path.stem().string();
    }

    return source_name;
}

std::string encode_utf8(char32_t codepoint) {
    if (!unicode_is_valid_codepoint(static_cast<unicode_codepoint_t>(codepoint))) {
        throw std::runtime_error("Invalid Unicode codepoint in theme string");
    }

    uint8_t buffer[4] = {0};
    int length = unicode_encode_utf8(static_cast<unicode_codepoint_t>(codepoint), buffer);

    if (length <= 0 || length > 4) {
        throw std::runtime_error("Failed to encode Unicode codepoint in theme string");
    }

    return std::string(reinterpret_cast<const char*>(buffer), static_cast<size_t>(length));
}

int hex_value(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');
    return 10 + (c - 'A');
}

std::string expand_variables_in_string(
    const std::string& input, const std::unordered_map<std::string, std::string>& variables,
    std::unordered_map<std::string, std::string>& cache, std::vector<std::string>& stack);

std::string expand_variable_reference(const std::string& name,
                                      const std::unordered_map<std::string, std::string>& variables,
                                      std::unordered_map<std::string, std::string>& cache,
                                      std::vector<std::string>& stack);

std::string expand_variables_in_string(
    const std::string& input, const std::unordered_map<std::string, std::string>& variables,
    std::unordered_map<std::string, std::string>& cache, std::vector<std::string>& stack) {
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
            throw std::runtime_error("Unterminated theme variable reference in '" + input + "'");
        }

        std::string raw_name = input.substr(marker + 2, close_brace - (marker + 2));
        std::string name = string_utils::trim_ascii_whitespace_copy(raw_name);

        if (name.empty()) {
            throw std::runtime_error("Empty theme variable reference detected");
        }

        std::string replacement = expand_variable_reference(name, variables, cache, stack);
        result.append(replacement);

        position = close_brace + 1;
    }

    if (result.empty()) {
        return input;
    }

    return result;
}

std::string expand_variable_reference(const std::string& name,
                                      const std::unordered_map<std::string, std::string>& variables,
                                      std::unordered_map<std::string, std::string>& cache,
                                      std::vector<std::string>& stack) {
    auto cached = cache.find(name);
    if (cached != cache.end()) {
        return cached->second;
    }

    if (std::find(stack.begin(), stack.end(), name) != stack.end()) {
        throw std::runtime_error("Cyclic theme variable reference detected for '" + name + "'");
    }

    auto it = variables.find(name);
    if (it == variables.end()) {
        throw std::runtime_error("Undefined theme variable referenced: '" + name + "'");
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

void substitute_variables_in_string(std::string& target,
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

void apply_variables_to_segment(ThemeSegment& segment,
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

void apply_variables_to_segments(std::vector<ThemeSegment>& segments,
                                 const std::unordered_map<std::string, std::string>& variables) {
    for (auto& segment : segments) {
        apply_variables_to_segment(segment, variables);
    }
}

void apply_variables_to_theme(ThemeDefinition& theme,
                              const std::unordered_map<std::string, std::string>& variables) {
    if (variables.empty()) {
        return;
    }

    substitute_variables_in_string(theme.terminal_title, variables);

    substitute_variables_in_string(theme.fill.character, variables);
    substitute_variables_in_string(theme.fill.fg_color, variables);
    substitute_variables_in_string(theme.fill.bg_color, variables);

    for (auto& [name, segment_var] : theme.segment_variables) {
        apply_variables_to_segment(segment_var, variables);
    }

    substitute_variables_in_string(theme.requirements.colors, variables);
    for (auto& font : theme.requirements.fonts) {
        substitute_variables_in_string(font, variables);
    }
    for (auto& custom_pair : theme.requirements.custom) {
        substitute_variables_in_string(custom_pair.second, variables);
    }

    apply_variables_to_segments(theme.ps1_segments, variables);
    apply_variables_to_segments(theme.git_segments, variables);
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

ThemeParser::ThemeParser(const std::string& theme_content, std::string source_name)
    : content(theme_content),
      position(0),
      line_number(1),
      source_name(std::move(source_name)),
      segment_variable_definitions() {
}

void ThemeParser::skip_whitespace() {
    while (!is_at_end() && (std::isspace(peek()) != 0)) {
        if (peek() == '\n') {
            line_number++;
        }
        advance();
    }
}

void ThemeParser::skip_comments() {
    while (!is_at_end() && peek() == '#') {
        while (!is_at_end() && peek() != '\n') {
            advance();
        }
        if (!is_at_end()) {
            advance();
            line_number++;
        }
    }
}

char ThemeParser::peek() const {
    if (is_at_end())
        return '\0';
    return content[position];
}

char ThemeParser::advance() {
    if (is_at_end())
        return '\0';
    return content[position++];
}

bool ThemeParser::is_at_end() const {
    return position >= content.length();
}

bool ThemeParser::is_keyword(const std::string& keyword) const {
    size_t remaining = content.size() - position;
    if (remaining < keyword.size()) {
        return false;
    }

    if (content.compare(position, keyword.size(), keyword) != 0) {
        return false;
    }

    size_t next_pos = position + keyword.size();
    if (next_pos < content.size()) {
        unsigned char next = static_cast<unsigned char>(content[next_pos]);
        if ((std::isalnum(next) != 0) || next == '_') {
            return false;
        }
    }

    return true;
}

std::string ThemeParser::parse_string() {
    if (peek() != '"') {
        parse_error("Expected string literal");
    }

    advance();
    std::string result;

    while (!is_at_end() && peek() != '"') {
        if (peek() == '\\') {
            advance();
            if (is_at_end()) {
                parse_error("Unterminated string literal");
            }

            char escaped = advance();
            switch (escaped) {
                case 'n':
                    result += '\n';
                    break;
                case 't':
                    result += '\t';
                    break;
                case 'r':
                    result += '\r';
                    break;
                case '\\':
                    result += '\\';
                    break;
                case '"':
                    result += '"';
                    break;
                case 'u': {
                    char32_t codepoint = 0;
                    int digits_parsed = 0;

                    if (peek() == '{') {
                        advance();
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
                        advance();

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

    advance();
    return result;
}

std::string ThemeParser::parse_identifier() {
    std::string result;

    if ((std::isalpha(peek()) == 0) && peek() != '_') {
        parse_error("Expected identifier");
    }

    while (!is_at_end() && ((std::isalnum(peek()) != 0) || peek() == '_')) {
        result += advance();
    }

    return result;
}

std::string ThemeParser::parse_value() {
    skip_whitespace();

    if (peek() == '"') {
        return parse_string();
    }
    if (std::isalpha(peek()) || peek() == '_' || peek() == '#') {
        std::string result;
        while (!is_at_end() && !std::isspace(peek()) && peek() != ',' && peek() != '}') {
            result += advance();
        }
        return result;
    }
    if (std::isdigit(peek()) || peek() == '.') {
        std::string result;
        while (!is_at_end() && (std::isdigit(peek()) || peek() == '.')) {
            result += advance();
        }
        return result;
    }
    if (peek() == '{') {
        std::string result;
        int brace_count = 0;
        while (!is_at_end()) {
            char c = peek();
            if (c == '{')
                brace_count++;
            if (c == '}')
                brace_count--;
            result += advance();
            if (brace_count == 0)
                break;
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

    return parse_segment_body(std::move(segment));
}

ThemeSegment ThemeParser::parse_segment_body(ThemeSegment segment) {
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
        } else if (prop.key == "bold") {
            segment.bold = (prop.value == "true" || prop.value == "1" || prop.value == "yes");
        } else if (prop.key == "italic") {
            segment.italic = (prop.value == "true" || prop.value == "1" || prop.value == "yes");
        } else if (prop.key == "underline") {
            segment.underline = (prop.value == "true" || prop.value == "1" || prop.value == "yes");
        } else if (prop.key == "dim") {
            segment.dim = (prop.value == "true" || prop.value == "1" || prop.value == "yes");
        } else if (prop.key == "strikethrough") {
            segment.strikethrough =
                (prop.value == "true" || prop.value == "1" || prop.value == "yes");
        }
    }

    return segment;
}

ThemeSegment ThemeParser::parse_segment_reference() {
    skip_whitespace();
    skip_comments();

    expect_token("use_segment");
    skip_whitespace();

    std::string reference_name;
    if (peek() == '"') {
        reference_name = parse_string();
    } else {
        reference_name = parse_identifier();
    }

    skip_whitespace();
    skip_comments();

    std::optional<std::string> alias;
    if (is_keyword("as")) {
        expect_token("as");
        skip_whitespace();
        skip_comments();
        if (peek() == '"') {
            alias = parse_string();
        } else {
            alias = parse_identifier();
        }
    }

    auto it = segment_variable_definitions.find(reference_name);
    if (it == segment_variable_definitions.end()) {
        parse_error("Undefined segment variable referenced: " + reference_name);
    }

    ThemeSegment segment_copy = it->second;
    if (alias) {
        segment_copy.name = *alias;
    }

    skip_whitespace();
    if (peek() == ',') {
        advance();
    }

    return segment_copy;
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

        if (is_keyword("use_segment")) {
            segments.push_back(parse_segment_reference());
        } else {
            segments.push_back(parse_segment());
        }
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
        } else if (prop.key == "cleanup_truncate_multiline" ||
                   prop.key == "cleanup_trunicate_multilin" ||
                   prop.key == "cleanup_trunicate_multiline") {
            behavior.cleanup_truncate_multiline = (prop.value == "true");
        } else if (prop.key == "newline_after_execution") {
            behavior.newline_after_execution = (prop.value == "true");
        } else if (prop.key == "cleanup_nl_after_exec") {
            behavior.cleanup_nl_after_exec = (prop.value == "true");
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

        if (prop.key == "colors") {
            requirements.colors = prop.value;
        } else if (prop.key == "fonts") {
            requirements.fonts.push_back(prop.value);
        } else {
            requirements.custom[prop.key] = prop.value;
        }
    }

    return requirements;
}

ThemeVariableSet ThemeParser::parse_variables_block() {
    ThemeVariableSet variables;

    skip_whitespace();
    expect_token("{");

    while (!is_at_end()) {
        skip_whitespace();
        skip_comments();

        if (peek() == '}') {
            advance();
            break;
        }

        if (is_keyword("segment")) {
            expect_token("segment");
            skip_whitespace();

            std::string segment_key;
            if (peek() == '"') {
                segment_key = parse_string();
            } else {
                segment_key = parse_identifier();
            }

            if (variables.string_variables.count(segment_key) ||
                variables.segment_variables.count(segment_key) ||
                segment_variable_definitions.count(segment_key)) {
                parse_error("Duplicate variable definition: " + segment_key);
            }

            ThemeSegment segment(segment_key);
            segment = parse_segment_body(std::move(segment));

            variables.segment_variables[segment_key] = segment;
            segment_variable_definitions[segment_key] = segment;

            skip_whitespace();
            if (peek() == ',') {
                advance();
            }
            continue;
        }

        ThemeProperty prop = parse_property();
        if (variables.string_variables.find(prop.key) != variables.string_variables.end() ||
            variables.segment_variables.find(prop.key) != variables.segment_variables.end() ||
            segment_variable_definitions.find(prop.key) != segment_variable_definitions.end()) {
            parse_error("Duplicate variable definition: " + prop.key);
        }
        variables.string_variables[prop.key] = prop.value;
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
    std::optional<ThemeParseContext> context;

    if (!content.empty()) {
        ThemeParseContext ctx;

        size_t highlight_index = std::min(position, content.size());
        bool at_content_end = highlight_index >= content.size();
        bool at_line_break = false;
        if (!at_content_end) {
            char current_char = content[highlight_index];
            at_line_break = current_char == '\n' || current_char == '\r';
        }

        size_t reference_index = highlight_index;
        if ((at_content_end || at_line_break) && reference_index > 0) {
            reference_index -= 1;
        }
        if (!content.empty() && reference_index >= content.size()) {
            reference_index = content.size() - 1;
        }

        size_t line_start = reference_index;
        while (line_start > 0) {
            char ch = content[line_start - 1];
            if (ch == '\n' || ch == '\r') {
                break;
            }
            --line_start;
        }

        size_t line_end = reference_index;
        while (line_end < content.size()) {
            char ch = content[line_end];
            if (ch == '\n' || ch == '\r') {
                break;
            }
            ++line_end;
        }

        ctx.line_content = content.substr(line_start, line_end - line_start);

        if (!ctx.line_content.empty()) {
            if (at_content_end || at_line_break) {
                ctx.column_start = ctx.line_content.size();
            } else if (reference_index >= line_start) {
                ctx.column_start = reference_index - line_start;
            }

            ctx.column_end = std::min(ctx.column_start + 1, ctx.line_content.size());
            if (ctx.column_end == ctx.column_start && ctx.column_start < ctx.line_content.size()) {
                ctx.column_end = ctx.column_start + 1;
            }
        } else {
            ctx.column_start = 0;
            ctx.column_end = 0;
        }

        ctx.char_offset = highlight_index;

        context = std::move(ctx);
    }

    ErrorInfo info{ErrorType::SYNTAX_ERROR,
                   source_name.empty() ? "theme_parser" : source_name,
                   message,
                   {"Check theme syntax and try again."}};
    throw ThemeParseException(line_number, message, source_name, info, context);
}

ThemeDefinition ThemeParser::parse() {
    skip_whitespace();
    skip_comments();

    segment_variable_definitions.clear();

    if (position == 0 && content.length() > 2 && content.substr(0, 2) == "#!") {
        while (!is_at_end() && peek() != '\n') {
            advance();
        }
        if (!is_at_end()) {
            advance();
            line_number++;
        }
    }

    skip_whitespace();
    skip_comments();

    expect_token("theme_definition");
    skip_whitespace();

    std::string theme_name;
    if (!is_at_end() && peek() == '"') {
        theme_name = parse_string();
    }

    if (theme_name.empty()) {
        theme_name = derive_theme_name_from_source(source_name);
        if (theme_name.empty()) {
            theme_name = "unnamed_theme";
        }
    }

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
            ThemeVariableSet parsed_variables = parse_variables_block();
            theme.variables = std::move(parsed_variables.string_variables);
            theme.segment_variables = std::move(parsed_variables.segment_variables);
        } else if (block_name == "ps1") {
            theme.ps1_segments = parse_segments_block();
        } else if (block_name == "git_segments") {
            theme.git_segments = parse_segments_block();
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
        ErrorInfo info{ErrorType::RUNTIME_ERROR,
                       source_name.empty() ? "theme_parser" : source_name,
                       e.what(),
                       {}};
        throw ThemeParseException(0, e.what(), source_name, info);
    }

    return theme;
}

ThemeDefinition ThemeParser::parse_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        ErrorInfo info{ErrorType::FILE_NOT_FOUND,
                       filepath,
                       "Could not open theme file",
                       {"Verify the file path and permissions."}};
        throw ThemeParseException(0, "Could not open theme file", filepath, info);
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    ThemeParser parser(content, filepath);
    return parser.parse();
}

std::string ThemeParser::write_theme(const ThemeDefinition& theme) {
    std::ostringstream oss;
    oss << "#! usr/bin/env cjsh\n\n";
    oss << "theme_definition {\n";

    if (!theme.terminal_title.empty()) {
        oss << "  terminal_title \"" << theme.terminal_title << "\"\n\n";
    }

    oss << "  fill {\n";
    oss << "    char \"" << theme.fill.character << "\",\n";
    oss << "    fg " << theme.fill.fg_color << "\n";
    oss << "    bg " << theme.fill.bg_color << "\n";
    oss << "  }\n\n";

    auto write_segment_definition = [&](const ThemeSegment& segment,
                                        const std::string& base_indent) {
        std::string inner_indent = base_indent + "  ";
        oss << base_indent << "segment \"" << segment.name << "\" {\n";
        oss << inner_indent << "content \"" << segment.content << "\"\n";
        oss << inner_indent << "fg \"" << segment.fg_color << "\"\n";
        oss << inner_indent << "bg \"" << segment.bg_color << "\"\n";
        if (!segment.separator.empty()) {
            oss << inner_indent << "separator \"" << segment.separator << "\"\n";
            oss << inner_indent << "separator_fg \"" << segment.separator_fg << "\"\n";
            oss << inner_indent << "separator_bg \"" << segment.separator_bg << "\"\n";
        }
        if (!segment.forward_separator.empty()) {
            oss << inner_indent << "forward_separator \"" << segment.forward_separator << "\"\n";
            oss << inner_indent << "forward_separator_fg \"" << segment.forward_separator_fg
                << "\"\n";
            oss << inner_indent << "forward_separator_bg \"" << segment.forward_separator_bg
                << "\"\n";
        }
        if (!segment.alignment.empty()) {
            oss << inner_indent << "alignment \"" << segment.alignment << "\"\n";
        }
        oss << base_indent << "}\n";
    };

    if (!theme.variables.empty() || !theme.segment_variables.empty()) {
        std::vector<std::pair<std::string, std::string>> scalar_variables(theme.variables.begin(),
                                                                          theme.variables.end());
        std::sort(scalar_variables.begin(), scalar_variables.end(),
                  [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

        std::vector<std::pair<std::string, const ThemeSegment*>> segment_variables;
        segment_variables.reserve(theme.segment_variables.size());
        for (const auto& [name, segment] : theme.segment_variables) {
            segment_variables.emplace_back(name, &segment);
        }
        std::sort(segment_variables.begin(), segment_variables.end(),
                  [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

        oss << "  variables {\n";
        for (const auto& [name, value] : scalar_variables) {
            oss << "    " << name << " \"" << value << "\"\n";
        }

        if (!segment_variables.empty()) {
            if (!scalar_variables.empty()) {
                oss << "\n";
            }
            for (const auto& [name, segment_ptr] : segment_variables) {
                ThemeSegment segment_copy = *segment_ptr;
                segment_copy.name = name;
                write_segment_definition(segment_copy, "    ");
            }
        }

        oss << "  }\n\n";
    }

    auto write_segments = [&](const std::string& name, const std::vector<ThemeSegment>& segments) {
        if (!segments.empty()) {
            oss << "  " << name << " {\n";
            for (const auto& segment : segments) {
                write_segment_definition(segment, "    ");
            }
            oss << "  }\n\n";
        }
    };

    write_segments("ps1", theme.ps1_segments);
    write_segments("git_segments", theme.git_segments);
    write_segments("newline", theme.newline_segments);
    write_segments("inline_right", theme.inline_right_segments);

    oss << "  behavior {\n";
    oss << "    cleanup " << (theme.behavior.cleanup ? "true" : "false") << "\n";
    oss << "    cleanup_empty_line " << (theme.behavior.cleanup_empty_line ? "true" : "false")
        << "\n";
    oss << "    cleanup_truncate_multiline "
        << (theme.behavior.cleanup_truncate_multiline ? "true" : "false") << "\n";
    oss << "    newline_after_execution "
        << (theme.behavior.newline_after_execution ? "true" : "false") << "\n";
    oss << "    cleanup_nl_after_exec " << (theme.behavior.cleanup_nl_after_exec ? "true" : "false")
        << "\n";
    oss << "  }\n";

    oss << "}\n";

    return oss.str();
}
#include "theme.h"

#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <utility>
#include <vector>

#include "utils/utf8_utils.h"

#include "cjsh.h"
#include "colors.h"
#include "error_out.h"
#include "script_interpreter/shell_script_interpreter_error_reporter.h"

namespace {

bool report_theme_parse_error_with_script_reporter(const ThemeParseException& exception,
                                                   const std::string& source_hint) {
    if (!exception.context()) {
        return false;
    }

    const ThemeParseContext& ctx = *exception.context();

    ShellScriptInterpreter::ErrorPosition position{exception.line(), ctx.column_start,
                                                   ctx.column_end, ctx.char_offset};

    std::string message;
    if (!source_hint.empty()) {
        message = "Theme parse error in '" + source_hint + "'";
    } else if (!exception.source().empty()) {
        message = "Theme parse error in '" + exception.source() + "'";
    } else {
        message = "Theme parse error";
    }

    if (!exception.detail().empty()) {
        if (!message.empty()) {
            message += ": " + exception.detail();
        } else {
            message = exception.detail();
        }
    }

    std::string suggestion = "Check theme syntax and try again.";
    if (exception.error_info() && !exception.error_info()->suggestions.empty()) {
        suggestion = exception.error_info()->suggestions.front();
    }

    ShellScriptInterpreter::SyntaxError error(
        position, ShellScriptInterpreter::ErrorSeverity::ERROR,
        ShellScriptInterpreter::ErrorCategory::SYNTAX, "THEME001", message, ctx.line_content,
        suggestion);

    if (!source_hint.empty()) {
        error.related_info.push_back("Source: " + source_hint);
    } else if (!exception.source().empty()) {
        error.related_info.push_back("Source: " + exception.source());
    }

    std::vector<ShellScriptInterpreter::SyntaxError> errors;
    errors.push_back(std::move(error));

    shell_script_interpreter::reset_error_count();
    shell_script_interpreter::print_error_report(errors, true, !ctx.line_content.empty());
    return true;
}

}  // namespace

Theme::Theme(std::string theme_dir, bool enabled)
    : theme_directory(std::move(theme_dir)), is_enabled(enabled) {
    is_enabled = enabled;
}

Theme::~Theme() {
}

void Theme::clear_theme_state() {
    theme_data = ThemeDefinition{};
    terminal_title_format.clear();
    fill_char_.clear();
    fill_fg_color_ = "RESET";
    fill_bg_color_ = "RESET";
    cleanup_ = false;
    cleanup_add_empty_line_ = false;
    newline_after_execution_ = false;
    cleanup_nl_after_exec_ = false;
    last_ps1_raw_length = 0;
    last_git_raw_length = 0;
    last_newline_raw_length = 0;
}

bool Theme::load_theme(const std::string& theme_name, bool allow_fallback) {
    std::string theme_name_to_use = strip_theme_extension(theme_name);
    if (!is_enabled || theme_name_to_use.empty()) {
        clear_theme_state();
        g_current_theme.clear();
        return false;
    }

    std::filesystem::path theme_path = resolve_theme_file(theme_name_to_use);
    std::string theme_file = theme_path.string();

    if (!std::filesystem::exists(theme_path)) {
        print_error({ErrorType::FILE_NOT_FOUND,
                     "load_theme",
                     "Theme file '" + theme_file + "' does not exist.",
                     {"Use 'theme' to see available themes."}});
        return false;
    }

    try {
        ThemeDefinition parsed_definition = ThemeParser::parse_file(theme_file);
        return apply_theme_definition(parsed_definition, theme_name_to_use, allow_fallback,
                                      theme_path);
    } catch (const ThemeParseException& e) {
        bool reported = report_theme_parse_error_with_script_reporter(e, theme_file);
        std::string message = "Failed to parse theme file '" + theme_file + "'";
        if (e.line() > 0) {
            message += " at line " + std::to_string(e.line());
        }
        if (!e.detail().empty()) {
            message += ": " + e.detail();
        }

        if (!reported) {
            if (e.error_info()) {
                ErrorInfo error = *e.error_info();
                error.type = ErrorType::SYNTAX_ERROR;
                error.command_used = "load_theme";
                error.message = message;
                if (error.suggestions.empty()) {
                    error.suggestions.push_back("Check theme syntax and try again.");
                }
                print_error(error);
            } else {
                print_error({ErrorType::SYNTAX_ERROR,
                             "load_theme",
                             message,
                             {"Check theme syntax and try again."}});
            }
        }
        return false;
    } catch (const std::exception& e) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "load_theme",
                     "Failed to load theme '" + theme_name_to_use + "': " + e.what(),
                     {}});
        return false;
    }
}

bool Theme::load_theme_from_path(const std::filesystem::path& file_path, bool allow_fallback) {
    if (!is_enabled) {
        return false;
    }

    std::filesystem::path normalized = file_path.lexically_normal();

    std::error_code abs_ec;
    auto absolute_path = std::filesystem::absolute(normalized, abs_ec);
    if (!abs_ec) {
        normalized = absolute_path.lexically_normal();
    }

    std::error_code status_ec;
    auto status = std::filesystem::status(normalized, status_ec);
    if (status_ec || !std::filesystem::is_regular_file(status)) {
        print_error({ErrorType::FILE_NOT_FOUND,
                     "load_theme",
                     "Theme file '" + normalized.string() + "' does not exist.",
                     {"Use 'theme' to see available themes."}});
        return false;
    }

    std::string theme_name_to_use = strip_theme_extension(normalized.filename().string());
    if (theme_name_to_use.empty()) {
        theme_name_to_use = "default";
    }

    try {
        ThemeDefinition parsed_definition = ThemeParser::parse_file(normalized.string());
        return apply_theme_definition(parsed_definition, theme_name_to_use, allow_fallback,
                                      normalized);
    } catch (const ThemeParseException& e) {
        bool reported = report_theme_parse_error_with_script_reporter(e, normalized.string());
        std::string message = "Failed to parse theme file '" + normalized.string() + "'";
        if (e.line() > 0) {
            message += " at line " + std::to_string(e.line());
        }
        if (!e.detail().empty()) {
            message += ": " + e.detail();
        }
        if (!reported) {
            if (e.error_info()) {
                ErrorInfo error = *e.error_info();
                error.type = ErrorType::SYNTAX_ERROR;
                error.command_used = "load_theme";
                error.message = message;
                if (error.suggestions.empty()) {
                    error.suggestions.push_back("Check theme syntax and try again.");
                }
                print_error(error);
            } else {
                print_error({ErrorType::SYNTAX_ERROR,
                             "load_theme",
                             message,
                             {"Check theme syntax and try again."}});
            }
        }
        return false;
    } catch (const std::exception& e) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "load_theme",
                     "Failed to load theme '" + theme_name_to_use + "': " + e.what(),
                     {}});
        return false;
    }
}

bool Theme::load_theme_from_string(const std::string& theme_content, const std::string& source_name,
                                   bool allow_fallback) {
    if (!is_enabled) {
        return false;
    }

    std::string source_label = source_name.empty() ? std::string("inline_theme") : source_name;

    auto sanitize_for_path = [](const std::string& label, const std::string& fallback) {
        std::string safe;
        safe.reserve(label.size());
        for (char ch : label) {
            switch (ch) {
                case '<':
                case '>':
                case ':':
                case '"':
                case '|':
                case '?':
                case '*':
                    safe.push_back('_');
                    break;
                default:
                    safe.push_back(ch);
                    break;
            }
        }
        if (safe.empty()) {
            safe = fallback;
        }
        return std::filesystem::path(safe);
    };

    try {
        ThemeParser parser(theme_content, source_label);
        ThemeDefinition parsed_definition = parser.parse();

        std::string theme_name_to_use =
            parsed_definition.name.empty() ? std::string("default") : parsed_definition.name;

        std::filesystem::path pseudo_source = sanitize_for_path(source_label, theme_name_to_use);

        return apply_theme_definition(parsed_definition, theme_name_to_use, allow_fallback,
                                      pseudo_source);
    } catch (const ThemeParseException& e) {
        bool reported = report_theme_parse_error_with_script_reporter(e, source_label);
        std::string message = "Failed to parse inline theme";
        if (!source_label.empty()) {
            message += " '" + source_label + "'";
        }
        if (e.line() > 0) {
            message += " at line " + std::to_string(e.line());
        }
        if (!e.detail().empty()) {
            message += ": " + e.detail();
        }

        if (!reported) {
            if (e.error_info()) {
                ErrorInfo error = *e.error_info();
                error.type = ErrorType::SYNTAX_ERROR;
                error.command_used = "load_theme";
                error.message = message;
                if (error.suggestions.empty()) {
                    error.suggestions.push_back("Check theme syntax and try again.");
                }
                print_error(error);
            } else {
                print_error({ErrorType::SYNTAX_ERROR,
                             "load_theme",
                             message,
                             {"Check theme syntax and try again."}});
            }
        }
        return false;
    } catch (const std::exception& e) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "load_theme",
                     "Failed to load inline theme '" + source_label + "': " + e.what(),
                     {}});
        return false;
    }
}

bool Theme::apply_theme_definition(const ThemeDefinition& definition, const std::string& theme_name,
                                   bool allow_fallback, const std::filesystem::path& source_path) {
    const auto& requirements = definition.requirements;
    bool has_requirements =
        !requirements.colors.empty() || !requirements.fonts.empty() || !requirements.custom.empty();
    const std::string source_hint = source_path.empty() ? theme_name : source_path.string();

    if (has_requirements && !check_theme_requirements(requirements)) {
        if (!allow_fallback) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "load_theme",
                         "Theme '" + theme_name + "' requirements not met (" + source_hint +
                             "), cannot load theme.",
                         {}});
            return false;
        }

        const std::string previous_theme = g_current_theme;
        if (!previous_theme.empty() && theme_name != previous_theme) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "load_theme",
                         "Theme '" + theme_name + "' requirements not met (" + source_hint +
                             "), falling back to previous theme '" + previous_theme + "'.",
                         {}});
            return load_theme(previous_theme, allow_fallback);
        }

        print_error({ErrorType::RUNTIME_ERROR,
                     "load_theme",
                     "Theme '" + theme_name + "' requirements not met (" + source_hint +
                         "), falling back to basic prompt.",
                     {}});
        clear_theme_state();
        g_current_theme.clear();
        return false;
    }

    auto has_duplicate_tags = [](const std::vector<ThemeSegment>& segs) {
        std::unordered_set<std::string> seen;
        for (const auto& s : segs) {
            if (!seen.insert(s.name).second) {
                return true;
            }
        }
        return false;
    };

    if (has_duplicate_tags(definition.ps1_segments) ||
        has_duplicate_tags(definition.git_segments) ||
        has_duplicate_tags(definition.newline_segments) ||
        has_duplicate_tags(definition.inline_right_segments)) {
        print_error({ErrorType::SYNTAX_ERROR,
                     "load_theme",
                     "Duplicate tags found in theme segments (" + source_hint + ").",
                     {"Ensure all segment tags are unique within their section."}});
        return false;
    }

    theme_data = definition;
    terminal_title_format = theme_data.terminal_title;
    fill_char_ = theme_data.fill.character;
    fill_fg_color_ = theme_data.fill.fg_color;
    fill_bg_color_ = theme_data.fill.bg_color;
    cleanup_ = theme_data.behavior.cleanup;
    cleanup_add_empty_line_ = theme_data.behavior.cleanup_empty_line;
    newline_after_execution_ = theme_data.behavior.newline_after_execution;
    cleanup_nl_after_exec_ = theme_data.behavior.cleanup_nl_after_exec;

    g_current_theme = theme_name;
    return true;
}

size_t Theme::get_terminal_width() const {
    struct winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        return w.ws_col;
    }
    return 80;
}

std::string Theme::render_line_aligned(
    const std::vector<ThemeSegment>& segments,
    const std::unordered_map<std::string, std::string>& vars) const {
    if (segments.empty())
        return "";

    bool isNewlineSegments = (&segments == &newline_segments);

    auto build = [&](const std::vector<ThemeSegment>& bucket) {
        std::string out;

        auto render_if_needed = [&](const std::string& text) -> std::string {
            if (text.empty()) {
                return {};
            }
            if (text.find('{') != std::string::npos) {
                return render_line(text, vars);
            }
            return escape_brackets_for_isocline(text);
        };

        auto render_with_fallback = [&](const std::string& value,
                                        const char* fallback) -> std::string {
            if (value.empty()) {
                return render_if_needed(std::string(fallback));
            }
            return render_if_needed(value);
        };

        auto is_whitespace_only = [](const std::string& text) {
            return std::all_of(text.begin(), text.end(),
                               [](unsigned char ch) { return std::isspace(ch); });
        };

        for (const auto& segment : bucket) {
            std::string segment_result;
            std::string content = render_if_needed(segment.content);
            std::string separator = render_if_needed(segment.separator);

            if (content.empty() || is_whitespace_only(content)) {
                continue;
            }

            if (!segment.forward_separator.empty()) {
                std::string forward_sep = render_if_needed(segment.forward_separator);
                if (!forward_sep.empty()) {
                    std::string forward_fg =
                        render_with_fallback(segment.forward_separator_fg, "RESET");
                    std::string forward_bg =
                        render_with_fallback(segment.forward_separator_bg, "RESET");

                    if (forward_fg != "RESET") {
                        out += colors::fg_color(colors::parse_color_value(forward_fg));
                    }
                    if (forward_bg != "RESET") {
                        out += colors::bg_color(colors::parse_color_value(forward_bg));
                    } else {
                        out += colors::ansi::BG_RESET;
                    }
                    out += forward_sep;
                }
            }

            std::string bg_color_name = render_with_fallback(segment.bg_color, "RESET");
            std::string fg_color_name = render_with_fallback(segment.fg_color, "RESET");
            std::string sep_fg_name = render_with_fallback(segment.separator_fg, "RESET");
            std::string sep_bg_name = render_with_fallback(segment.separator_bg, "RESET");

            std::string styled_content = content;

            std::string style_codes;
            if (segment.bold)
                style_codes += colors::ansi::BOLD;
            if (segment.italic)
                style_codes += colors::ansi::ITALIC;
            if (segment.underline)
                style_codes += colors::ansi::UNDERLINE;
            if (segment.dim)
                style_codes += colors::ansi::DIM;
            if (segment.strikethrough)
                style_codes += colors::ansi::STRIKETHROUGH;

            if (colors::is_gradient_value(bg_color_name)) {
                styled_content =
                    colors::apply_gradient_bg_with_fg(content, bg_color_name, fg_color_name);
                segment_result += style_codes + styled_content;
            } else if (colors::is_gradient_value(fg_color_name)) {
                if (bg_color_name != "RESET") {
                    segment_result += colors::bg_color(colors::parse_color_value(bg_color_name));
                } else {
                    segment_result += colors::ansi::BG_RESET;
                }
                segment_result += style_codes;
                styled_content = colors::apply_color_or_gradient(content, fg_color_name, true);
                segment_result += styled_content;
            } else {
                if (bg_color_name != "RESET") {
                    segment_result += colors::bg_color(colors::parse_color_value(bg_color_name));
                } else {
                    segment_result += colors::ansi::BG_RESET;
                }
                if (fg_color_name != "RESET") {
                    segment_result += colors::fg_color(colors::parse_color_value(fg_color_name));
                }
                segment_result += style_codes + content;
            }

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

    bool hasAlign = false;
    for (const auto& seg : segments) {
        if (!seg.alignment.empty()) {
            hasAlign = true;
            break;
        }
    }
    if (!hasAlign) {
        auto result = build(segments);
        result += colors::ansi::RESET;
        return result;
    }

    std::vector<ThemeSegment> left;
    std::vector<ThemeSegment> center;
    std::vector<ThemeSegment> right;
    for (const auto& seg : segments) {
        std::string alignment = seg.alignment.empty() ? "left" : seg.alignment;
        if (alignment == "center") {
            center.push_back(seg);
        } else if (alignment == "right") {
            right.push_back(seg);
        } else {
            left.push_back(seg);
        }
    }

    auto L = build(left);
    auto C = build(center);
    auto R = build(right);

    size_t w = get_terminal_width();
    size_t lL = calculate_raw_length(L);
    size_t lC = calculate_raw_length(C);
    size_t lR = calculate_raw_length(R);

    std::string out;

    if (isNewlineSegments) {
        out = L + C + R;
    } else if (!C.empty()) {
        size_t total_content_width = lL + lC + lR;

        if (w >= total_content_width) {
            size_t desiredCStart = (w - lC) / 2;

            size_t padL = (desiredCStart > lL) ? (desiredCStart - lL) : 0;
            size_t afterC = lL + padL + lC;
            size_t padR = (w > afterC + lR) ? (w - afterC - lR) : 0;

            std::string fillL;
            if (padL > 0) {
                if (fill_bg_color_ != "RESET") {
                    if (!colors::is_gradient_value(fill_bg_color_)) {
                        fillL += colors::bg_color(colors::parse_color_value(fill_bg_color_));
                    }
                } else {
                    fillL += colors::ansi::BG_RESET;
                }

                if (colors::is_gradient_value(fill_fg_color_) ||
                    colors::is_gradient_value(fill_bg_color_)) {
                    if (!fill_char_.empty()) {
                        std::string fill_text(padL, fill_char_[0]);
                        if (colors::is_gradient_value(fill_bg_color_)) {
                            fillL +=
                                colors::apply_color_or_gradient(fill_text, fill_bg_color_, false);
                        } else if (colors::is_gradient_value(fill_fg_color_)) {
                            fillL +=
                                colors::apply_color_or_gradient(fill_text, fill_fg_color_, true);
                        }
                    }
                } else {
                    if (fill_fg_color_ != "RESET") {
                        fillL += colors::fg_color(colors::parse_color_value(fill_fg_color_));
                    }
                    if (!fill_char_.empty()) {
                        for (size_t i = 0; i < padL; ++i) {
                            fillL += fill_char_;
                        }
                    }
                }
            }

            std::string fillR;
            if (padR > 0) {
                if (fill_bg_color_ != "RESET") {
                    if (!colors::is_gradient_value(fill_bg_color_)) {
                        fillR += colors::bg_color(colors::parse_color_value(fill_bg_color_));
                    }
                } else {
                    fillR += colors::ansi::BG_RESET;
                }

                if (colors::is_gradient_value(fill_fg_color_) ||
                    colors::is_gradient_value(fill_bg_color_)) {
                    if (!fill_char_.empty()) {
                        std::string fill_text(padR, fill_char_[0]);
                        if (colors::is_gradient_value(fill_bg_color_)) {
                            fillR +=
                                colors::apply_color_or_gradient(fill_text, fill_bg_color_, false);
                        } else if (colors::is_gradient_value(fill_fg_color_)) {
                            fillR +=
                                colors::apply_color_or_gradient(fill_text, fill_fg_color_, true);
                        }
                    }
                } else {
                    if (fill_fg_color_ != "RESET") {
                        fillR += colors::fg_color(colors::parse_color_value(fill_fg_color_));
                    }
                    if (!fill_char_.empty()) {
                        for (size_t i = 0; i < padR; ++i) {
                            fillR += fill_char_;
                        }
                    }
                }
            }

            out = L + fillL + C + fillR + R;
        } else {
            if (lL + lC + 3 < w) {
                size_t remaining = w - lL - lC - 1;
                if (remaining >= 3) {
                    out = L + C + "...";
                } else {
                    out = L + C;
                }
            } else if (lL + 3 < w) {
                size_t remaining = w - lL - 1;
                if (remaining >= 3) {
                    out = L + "...";
                } else {
                    out = L;
                }
            } else {
                out = L.substr(0, w - 3) + "...";
            }
        }
    } else {
        size_t total_content_width = lL + lR;
        size_t pad = 0;
        if (w > total_content_width) {
            pad = w - total_content_width;
        }

        std::string fill;

        if (fill_bg_color_ != "RESET") {
            if (!colors::is_gradient_value(fill_bg_color_)) {
                fill += colors::bg_color(colors::parse_color_value(fill_bg_color_));
            }
        } else {
            fill += colors::ansi::BG_RESET;
        }

        if (colors::is_gradient_value(fill_fg_color_) ||
            colors::is_gradient_value(fill_bg_color_)) {
            if (!fill_char_.empty()) {
                std::string fill_text(pad, fill_char_[0]);
                if (colors::is_gradient_value(fill_bg_color_)) {
                    fill += colors::apply_color_or_gradient(fill_text, fill_bg_color_, false);
                } else if (colors::is_gradient_value(fill_fg_color_)) {
                    fill += colors::apply_color_or_gradient(fill_text, fill_fg_color_, true);
                }
            }
        } else {
            if (fill_fg_color_ != "RESET") {
                fill += colors::fg_color(colors::parse_color_value(fill_fg_color_));
            }
            if (!fill_char_.empty()) {
                for (size_t i = 0; i < pad; ++i) {
                    fill += fill_char_;
                }
            }
        }
        if (w < total_content_width && lL < w) {
            size_t available_for_right = w - lL - 1;
            if (available_for_right > 3) {
                std::string truncated_R = "...";
                out = L + fill + truncated_R;
            } else {
                out = L;
            }
        } else {
            out = L + fill + R;
        }
    }
    out += colors::ansi::RESET;
    return out;
}

std::string Theme::get_ps1_prompt_format(
    const std::unordered_map<std::string, std::string>& vars) const {
    auto result = render_line_aligned(ps1_segments, vars);
    last_ps1_raw_length = calculate_raw_length(result);
    return result;
}

std::string Theme::get_git_prompt_format(
    const std::unordered_map<std::string, std::string>& vars) const {
    auto result = render_line_aligned(git_segments, vars);
    last_git_raw_length = calculate_raw_length(result);
    return result;
}

std::string Theme::get_newline_prompt(
    const std::unordered_map<std::string, std::string>& vars) const {
    auto result = render_line_aligned(newline_segments, vars);
    last_newline_raw_length = calculate_raw_length(result);
    return result;
}

std::string Theme::get_inline_right_prompt(
    const std::unordered_map<std::string, std::string>& vars) const {
    if (inline_right_segments.empty()) {
        return "";
    }
    auto result = render_line_aligned(inline_right_segments, vars);
    return result;
}

std::vector<std::string> Theme::list_themes() {
    std::vector<std::string> themes;

    for (const auto& entry : std::filesystem::directory_iterator(theme_directory)) {
        if (entry.is_regular_file() && entry.path().extension() == Theme::kThemeFileExtension) {
            std::string name = entry.path().stem().string();
            if (!name.empty() && name[0] != '.') {
                themes.push_back(name);
            }
        }
    }
    return themes;
}

bool Theme::uses_newline() const {
    return !newline_segments.empty();
}

bool Theme::uses_cleanup() const {
    return cleanup_;
}

bool Theme::cleanup_adds_empty_line() const {
    return cleanup_add_empty_line_;
}

bool Theme::newline_after_execution() const {
    return newline_after_execution_;
}

bool Theme::cleanup_nl_after_exec() const {
    return cleanup_nl_after_exec_;
}

void Theme::set_cleanup_nl_after_exec(bool enabled) {
    cleanup_nl_after_exec_ = enabled;
    theme_data.behavior.cleanup_nl_after_exec = enabled;
}

std::string Theme::get_terminal_title_format() const {
    return terminal_title_format;
}

std::string Theme::escape_brackets_for_isocline(const std::string& input) const {
    std::string result;
    result.reserve(input.size());

    const size_t len = input.size();
    size_t i = 0;

    auto is_numeric_placeholder = [&](const std::string& text) -> bool {
        std::string trimmed = trim(text);
        if (trimmed.empty()) {
            return false;
        }

        size_t idx = 0;
        if (trimmed[idx] == '+' || trimmed[idx] == '-') {
            ++idx;
        }

        if (idx >= trimmed.size()) {
            return false;
        }

        return std::all_of(trimmed.begin() + static_cast<std::string::difference_type>(idx),
                           trimmed.end(), [](unsigned char c) { return std::isdigit(c); });
    };

    while (i < len) {
        char ch = input[i];
        if (ch == '[') {
            bool already_escaped = (i > 0 && input[i - 1] == '\\');
            size_t closing = i + 1;
            while (closing < len && input[closing] != ']') {
                ++closing;
            }

            if (!already_escaped && closing < len) {
                std::string inside = input.substr(i + 1, closing - i - 1);
                if (is_numeric_placeholder(inside)) {
                    result.push_back('\\');
                    result.append(input, i, closing - i + 1);
                    i = closing + 1;
                    continue;
                }
            }
        }

        result.push_back(ch);
        ++i;
    }

    return result;
}

std::string Theme::process_conditionals(
    const std::string& line, const std::unordered_map<std::string, std::string>& vars) const {
    std::string result = line;
    size_t pos = 0;

    while ((pos = result.find("{if =", pos)) != std::string::npos) {
        size_t brace_count = 1;
        size_t end_pos = pos + 4;

        while (end_pos < result.length() && brace_count > 0) {
            end_pos++;
            if (result[end_pos] == '{') {
                brace_count++;
            } else if (result[end_pos] == '}') {
                brace_count--;
            }
        }

        if (brace_count != 0) {
            pos += 5;
            continue;
        }

        std::string conditional_expr = result.substr(pos + 5, end_pos - pos - 5);
        std::string replacement = evaluate_conditional(conditional_expr, vars);

        result.replace(pos, end_pos - pos + 1, replacement);
        pos += replacement.length();
    }

    return result;
}

std::string Theme::evaluate_conditional(
    const std::string& expr, const std::unordered_map<std::string, std::string>& vars) const {
    size_t question_pos = expr.find('?');
    if (question_pos == std::string::npos) {
        return "";
    }

    size_t colon_pos = expr.find(':', question_pos + 1);
    if (colon_pos == std::string::npos) {
        return "";
    }

    std::string condition = trim(expr.substr(0, question_pos));
    std::string true_value = expr.substr(question_pos + 1, colon_pos - question_pos - 1);
    std::string false_value = expr.substr(colon_pos + 1);

    bool condition_result = evaluate_condition(condition, vars);

    std::string selected_value = condition_result ? true_value : false_value;

    return render_line(selected_value, vars);
}

bool Theme::evaluate_condition(const std::string& condition,
                               const std::unordered_map<std::string, std::string>& vars) const {
    std::string trimmed_condition = trim(condition);

    if (trimmed_condition == "true") {
        return true;
    }
    if (trimmed_condition == "false") {
        return false;
    }

    if (trimmed_condition.front() == '{' && trimmed_condition.back() == '}') {
        std::string var_name = trimmed_condition.substr(1, trimmed_condition.length() - 2);
        auto it = vars.find(var_name);
        if (it != vars.end()) {
            std::string value = it->second;

            return !value.empty() && value != "0" && value != "false";
        }
        return false;
    }

    if (trimmed_condition.find("==") != std::string::npos) {
        return evaluate_comparison(trimmed_condition, "==", vars);
    }
    if (trimmed_condition.find("!=") != std::string::npos) {
        return evaluate_comparison(trimmed_condition, "!=", vars);
    }
    if (trimmed_condition.find(">=") != std::string::npos) {
        return evaluate_comparison(trimmed_condition, ">=", vars);
    }
    if (trimmed_condition.find("<=") != std::string::npos) {
        return evaluate_comparison(trimmed_condition, "<=", vars);
    }
    if (trimmed_condition.find('>') != std::string::npos) {
        return evaluate_comparison(trimmed_condition, ">", vars);
    }
    if (trimmed_condition.find('<') != std::string::npos) {
        return evaluate_comparison(trimmed_condition, "<", vars);
    }

    auto it = vars.find(trimmed_condition);
    if (it != vars.end()) {
        std::string value = it->second;
        return !value.empty() && value != "0" && value != "false";
    }

    return !trimmed_condition.empty();
}

bool Theme::evaluate_comparison(const std::string& condition, const std::string& op,
                                const std::unordered_map<std::string, std::string>& vars) const {
    size_t op_pos = condition.find(op);
    if (op_pos == std::string::npos) {
        return false;
    }

    std::string left = trim(condition.substr(0, op_pos));
    std::string right = trim(condition.substr(op_pos + op.length()));

    std::string left_value = resolve_value(left, vars);
    std::string right_value = resolve_value(right, vars);

    if (op == "==") {
        return left_value == right_value;
    }
    if (op == "!=") {
        return left_value != right_value;
    }
    if (op == ">" || op == "<" || op == ">=" || op == "<=") {
        try {
            double left_num = std::stod(left_value);
            double right_num = std::stod(right_value);

            if (op == ">")
                return left_num > right_num;
            if (op == "<")
                return left_num < right_num;
            if (op == ">=")
                return left_num >= right_num;
            if (op == "<=")
                return left_num <= right_num;
        } catch (const std::exception&) {
            if (op == ">")
                return left_value > right_value;
            if (op == "<")
                return left_value < right_value;
            if (op == ">=")
                return left_value >= right_value;
            if (op == "<=")
                return left_value <= right_value;
        }
    }

    return false;
}

std::string Theme::resolve_value(const std::string& value,
                                 const std::unordered_map<std::string, std::string>& vars) const {
    std::string trimmed = trim(value);

    if (trimmed.front() == '{' && trimmed.back() == '}') {
        std::string var_name = trimmed.substr(1, trimmed.length() - 2);
        auto it = vars.find(var_name);
        return (it != vars.end()) ? it->second : "";
    }

    if ((trimmed.front() == '"' && trimmed.back() == '"') ||
        (trimmed.front() == '\'' && trimmed.back() == '\'')) {
        return trimmed.substr(1, trimmed.length() - 2);
    }

    return trimmed;
}

std::string Theme::trim(const std::string& str) const {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::string Theme::render_line(const std::string& line,
                               const std::unordered_map<std::string, std::string>& vars) const {
    if (line.empty()) {
        return "";
    }

    std::string result = line;

    result = process_conditionals(result, vars);

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

    result = escape_brackets_for_isocline(result);
    return result;
}

void Theme::view_theme_requirements(const std::string& theme) const {
    std::filesystem::path theme_path = resolve_theme_file(theme);
    std::string theme_file = theme_path.string();

    if (!std::filesystem::exists(theme_path)) {
        print_error({ErrorType::FILE_NOT_FOUND,
                     "view_theme_requirements",
                     "Theme file '" + theme_file + "' does not exist.",
                     {"Use 'theme' to see available themes."}});
        return;
    }

    try {
        ThemeDefinition theme_def = ThemeParser::parse_file(theme_file);
        const ThemeRequirements& requirements = theme_def.requirements;

        if (!requirements.colors.empty()) {
            std::cout << "Terminal color support for " << requirements.colors << " is required."
                      << '\n';
        }

        if (!requirements.fonts.empty()) {
            std::stringstream font_req;
            font_req << "This theme works best with one of these fonts: ";
            bool first = true;

            for (const auto& font : requirements.fonts) {
                if (!first)
                    font_req << ", ";
                font_req << font;
                first = false;
            }

            std::cout << font_req.str() << '\n';
        }

        if (!requirements.custom.empty()) {
            for (const auto& [key, value] : requirements.custom) {
                std::cout << "Custom requirement: " << key << " = " << value << '\n';
            }
        }

        if (requirements.colors.empty() && requirements.fonts.empty() &&
            requirements.custom.empty()) {
            std::cout << "No specific requirements found for theme " << theme << '\n';
        }
    } catch (const ThemeParseException& e) {
        bool reported = report_theme_parse_error_with_script_reporter(e, theme_file);
        std::string message = "Failed to parse theme file '" + theme_file + "'";
        if (e.line() > 0) {
            message += " at line " + std::to_string(e.line());
        }
        if (!e.detail().empty()) {
            message += ": " + e.detail();
        }
        if (!reported) {
            if (e.error_info()) {
                ErrorInfo error = *e.error_info();
                error.type = ErrorType::SYNTAX_ERROR;
                error.command_used = "view_theme_requirements";
                error.message = message;
                if (error.suggestions.empty()) {
                    error.suggestions.push_back("Check theme syntax and try again.");
                }
                print_error(error);
            } else {
                print_error({ErrorType::SYNTAX_ERROR,
                             "view_theme_requirements",
                             message,
                             {"Check theme syntax and try again."}});
            }
        }
    } catch (const std::exception& e) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "view_theme_requirements",
                     "Failed to process theme requirements for '" + theme + "': " + e.what(),
                     {}});
    }
}

std::filesystem::path Theme::resolve_theme_file(const std::string& theme_name) const {
    std::filesystem::path theme_dir(theme_directory);
    return theme_dir / Theme::ensure_theme_extension(theme_name);
}

std::string Theme::strip_theme_extension(const std::string& theme_name) {
    const std::string ext(Theme::kThemeFileExtension);
    if (theme_name.size() >= ext.size()) {
        std::string suffix = theme_name.substr(theme_name.size() - ext.size());
        std::string suffix_lower = suffix;
        std::transform(suffix_lower.begin(), suffix_lower.end(), suffix_lower.begin(),
                       [](unsigned char ch) { return std::tolower(ch); });
        std::string ext_lower = ext;
        std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(),
                       [](unsigned char ch) { return std::tolower(ch); });
        if (suffix_lower == ext_lower) {
            return theme_name.substr(0, theme_name.size() - ext.size());
        }
    }
    return theme_name;
}

std::string Theme::ensure_theme_extension(const std::string& theme_name) {
    std::string stripped = strip_theme_extension(theme_name);
    return stripped + std::string(Theme::kThemeFileExtension);
}

bool Theme::check_theme_requirements(const ThemeRequirements& requirements) const {
    bool requirements_met = true;
    std::vector<std::string> missing_requirements;

    if (!requirements.colors.empty()) {
        std::string required_capability = requirements.colors;

        if (required_capability == "true_color" &&
            colors::g_color_capability != colors::ColorCapability::TRUE_COLOR) {
            requirements_met = false;
            missing_requirements.push_back(
                "True color (24-bit) terminal support is required. Current "
                "terminal "
                "support: " +
                colors::get_color_capability_string(colors::g_color_capability));
        } else if (required_capability == "256_color" &&
                   colors::g_color_capability < colors::ColorCapability::XTERM_256_COLOR) {
            requirements_met = false;
            missing_requirements.push_back(
                "256-color terminal support is required. Current terminal "
                "support: " +
                colors::get_color_capability_string(colors::g_color_capability));
        }
    }

    if (!requirements.custom.empty()) {
        for (const auto& [key, value] : requirements.custom) {
            std::cout << "Custom requirement: " << key << " = " << value << '\n';
        }
    }

    if (!requirements_met) {
        print_error({ErrorType::RUNTIME_ERROR, "load_theme", "Theme requirements not met.",
                     missing_requirements});
    }

    return requirements_met;
}

size_t Theme::calculate_raw_length(const std::string& str) const {
    size_t ansi_chars = 0;
    size_t visible_chars = 0;

    std::string str_without_isocline_escapes;
    str_without_isocline_escapes.reserve(str.size());

    auto is_numeric_placeholder = [&](const std::string& text) -> bool {
        std::string trimmed = trim(text);
        if (trimmed.empty()) {
            return false;
        }

        size_t idx = 0;
        if (trimmed[idx] == '+' || trimmed[idx] == '-') {
            ++idx;
        }

        if (idx >= trimmed.size()) {
            return false;
        }

        return std::all_of(trimmed.begin() + static_cast<std::string::difference_type>(idx),
                           trimmed.end(), [](unsigned char c) { return std::isdigit(c); });
    };

    for (size_t i = 0; i < str.size(); ++i) {
        char ch = str[i];
        if (ch == '\\' && i + 1 < str.size() && str[i + 1] == '[') {
            size_t closing = i + 2;
            while (closing < str.size() && str[closing] != ']') {
                ++closing;
            }

            if (closing < str.size()) {
                std::string inside = str.substr(i + 2, closing - (i + 2));
                if (is_numeric_placeholder(inside)) {
                    str_without_isocline_escapes.push_back('[');
                    str_without_isocline_escapes.append(inside);
                    str_without_isocline_escapes.push_back(']');
                    i = closing;
                    continue;
                }
            }
        }

        str_without_isocline_escapes.push_back(ch);
    }

    size_t raw_length = utf8_utils::calculate_display_width(str_without_isocline_escapes,
                                                            &ansi_chars, &visible_chars);
    return raw_length;
}

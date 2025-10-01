#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "theme_parser.h"

class Theme {
   private:
    std::string theme_directory;
    bool is_enabled;
    void create_default_theme();

    std::string terminal_title_format;
    bool check_theme_requirements(const ThemeRequirements& requirements) const;

    mutable size_t last_ps1_raw_length = 0;
    mutable size_t last_git_raw_length = 0;
    mutable size_t last_ai_raw_length = 0;
    mutable size_t last_newline_raw_length = 0;

    std::string process_conditionals(
        const std::string& line,
        const std::unordered_map<std::string, std::string>& vars) const;

    std::string evaluate_conditional(
        const std::string& expr,
        const std::unordered_map<std::string, std::string>& vars) const;

    bool evaluate_condition(
        const std::string& condition,
        const std::unordered_map<std::string, std::string>& vars) const;

    bool evaluate_comparison(
        const std::string& condition, const std::string& op,
        const std::unordered_map<std::string, std::string>& vars) const;

    std::string resolve_value(
        const std::string& value,
        const std::unordered_map<std::string, std::string>& vars) const;

    std::string trim(const std::string& str) const;

    std::string escape_brackets_for_isocline(const std::string& input) const;

    size_t calculate_raw_length(const std::string& str) const;
    size_t get_terminal_width() const;
    std::string render_line_aligned(
        const std::vector<ThemeSegment>& segments,
        const std::unordered_map<std::string, std::string>& vars) const;

        std::filesystem::path resolve_theme_file(
            const std::string& theme_name) const;

    bool apply_theme_definition(const ThemeDefinition& definition,
                                const std::string& theme_name,
                                bool allow_fallback,
                                const std::filesystem::path& source_path);

    std::string fill_char_{""};
    std::string fill_fg_color_{"RESET"};
    std::string fill_bg_color_{"RESET"};
    bool cleanup_{false};
    bool cleanup_add_empty_line_{false};
    bool newline_after_execution_{false};

   public:
    Theme(std::string theme_dir, bool enabled);
    ~Theme();

    ThemeDefinition theme_data;
    
    std::vector<ThemeSegment>& ps1_segments = theme_data.ps1_segments;
    std::vector<ThemeSegment>& git_segments = theme_data.git_segments;
    std::vector<ThemeSegment>& ai_segments = theme_data.ai_segments;
    std::vector<ThemeSegment>& newline_segments = theme_data.newline_segments;
    std::vector<ThemeSegment>& inline_right_segments = theme_data.inline_right_segments;

    bool load_theme(const std::string& theme_name, bool allow_fallback);
    bool load_theme_from_path(const std::filesystem::path& file_path,
                              bool allow_fallback);
    std::vector<std::string> list_themes();
    void view_theme_requirements(const std::string& theme) const;

    bool uses_newline() const;

    std::string get_terminal_title_format() const;

    size_t get_ps1_raw_length() const {
        return last_ps1_raw_length;
    }
    size_t get_git_raw_length() const {
        return last_git_raw_length;
    }
    size_t get_ai_raw_length() const {
        return last_ai_raw_length;
    }
    size_t get_newline_raw_length() const {
        return last_newline_raw_length;
    }

    std::string get_newline_prompt(
        const std::unordered_map<std::string, std::string>& vars) const;
    std::string get_ps1_prompt_format(
        const std::unordered_map<std::string, std::string>& vars) const;
    std::string get_git_prompt_format(
        const std::unordered_map<std::string, std::string>& vars) const;
    std::string get_ai_prompt_format(
        const std::unordered_map<std::string, std::string>& vars) const;
    std::string get_inline_right_prompt(
        const std::unordered_map<std::string, std::string>& vars) const;

    bool get_enabled() const {
        return is_enabled;
    }

    bool uses_cleanup() const;
    bool cleanup_adds_empty_line() const;
    bool newline_after_execution() const;

    // Public method for testing conditional functionality
    std::string render_line(
        const std::string& line,
        const std::unordered_map<std::string, std::string>& vars) const;

    static inline constexpr std::string_view kThemeFileExtension = ".cjsh";
    static std::string ensure_theme_extension(const std::string& theme_name);
    static std::string strip_theme_extension(const std::string& theme_name);
};
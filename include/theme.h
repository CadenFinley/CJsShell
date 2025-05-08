#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

class Theme {
 private:
  std::string theme_directory;
  bool is_enabled;
  void create_default_theme();

  std::string terminal_title_format;

  void prerender_segments();
  std::string prerender_line(const std::vector<nlohmann::json>& segments) const;

  std::string prerendered_ps1_format;
  std::string prerendered_git_format;
  std::string prerendered_ai_format;
  std::string prerendered_newline_format;

  mutable size_t last_ps1_raw_length = 0;
  mutable size_t last_git_raw_length = 0;
  mutable size_t last_ai_raw_length = 0;
  mutable size_t last_newline_raw_length = 0;

  std::string render_line(
      const std::string& line,
      const std::unordered_map<std::string, std::string>& vars) const;

  size_t calculate_raw_length(const std::string& str) const;
  size_t get_terminal_width() const;
  std::string prerender_line_aligned(
      const std::vector<nlohmann::json>& segments) const;
  std::string render_line_aligned(
      const std::vector<nlohmann::json>& segments,
      const std::unordered_map<std::string, std::string>& vars) const;

  std::string fill_char_{" "};
  std::string fill_fg_color_{"RESET"};
  std::string fill_bg_color_{"RESET"};

 public:
  Theme(std::string theme_dir, bool enabled);
  ~Theme();

  std::vector<nlohmann::json> ps1_segments;
  std::vector<nlohmann::json> git_segments;
  std::vector<nlohmann::json> ai_segments;
  std::vector<nlohmann::json> newline_segments;

  bool load_theme(const std::string& theme_name);
  std::vector<std::string> list_themes();

  bool uses_newline() const;

  std::string get_terminal_title_format() const;

  std::string get_rendered_ps1_format() const { return prerendered_ps1_format; }
  std::string get_rendered_git_format() const { return prerendered_git_format; }
  std::string get_rendered_ai_format() const { return prerendered_ai_format; }
  std::string get_rendered_newline_format() const {
    return prerendered_newline_format;
  }

  size_t get_ps1_raw_length() const { return last_ps1_raw_length; }
  size_t get_git_raw_length() const { return last_git_raw_length; }
  size_t get_ai_raw_length() const { return last_ai_raw_length; }
  size_t get_newline_raw_length() const { return last_newline_raw_length; }

  std::string get_newline_prompt(
      const std::unordered_map<std::string, std::string>& vars) const;
  std::string get_ps1_prompt_format(
      const std::unordered_map<std::string, std::string>& vars) const;
  std::string get_git_prompt_format(
      const std::unordered_map<std::string, std::string>& vars) const;
  std::string get_ai_prompt_format(
      const std::unordered_map<std::string, std::string>& vars) const;

  bool get_enabled() const { return is_enabled; }
};
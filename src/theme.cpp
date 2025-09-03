#include "theme.h"

#include <sys/ioctl.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>

#include "cjsh.h"
#include "colors.h"

Theme::Theme(std::string theme_dir, bool enabled)
    : theme_directory(theme_dir), is_enabled(enabled) {
  if (!std::filesystem::exists(theme_directory + "/default.json")) {
    create_default_theme();
  }
  is_enabled = enabled;
}

Theme::~Theme() {}

void Theme::create_default_theme() {
  nlohmann::json default_theme;

  default_theme["terminal_title"] = "{PATH}";
  default_theme["requirements"] = nlohmann::json::object();
  default_theme["ps1_segments"] = nlohmann::json::array();
  default_theme["ps1_segments"].push_back(
      {{"tag", "username"},
       {"content", "{USERNAME}@{HOSTNAME}:"},
       {"bg_color", "RESET"},
       {"fg_color", "#5555FF"},
       {"separator", ""},
       {"separator_fg", "RESET"},
       {"separator_bg", "RESET"}});
  default_theme["ps1_segments"].push_back({{"tag", "directory"},
                                           {"content", " {DIRECTORY} "},
                                           {"bg_color", "RESET"},
                                           {"fg_color", "#55FF55"},
                                           {"separator", " "},
                                           {"separator_fg", "#FFFFFF"},
                                           {"separator_bg", "RESET"}});
  default_theme["ps1_segments"].push_back({{"tag", "prompt"},
                                           {"content", "$ "},
                                           {"bg_color", "RESET"},
                                           {"fg_color", "#FFFFFF"},
                                           {"separator", ""},
                                           {"separator_fg", "RESET"},
                                           {"separator_bg", "RESET"}});

  default_theme["git_segments"] = nlohmann::json::array();
  default_theme["git_segments"].push_back({{"tag", "path"},
                                           {"content", " {LOCAL_PATH} "},
                                           {"bg_color", "RESET"},
                                           {"fg_color", "#55FF55"},
                                           {"separator", " "},
                                           {"separator_fg", "#FFFFFF"},
                                           {"separator_bg", "RESET"}});
  default_theme["git_segments"].push_back({{"tag", "branch"},
                                           {"content", "{GIT_BRANCH}"},
                                           {"bg_color", "RESET"},
                                           {"fg_color", "#FFFF55"},
                                           {"separator", ""},
                                           {"separator_fg", "RESET"},
                                           {"separator_bg", "RESET"}});
  default_theme["git_segments"].push_back({{"tag", "status"},
                                           {"content", "{GIT_STATUS}"},
                                           {"bg_color", "RESET"},
                                           {"fg_color", "#FF5555"},
                                           {"separator", " $ "},
                                           {"separator_fg", "#FFFFFF"},
                                           {"separator_bg", "RESET"}});

  default_theme["ai_segments"] = nlohmann::json::array();
  default_theme["ai_segments"].push_back({{"tag", "model"},
                                          {"content", " {AI_MODEL} "},
                                          {"bg_color", "RESET"},
                                          {"fg_color", "#FF55FF"},
                                          {"separator", " / "},
                                          {"separator_fg", "#FFFFFF"},
                                          {"separator_bg", "RESET"}});
  default_theme["ai_segments"].push_back({{"tag", "mode"},
                                          {"content", "{AI_AGENT_TYPE} "},
                                          {"bg_color", "RESET"},
                                          {"fg_color", "#55FFFF"},
                                          {"separator", ""},
                                          {"separator_fg", "RESET"},
                                          {"separator_bg", "RESET"}});

  default_theme["newline_segments"] = nlohmann::json::array();
  default_theme["fill_char"] = "";
  default_theme["fill_fg_color"] = "RESET";
  default_theme["fill_bg_color"] = "RESET";

  std::ofstream file(theme_directory + "/default.json");
  file << default_theme.dump(4);
  file.close();
}

bool Theme::load_theme(const std::string& theme_name) {
  std::string theme_name_to_use = theme_name;
  if (!is_enabled) {
    theme_name_to_use = "default";
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: Loading theme '" << theme_name_to_use
              << "', startup_active=" << (g_startup_active ? "true" : "false")
              << std::endl;
  }

  std::string theme_file = theme_directory + "/" + theme_name_to_use + ".json";

  if (!std::filesystem::exists(theme_file)) {
    return false;
  }

  std::ifstream file(theme_file);
  nlohmann::json theme_json;
  file >> theme_json;
  file.close();

  if (theme_json.contains("requirements") &&
      theme_json["requirements"].is_object() &&
      !theme_json["requirements"].empty()) {
    if (!check_theme_requirements(theme_json["requirements"])) {
      std::cerr << "Theme '" << theme_name_to_use
                << "' requirements not met, falling back to previous theme: '"
                << g_current_theme << "'" << std::endl;
      if (theme_name_to_use != g_current_theme) {
        return load_theme(g_current_theme);
      } else {
        if (theme_name_to_use != "default") {
          std::cerr << "Falling back to default theme" << std::endl;
          return load_theme("default");
        }
        return false;
      }
    }
  }

  ps1_segments.clear();
  git_segments.clear();
  ai_segments.clear();
  newline_segments.clear();

  if (theme_json.contains("ps1_segments") &&
      theme_json["ps1_segments"].is_array()) {
    ps1_segments = theme_json["ps1_segments"];
  }

  if (theme_json.contains("git_segments") &&
      theme_json["git_segments"].is_array()) {
    git_segments = theme_json["git_segments"];
  }

  if (theme_json.contains("ai_segments") &&
      theme_json["ai_segments"].is_array()) {
    ai_segments = theme_json["ai_segments"];
  }

  if (theme_json.contains("newline_segments") &&
      theme_json["newline_segments"].is_array()) {
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

  if (has_duplicate_tags(ps1_segments) || has_duplicate_tags(git_segments) ||
      has_duplicate_tags(ai_segments) || has_duplicate_tags(newline_segments)) {
    return false;
  }

  if (theme_json.contains("terminal_title")) {
    terminal_title_format = theme_json["terminal_title"];
  }

  if (theme_json.contains("fill_char") && theme_json["fill_char"].is_string()) {
    auto s = theme_json["fill_char"].get<std::string>();
    if (!s.empty()) fill_char_ = s;
  }
  if (theme_json.contains("fill_fg_color") &&
      theme_json["fill_fg_color"].is_string()) {
    fill_fg_color_ = theme_json["fill_fg_color"].get<std::string>();
  }
  if (theme_json.contains("fill_bg_color") &&
      theme_json["fill_bg_color"].is_string()) {
    fill_bg_color_ = theme_json["fill_bg_color"].get<std::string>();
  }
  return true;
}

std::string Theme::prerender_line(
    const std::vector<nlohmann::json>& segments) const {
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

    if (segment.contains("forward_separator") &&
        !segment["forward_separator"].empty()) {
      std::string forward_separator = segment["forward_separator"];
      std::string forward_separator_fg_name =
          segment.value("forward_separator_fg", "RESET");
      std::string forward_separator_bg_name =
          segment.value("forward_separator_bg", "RESET");

      if (forward_separator_bg_name != "RESET") {
        auto fw_sep_bg_rgb =
            colors::parse_color_value(forward_separator_bg_name);
        segment_result += colors::bg_color(fw_sep_bg_rgb);
      } else {
        segment_result += colors::ansi::BG_RESET;
      }

      if (forward_separator_fg_name != "RESET") {
        auto fw_sep_fg_rgb =
            colors::parse_color_value(forward_separator_fg_name);
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

size_t Theme::get_terminal_width() const {
  struct winsize w;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
    if (g_debug_mode) {
      std::cout << "Detected terminal width: " << w.ws_col << " columns"
                << std::endl;
    }
    return w.ws_col;
    // return w.ws_col > 2 ? w.ws_col - 1 : w.ws_col;
  }

  if (g_debug_mode) {
    std::cout
        << "Terminal width detection failed, using default width: 80 columns"
        << std::endl;
  }
  return 80;
}

std::string Theme::render_line_aligned(
    const std::vector<nlohmann::json>& segments,
    const std::unordered_map<std::string, std::string>& vars) const {
  if (segments.empty()) return "";

  bool isNewlineSegments = (&segments == &newline_segments);

  bool hasAlign = false;
  for (auto& seg : segments)
    if (seg.contains("align")) {
      hasAlign = true;
      break;
    }
  if (!hasAlign) {
    std::string flat = prerender_line(segments);
    return render_line(flat, vars);
  }

  std::vector<nlohmann::json> left, center, right;
  for (auto& seg : segments) {
    auto a = seg.value("align", "left");
    if (a == "center")
      center.push_back(seg);
    else if (a == "right")
      right.push_back(seg);
    else
      left.push_back(seg);
  }

  auto build = [&](const std::vector<nlohmann::json>& bucket) {
    std::string out;
    for (auto& segment : bucket) {
      std::string segment_result;
      std::string content = render_line(segment.value("content", ""), vars);
      std::string separator = render_line(segment.value("separator", ""), vars);

      std::string bg_color_name = segment.value("bg_color", "RESET");
      std::string fg_color_name = segment.value("fg_color", "RESET");
      std::string sep_fg_name = segment.value("separator_fg", "RESET");
      std::string sep_bg_name = segment.value("separator_bg", "RESET");

      if (segment.contains("forward_separator") &&
          !segment["forward_separator"].empty()) {
        std::string fsep = segment["forward_separator"];
        std::string fsep_fg = segment.value("forward_separator_fg", "RESET");
        std::string fsep_bg = segment.value("forward_separator_bg", "RESET");
        if (fsep_bg != "RESET") {
          segment_result +=
              colors::bg_color(colors::parse_color_value(fsep_bg));
        } else {
          segment_result += colors::ansi::BG_RESET;
        }
        if (fsep_fg != "RESET") {
          segment_result +=
              colors::fg_color(colors::parse_color_value(fsep_fg));
        }
        segment_result += fsep;
      }

      if (bg_color_name != "RESET") {
        segment_result +=
            colors::bg_color(colors::parse_color_value(bg_color_name));
      } else {
        segment_result += colors::ansi::BG_RESET;
      }
      if (fg_color_name != "RESET") {
        segment_result +=
            colors::fg_color(colors::parse_color_value(fg_color_name));
      }
      segment_result += content;
      if (!separator.empty()) {
        if (sep_fg_name != "RESET") {
          segment_result +=
              colors::fg_color(colors::parse_color_value(sep_fg_name));
        }
        if (sep_bg_name != "RESET") {
          segment_result +=
              colors::bg_color(colors::parse_color_value(sep_bg_name));
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
          fillL += colors::bg_color(colors::parse_color_value(fill_bg_color_));
        } else {
          fillL += colors::ansi::BG_RESET;
        }

        if (fill_fg_color_ != "RESET") {
          fillL += colors::fg_color(colors::parse_color_value(fill_fg_color_));
        }

        for (size_t i = 0; i < padL; ++i) {
          fillL += fill_char_;
        }
      }

      std::string fillR;
      if (padR > 0) {
        if (fill_bg_color_ != "RESET") {
          fillR += colors::bg_color(colors::parse_color_value(fill_bg_color_));
        } else {
          fillR += colors::ansi::BG_RESET;
        }

        if (fill_fg_color_ != "RESET") {
          fillR += colors::fg_color(colors::parse_color_value(fill_fg_color_));
        }

        for (size_t i = 0; i < padR; ++i) {
          fillR += fill_char_;
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

      if (g_debug_mode) {
        std::cout << "Raw pad calculation: " << pad << " chars" << std::endl;
      }
    }

    std::string fill;

    if (fill_bg_color_ != "RESET") {
      fill += colors::bg_color(colors::parse_color_value(fill_bg_color_));
    } else {
      fill += colors::ansi::BG_RESET;
    }

    if (fill_fg_color_ != "RESET") {
      fill += colors::fg_color(colors::parse_color_value(fill_fg_color_));
    }

    for (size_t i = 0; i < pad; ++i) {
      fill += fill_char_;
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
  if (g_debug_mode) {
    std::cout << "\nCombined render:\n" << out << std::endl;
    std::cout << "Terminal width: " << w << " chars" << std::endl;
    std::cout << "Left segments width: " << lL << " chars" << std::endl;
    std::cout << "Center segments width: " << lC << " chars" << std::endl;
    std::cout << "Right segments width: " << lR << " chars" << std::endl;
    std::cout << "Total content width: " << (lL + lC + lR) << " chars"
              << std::endl;
    std::cout << "Final rendered width: " << calculate_raw_length(out)
              << " chars" << std::endl;
  }
  return out;
}

std::string Theme::get_ps1_prompt_format(
    const std::unordered_map<std::string, std::string>& vars) const {
  auto result = render_line_aligned(ps1_segments, vars);
  last_ps1_raw_length = calculate_raw_length(result);
  if (g_debug_mode)
    std::cout << "Last PS1 raw length: " << last_ps1_raw_length << std::endl;
  return result;
}

std::string Theme::get_git_prompt_format(
    const std::unordered_map<std::string, std::string>& vars) const {
  auto result = render_line_aligned(git_segments, vars);
  last_git_raw_length = calculate_raw_length(result);
  if (g_debug_mode)
    std::cout << "Last Git raw length: " << last_git_raw_length << std::endl;
  return result;
}

std::string Theme::get_ai_prompt_format(
    const std::unordered_map<std::string, std::string>& vars) const {
  auto result = render_line_aligned(ai_segments, vars);
  last_ai_raw_length = calculate_raw_length(result);
  if (g_debug_mode)
    std::cout << "Last AI raw length: " << last_ai_raw_length << std::endl;
  return result;
}

std::string Theme::get_newline_prompt(
    const std::unordered_map<std::string, std::string>& vars) const {
  auto result = render_line_aligned(newline_segments, vars);
  last_newline_raw_length = calculate_raw_length(result);
  if (g_debug_mode)
    std::cout << "Last newline raw length: " << last_newline_raw_length
              << std::endl;
  return result;
}

std::vector<std::string> Theme::list_themes() {
  std::vector<std::string> themes;

  for (const auto& entry :
       std::filesystem::directory_iterator(theme_directory)) {
    if (entry.path().extension() == ".json") {
      themes.push_back(entry.path().stem().string());
    }
  }
  return themes;
}

bool Theme::uses_newline() const { return !newline_segments.empty(); }

std::string Theme::get_terminal_title_format() const {
  return terminal_title_format;
}

std::string Theme::render_line(
    const std::string& line,
    const std::unordered_map<std::string, std::string>& vars) const {
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

    std::string placeholder =
        result.substr(start_pos + 1, end_pos - start_pos - 1);
    auto it = vars.find(placeholder);
    if (it != vars.end()) {
      result.replace(start_pos, end_pos - start_pos + 1, it->second);
      start_pos += it->second.length();
    } else {
      start_pos = end_pos + 1;
    }
  }

  if (g_debug_mode) {
    std::cout << "Rendered line: \n" << result << std::endl;
  }
  return result;
}

bool Theme::check_theme_requirements(const nlohmann::json& requirements) const {
  bool requirements_met = true;
  std::vector<std::string> missing_requirements;

  if (requirements.contains("plugins") && requirements["plugins"].is_array()) {
    for (const auto& plugin_name : requirements["plugins"]) {
      if (plugin_name.is_string()) {
        std::string name = plugin_name.get<std::string>();

        bool plugin_enabled = false;
        if (g_plugin != nullptr) {
          auto enabled_plugins = g_plugin->get_enabled_plugins();
          plugin_enabled =
              std::find(enabled_plugins.begin(), enabled_plugins.end(), name) !=
              enabled_plugins.end();
        }

        if (!plugin_enabled) {
          if (g_plugin->get_enabled()) {
            if (!g_plugin->enable_plugin(name)) {
              auto available_plugins = g_plugin->get_available_plugins();
              requirements_met = false;
              if ((std::find(available_plugins.begin(), available_plugins.end(),
                             name) == available_plugins.end())) {
                missing_requirements.push_back("Plugin '" + name +
                                               "' is required but not found");
              } else {
                missing_requirements.push_back("Plugin '" + name +
                                               "' is required but not enabled");
              }
            }
          } else {
            requirements_met = false;
            missing_requirements.push_back("Plugin system is disabled");
          }
        }
      }
    }
  }

  if (requirements.contains("colors") && requirements["colors"].is_string()) {
    std::string required_capability = requirements["colors"].get<std::string>();

    if (required_capability == "true_color" &&
        colors::g_color_capability != colors::ColorCapability::TRUE_COLOR) {
      requirements_met = false;
      missing_requirements.push_back(
          "True color (24-bit) terminal support is required");
    } else if (required_capability == "256_color" &&
               colors::g_color_capability <
                   colors::ColorCapability::XTERM_256_COLOR) {
      requirements_met = false;
      missing_requirements.push_back("256-color terminal support is required");
    }
  }

  if (requirements.contains("fonts") && requirements["fonts"].is_array()) {
    std::stringstream font_req;
    font_req << "This theme works best with one of these fonts: ";
    bool first = true;

    for (const auto& font : requirements["fonts"]) {
      if (font.is_string()) {
        if (!first) font_req << ", ";
        font_req << font.get<std::string>();
        first = false;
      }
    }

    std::cout << font_req.str() << std::endl;
  }

  if (requirements.contains("custom") && requirements["custom"].is_object()) {
    for (auto it = requirements["custom"].begin();
         it != requirements["custom"].end(); ++it) {
      std::string requirement_name = it.key();
      if (it.value().is_string()) {
        std::string requirement_value = it.value().get<std::string>();
        std::cout << "Custom requirement: " << requirement_name << " = "
                  << requirement_value << std::endl;
      }
    }
  }

  // Output all missing requirements
  if (!requirements_met) {
    std::cerr << "Theme requirements not met:" << std::endl;
    for (const auto& req : missing_requirements) {
      std::cerr << " - " << req << std::endl;
    }
  }

  return requirements_met;
}

size_t Theme::calculate_raw_length(const std::string& str) const {
  size_t raw_length = 0;

  size_t visible_chars = 0;
  size_t ansi_chars = 0;

  for (size_t i = 0; i < str.size();) {
    unsigned char c = static_cast<unsigned char>(str[i]);

    if (c == '\033') {
      ansi_chars++;
      if (i + 1 < str.size()) {
        unsigned char next = static_cast<unsigned char>(str[i + 1]);
        if (next == '[') {
          i += 2;
          ansi_chars += 2;
          while (i < str.size() && !(str[i] >= '@' && str[i] <= '~')) {
            ++i;
            ansi_chars++;
          }
          if (i < str.size()) {
            ++i;
            ansi_chars++;
          }
          continue;
        } else if (next == ']') {
          i += 2;
          ansi_chars += 2;
          while (i < str.size()) {
            if (str[i] == '\a') {
              ++i;
              ansi_chars++;
              break;
            }
            if (str[i] == '\033' && i + 1 < str.size() && str[i + 1] == '\\') {
              i += 2;
              ansi_chars += 2;
              break;
            }
            ++i;
            ansi_chars++;
          }
          continue;
        } else {
          i += 2;
          ansi_chars += 2;
          continue;
        }
      }
      ++i;
      ansi_chars++;
    } else if ((c & 0xF8) == 0xF0) {
      // Handle emoji and other characters in the supplemental planes (U+10000
      // to U+10FFFF) Most emoji are in this range and take up 2 character
      // cells
      raw_length += 2;
      visible_chars++;

      // Check for emoji modifiers, ZWJ sequences, and variation selectors
      size_t original_i = i;
      i += 4;  // Move past the base character (4 bytes)

      // Check for emoji modifiers (skin tones) which are 4 bytes
      if (i + 3 < str.size() && (unsigned char)str[i] == 0xF0 &&
          (unsigned char)str[i + 1] == 0x9F &&
          (unsigned char)str[i + 2] == 0x8F &&
          ((unsigned char)str[i + 3] >= 0xBB &&
           (unsigned char)str[i + 3] <= 0xBF)) {
        i += 4;
      }

      // Check for ZWJ (Zero Width Joiner) sequences - emoji composed with ZWJ
      while (i + 2 < str.size() && (unsigned char)str[i] == 0xE2 &&
             (unsigned char)str[i + 1] == 0x80 &&
             (unsigned char)str[i + 2] == 0x8D) {
        i += 3;  // Skip over ZWJ

        // Skip the next character in the sequence (could be 3 or 4 bytes)
        if (i < str.size()) {
          unsigned char next_c = (unsigned char)str[i];
          if ((next_c & 0xF8) == 0xF0) {
            i += 4;  // 4-byte character
          } else if ((next_c & 0xF0) == 0xE0) {
            i += 3;  // 3-byte character
          } else if ((next_c & 0xE0) == 0xC0) {
            i += 2;  // 2-byte character
          } else {
            i += 1;  // 1-byte character
          }
        }
      }

      // Check for variation selectors (VS15, VS16 - text/emoji style)
      if (i + 2 < str.size()) {
        unsigned char vs1 = (unsigned char)str[i];
        unsigned char vs2 = (unsigned char)str[i + 1];
        unsigned char vs3 = (unsigned char)str[i + 2];

        if (vs1 == 0xEF && vs2 == 0xB8 && (vs3 == 0x8E || vs3 == 0x8F)) {
          // These are variation selectors (VS15/VS16) which modify display
          // but don't add width
          i += 3;
        }
      }

      // If we've advanced past modifiers, we still count it as a single emoji
      if (i > original_i + 4) {
        // We already added the width, so don't add more
      }
    } else if ((c & 0xF0) == 0xE0) {
      if (i + 2 < str.size()) {
        unsigned char c1 = static_cast<unsigned char>(str[i + 1]);
        unsigned char c2 = static_cast<unsigned char>(str[i + 2]);

        // Check for specific emoji in the 3-byte range
        bool is_emoji =
            (c == 0xE2 && c1 == 0x9C && c2 >= 0x85 &&
             c2 <= 0x88) ||  // clock emoji
            (c == 0xE2 && c1 == 0x98 && c2 >= 0x80 &&
             c2 <= 0x8F) ||  // weather symbols
            (c == 0xE2 && c1 == 0x9D && c2 >= 0x84 &&
             c2 <= 0x8C) ||  // geometric shapes
            (c == 0xE2 && c1 == 0x9A &&
             c2 == 0xA1) ||  // lightning bolt ⚡ (U+26A1)
            (c == 0xE2 && ((c1 == 0x9A || c1 == 0x9B) && c2 >= 0x80 &&
                           c2 <= 0xBF));  // miscellaneous symbols

        // Check for wide CJK characters
        bool is_wide =
            (c == 0xE3 && c1 >= 0x80 && c1 <= 0xBF) ||
            (c == 0xE4 && c1 >= 0xB8) ||
            (c == 0xE5 || c == 0xE6 || c == 0xE7 || c == 0xE8 || c == 0xE9);

        raw_length += (is_wide || is_emoji ? 2 : 1);
      } else {
        raw_length += 1;
      }
      visible_chars++;
      i += 3;
    } else if ((c & 0xE0) == 0xC0) {
      raw_length += 1;
      visible_chars++;
      i += 2;
    } else {
      if ((c & 0xC0) != 0x80) {
        raw_length++;
        visible_chars++;
      }
      ++i;
    }
  }

  if (g_debug_mode) {
    std::cout << "String length: " << str.size()
              << " bytes, Visible chars: " << visible_chars
              << ", ANSI chars: " << ansi_chars
              << ", Raw display width: " << raw_length << std::endl;
  }

  return raw_length;
}
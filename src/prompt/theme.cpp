#include "theme.h"

#include <sys/ioctl.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <unordered_set>

#include "utils/utf8_utils.h"

#include "cjsh.h"
#include "colors.h"
#include "error_out.h"

Theme::Theme(std::string theme_dir, bool enabled)
    : theme_directory(theme_dir), is_enabled(enabled) {
  if (!std::filesystem::exists(theme_directory + "/default.json")) {
    create_default_theme();
  }
  is_enabled = enabled;
}

Theme::~Theme() {
}

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

bool Theme::load_theme(const std::string& theme_name, bool allow_fallback) {
  std::string theme_name_to_use = theme_name;
  if (!is_enabled || theme_name_to_use == "") {
    theme_name_to_use = "default";
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: Loading theme '" << theme_name_to_use
              << "', startup_active=" << (g_startup_active ? "true" : "false")
              << std::endl;
  }

  std::string theme_file = theme_directory + "/" + theme_name_to_use + ".json";

  if (!std::filesystem::exists(theme_file)) {
    print_error({ErrorType::FILE_NOT_FOUND,
                 "load_theme",
                 "Theme file '" + theme_file + "' does not exist.",
                 {"Use 'theme' to see available themes."}});
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
      if (!allow_fallback) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "load_theme",
                     "Theme '" + theme_name_to_use +
                         "' requirements not met, cannot load theme.",
                     {}});
        return false;
      }
      std::string previous_theme =
          (g_current_theme == "" ? "default" : g_current_theme);
      print_error(
          {ErrorType::RUNTIME_ERROR,
           "load_theme",
           "Theme '" + theme_name_to_use +
               "' requirements not met, falling back to previous theme: '" +
               previous_theme + "'.",
           {}});
      if (theme_name_to_use != previous_theme) {
        return load_theme(previous_theme, allow_fallback);
      } else {
        if (theme_name_to_use != "default") {
          std::cerr << "Falling back to default theme" << std::endl;
          return load_theme("default", allow_fallback);
        }
        print_error({ErrorType::FILE_NOT_FOUND,
                     "load_theme",
                     "Theme file '" + theme_file + "' does not exist.",
                     {"Use 'theme' to see available themes."}});
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
    print_error({ErrorType::SYNTAX_ERROR,
                 "load_theme",
                 "Duplicate tags found in theme segments.",
                 {"Ensure all segment tags are unique within their section."}});
    return false;
  }

  if (theme_json.contains("terminal_title")) {
    terminal_title_format = theme_json["terminal_title"];
  }

  if (theme_json.contains("fill_char") && theme_json["fill_char"].is_string()) {
    auto s = theme_json["fill_char"].get<std::string>();
    if (!s.empty())
      fill_char_ = s;
  }
  if (theme_json.contains("fill_fg_color") &&
      theme_json["fill_fg_color"].is_string()) {
    fill_fg_color_ = theme_json["fill_fg_color"].get<std::string>();
  }
  if (theme_json.contains("fill_bg_color") &&
      theme_json["fill_bg_color"].is_string()) {
    fill_bg_color_ = theme_json["fill_bg_color"].get<std::string>();
  }
  g_current_theme = theme_name_to_use;
  return true;
}

size_t Theme::get_terminal_width() const {
  struct winsize w;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
    if (g_debug_mode) {
      std::cout << "Detected terminal width: " << w.ws_col << " columns"
                << std::endl;
    }
    return w.ws_col;
  }

  if (g_debug_mode) {
    print_error({ErrorType::RUNTIME_ERROR,
                 "get_terminal_width",
                 "Failed to detect terminal width, defaulting to 80 columns.",
                 {}});
  }
  return 80;
}

std::string Theme::render_line_aligned(
    const std::vector<nlohmann::json>& segments,
    const std::unordered_map<std::string, std::string>& vars) const {
  if (segments.empty())
    return "";

  bool isNewlineSegments = (&segments == &newline_segments);

  auto build = [&](const std::vector<nlohmann::json>& bucket) {
    std::string out;
    for (auto& segment : bucket) {
      std::string segment_result;
      std::string content = render_line(segment.value("content", ""), vars);
      std::string separator = render_line(segment.value("separator", ""), vars);

      if (content.empty() || trim(content).empty()) {
        continue;
      }

      std::string bg_color_name = render_line(segment.value("bg_color", "RESET"), vars);
      std::string fg_color_name = render_line(segment.value("fg_color", "RESET"), vars);
      std::string sep_fg_name = render_line(segment.value("separator_fg", "RESET"), vars);
      std::string sep_bg_name = render_line(segment.value("separator_bg", "RESET"), vars);

      if (segment.contains("forward_separator") &&
          !segment["forward_separator"].empty()) {
        std::string fsep = segment["forward_separator"];
        std::string fsep_fg = render_line(segment.value("forward_separator_fg", "RESET"), vars);
        std::string fsep_bg = render_line(segment.value("forward_separator_bg", "RESET"), vars);
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

      std::string styled_content = content;

      if (colors::is_gradient_value(bg_color_name)) {
        styled_content = colors::apply_gradient_bg_with_fg(
            content, bg_color_name, fg_color_name);
        segment_result += styled_content;
      } else if (colors::is_gradient_value(fg_color_name)) {
        if (bg_color_name != "RESET") {
          segment_result +=
              colors::bg_color(colors::parse_color_value(bg_color_name));
        } else {
          segment_result += colors::ansi::BG_RESET;
        }
        styled_content =
            colors::apply_color_or_gradient(content, fg_color_name, true);
        segment_result += styled_content;
      } else {
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
      }

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

  bool hasAlign = false;
  for (auto& seg : segments)
    if (seg.contains("align")) {
      hasAlign = true;
      break;
    }
  if (!hasAlign) {
    auto result = build(segments);
    result += colors::ansi::RESET;
    return result;
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
            fillL +=
                colors::bg_color(colors::parse_color_value(fill_bg_color_));
          }
        } else {
          fillL += colors::ansi::BG_RESET;
        }

        if (colors::is_gradient_value(fill_fg_color_) ||
            colors::is_gradient_value(fill_bg_color_)) {
          std::string fill_text(padL, fill_char_[0]);
          if (colors::is_gradient_value(fill_bg_color_)) {
            fillL += colors::apply_color_or_gradient(fill_text, fill_bg_color_,
                                                     false);
          } else if (colors::is_gradient_value(fill_fg_color_)) {
            fillL += colors::apply_color_or_gradient(fill_text, fill_fg_color_,
                                                     true);
          }
        } else {
          if (fill_fg_color_ != "RESET") {
            fillL +=
                colors::fg_color(colors::parse_color_value(fill_fg_color_));
          }
          for (size_t i = 0; i < padL; ++i) {
            fillL += fill_char_;
          }
        }
      }

      std::string fillR;
      if (padR > 0) {
        if (fill_bg_color_ != "RESET") {
          if (!colors::is_gradient_value(fill_bg_color_)) {
            fillR +=
                colors::bg_color(colors::parse_color_value(fill_bg_color_));
          }
        } else {
          fillR += colors::ansi::BG_RESET;
        }

        if (colors::is_gradient_value(fill_fg_color_) ||
            colors::is_gradient_value(fill_bg_color_)) {
          std::string fill_text(padR, fill_char_[0]);
          if (colors::is_gradient_value(fill_bg_color_)) {
            fillR += colors::apply_color_or_gradient(fill_text, fill_bg_color_,
                                                     false);
          } else if (colors::is_gradient_value(fill_fg_color_)) {
            fillR += colors::apply_color_or_gradient(fill_text, fill_fg_color_,
                                                     true);
          }
        } else {
          if (fill_fg_color_ != "RESET") {
            fillR +=
                colors::fg_color(colors::parse_color_value(fill_fg_color_));
          }
          for (size_t i = 0; i < padR; ++i) {
            fillR += fill_char_;
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

      if (g_debug_mode) {
        std::cout << "Raw pad calculation: " << pad << " chars" << std::endl;
      }
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
      std::string fill_text(pad, fill_char_[0]);
      if (colors::is_gradient_value(fill_bg_color_)) {
        fill +=
            colors::apply_color_or_gradient(fill_text, fill_bg_color_, false);
      } else if (colors::is_gradient_value(fill_fg_color_)) {
        fill +=
            colors::apply_color_or_gradient(fill_text, fill_fg_color_, true);
      }
    } else {
      if (fill_fg_color_ != "RESET") {
        fill += colors::fg_color(colors::parse_color_value(fill_fg_color_));
      }
      for (size_t i = 0; i < pad; ++i) {
        fill += fill_char_;
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

bool Theme::uses_newline() const {
  return !newline_segments.empty();
}

std::string Theme::get_terminal_title_format() const {
  return terminal_title_format;
}

std::string Theme::escape_brackets_for_isocline(
    const std::string& input) const {
  std::string result = input;

  std::regex numeric_bracket_pattern(R"(\[\s*([+-]?\d+)\s*\])");

  result = std::regex_replace(result, numeric_bracket_pattern, R"(\[$1])");

  if (g_debug_mode) {
    std::cout << "After bracket escaping: " << result << std::endl;
  }

  return result;
}

std::string Theme::process_conditionals(
    const std::string& line,
    const std::unordered_map<std::string, std::string>& vars) const {
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
    const std::string& expr,
    const std::unordered_map<std::string, std::string>& vars) const {
  if (g_debug_mode) {
    std::cout << "Evaluating conditional: " << expr << std::endl;
  }

  size_t question_pos = expr.find('?');
  if (question_pos == std::string::npos) {
    if (g_debug_mode) {
      std::cout << "No '?' found in conditional expression" << std::endl;
    }
    return "";
  }

  size_t colon_pos = expr.find(':', question_pos + 1);
  if (colon_pos == std::string::npos) {
    if (g_debug_mode) {
      std::cout << "No ':' found in conditional expression" << std::endl;
    }
    return "";
  }

  std::string condition = trim(expr.substr(0, question_pos));
  std::string true_value =
      trim(expr.substr(question_pos + 1, colon_pos - question_pos - 1));
  std::string false_value = trim(expr.substr(colon_pos + 1));

  if (g_debug_mode) {
    std::cout << "Condition: '" << condition << "'" << std::endl;
    std::cout << "True value: '" << true_value << "'" << std::endl;
    std::cout << "False value: '" << false_value << "'" << std::endl;
  }

  bool condition_result = evaluate_condition(condition, vars);

  if (g_debug_mode) {
    std::cout << "Condition result: " << (condition_result ? "true" : "false")
              << std::endl;
  }

  std::string selected_value = condition_result ? true_value : false_value;

  return render_line(selected_value, vars);
}

bool Theme::evaluate_condition(
    const std::string& condition,
    const std::unordered_map<std::string, std::string>& vars) const {
  std::string trimmed_condition = trim(condition);

  if (trimmed_condition == "true") {
    return true;
  }
  if (trimmed_condition == "false") {
    return false;
  }

  if (trimmed_condition.front() == '{' && trimmed_condition.back() == '}') {
    std::string var_name =
        trimmed_condition.substr(1, trimmed_condition.length() - 2);
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
  if (trimmed_condition.find(">") != std::string::npos) {
    return evaluate_comparison(trimmed_condition, ">", vars);
  }
  if (trimmed_condition.find("<") != std::string::npos) {
    return evaluate_comparison(trimmed_condition, "<", vars);
  }

  auto it = vars.find(trimmed_condition);
  if (it != vars.end()) {
    std::string value = it->second;
    return !value.empty() && value != "0" && value != "false";
  }

  return !trimmed_condition.empty();
}

bool Theme::evaluate_comparison(
    const std::string& condition, const std::string& op,
    const std::unordered_map<std::string, std::string>& vars) const {
  size_t op_pos = condition.find(op);
  if (op_pos == std::string::npos) {
    return false;
  }

  std::string left = trim(condition.substr(0, op_pos));
  std::string right = trim(condition.substr(op_pos + op.length()));

  std::string left_value = resolve_value(left, vars);
  std::string right_value = resolve_value(right, vars);

  if (g_debug_mode) {
    std::cout << "Comparing: '" << left_value << "' " << op << " '"
              << right_value << "'" << std::endl;
  }

  if (op == "==") {
    return left_value == right_value;
  } else if (op == "!=") {
    return left_value != right_value;
  } else if (op == ">" || op == "<" || op == ">=" || op == "<=") {
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

std::string Theme::resolve_value(
    const std::string& value,
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

std::string Theme::render_line(
    const std::string& line,
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

  result = escape_brackets_for_isocline(result);

  if (g_debug_mode) {
    std::cout << "Rendered line: \n" << result << std::endl;
  }
  return result;
}

void Theme::view_theme_requirements(const std::string& theme) const {
  std::string theme_file = theme_directory + "/" + theme + ".json";

  if (!std::filesystem::exists(theme_file)) {
    print_error({ErrorType::FILE_NOT_FOUND,
                 "view_theme_requirements",
                 "Theme file '" + theme_file + "' does not exist.",
                 {"Use 'theme' to see available themes."}});
    return;
  }
  std::ifstream file(theme_file);
  nlohmann::json theme_json;
  file >> theme_json;
  file.close();

  if (theme_json.contains("requirements") &&
      theme_json["requirements"].is_object() &&
      !theme_json["requirements"].empty()) {
    const nlohmann::json& requirements = theme_json["requirements"];

    if (requirements.contains("colors") && requirements["colors"].is_string()) {
      std::string required_capability =
          requirements["colors"].get<std::string>();
      std::cout << "Terminal color support for " << required_capability
                << " is required." << std::endl;
    }

    if (requirements.contains("fonts") && requirements["fonts"].is_array()) {
      std::stringstream font_req;
      font_req << "This theme works best with one of these fonts: ";
      bool first = true;

      for (const auto& font : requirements["fonts"]) {
        if (font.is_string()) {
          if (!first)
            font_req << ", ";
          font_req << font.get<std::string>();
          first = false;
        }
      }

      std::cout << font_req.str() << std::endl;
    }

    if (requirements.contains("plugins") &&
        requirements["plugins"].is_array()) {
      for (const auto& plugin_name : requirements["plugins"]) {
        if (plugin_name.is_string()) {
          std::string name = plugin_name.get<std::string>();
          std::cout << "Plugin requirement for this theme: " << name
                    << std::endl;
        }
      }
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
  } else {
    std::cout << "No specific requirements found for theme " << theme
              << std::endl;
  }
}

bool Theme::check_theme_requirements(const nlohmann::json& requirements) const {
  bool requirements_met = true;
  std::vector<std::string> missing_requirements;

  if (requirements.contains("colors") && requirements["colors"].is_string()) {
    std::string required_capability = requirements["colors"].get<std::string>();

    if (required_capability == "true_color" &&
        colors::g_color_capability != colors::ColorCapability::TRUE_COLOR) {
      requirements_met = false;
      missing_requirements.push_back(
          "True color (24-bit) terminal support is required. Current terminal "
          "support: " +
          colors::get_color_capability_string(colors::g_color_capability));
    } else if (required_capability == "256_color" &&
               colors::g_color_capability <
                   colors::ColorCapability::XTERM_256_COLOR) {
      requirements_met = false;
      missing_requirements.push_back(
          "256-color terminal support is required. Current terminal support: " +
          colors::get_color_capability_string(colors::g_color_capability));
    }
  }

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
            if (requirements_met) {
              if (!g_plugin->enable_plugin(name)) {
                auto available_plugins = g_plugin->get_available_plugins();
                requirements_met = false;
                if ((std::find(available_plugins.begin(),
                               available_plugins.end(),
                               name) == available_plugins.end())) {
                  missing_requirements.push_back("Plugin '" + name +
                                                 "' is required but not found");
                } else {
                  missing_requirements.push_back(
                      "Plugin '" + name + "' is required but not enabled");
                }
              }
            } else {
              auto available_plugins = g_plugin->get_available_plugins();
              if ((std::find(available_plugins.begin(), available_plugins.end(),
                             name) == available_plugins.end())) {
                missing_requirements.push_back("Plugin '" + name +
                                               "' is required but not found");
              } else {
                missing_requirements.push_back(
                    "Other requirements are not passing. Not attempting to "
                    "enable plugin '" +
                    name + "'");
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

  if (!requirements_met) {
    std::cerr << "Theme requirements not met:" << std::endl;
    for (const auto& req : missing_requirements) {
      std::cerr << " - " << req << std::endl;
    }
  }

  return requirements_met;
}

size_t Theme::calculate_raw_length(const std::string& str) const {
  size_t ansi_chars = 0;
  size_t visible_chars = 0;

  std::string str_without_isocline_escapes = str;

  std::regex isocline_bracket_pattern(R"(\\(\[[+-]?\d+\]))");
  str_without_isocline_escapes = std::regex_replace(
      str_without_isocline_escapes, isocline_bracket_pattern, "$1");

  size_t raw_length = utf8_utils::calculate_display_width(
      str_without_isocline_escapes, &ansi_chars, &visible_chars);

  if (g_debug_mode) {
    std::cout << "String length: " << str.size()
              << " bytes, Visible chars: " << visible_chars
              << ", ANSI chars: " << ansi_chars
              << ", Raw display width: " << raw_length << std::endl;
  }

  return raw_length;
}
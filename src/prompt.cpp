#include "prompt.h"

#include <iostream>

#include "cjsh.h"
#include "theme.h"

Prompt::Prompt() {}

Prompt::~Prompt() {}

std::string Prompt::get_prompt() {
  if (g_current_theme.empty()) {
    if (!g_theme->get_enabled()) {
      return info.get_basic_prompt();
    } else {
      g_theme->load_theme("default");
    }
  }
  
  std::filesystem::path repo_root;
  bool is_git_repo = info.is_git_repository(repo_root);
  
  // Get all variables
  std::unordered_map<std::string, std::string> vars = get_all_variables();

  if (is_git_repo) {
    return g_theme->get_git_prompt_format(vars);
  } else {
    return g_theme->get_ps1_prompt_format(vars);
  }
}

std::string Prompt::get_ai_prompt() {
  std::string modelInfo = g_ai->get_model();
  std::string modeInfo = g_ai->get_assistant_type();

  if (g_current_theme.empty()) {
    if (!g_theme->get_enabled()) {
      return " > ";
    } else {
      g_theme->load_theme("default");
    }
  }

  if (modelInfo.empty()) modelInfo = "Unknown";
  if (modeInfo.empty()) modeInfo = "Chat";

  // Get all variables
  std::unordered_map<std::string, std::string> vars = get_all_variables();
  
  // Add or update AI-specific variables
  vars["AI_MODEL"] = modelInfo;
  vars["AI_AGENT_TYPE"] = modeInfo;
  vars["AI_DIVIDER"] = ">";
  vars["AI_CONTEXT"] = g_ai->get_save_directory();
  vars["AI_CONTEXT_COMPARISON"] =
      (std::filesystem::current_path().string() + "/" ==
       g_ai->get_save_directory())
          ? "✔"
          : "✖";

  return g_theme->get_ai_prompt_format(vars);
}

std::string Prompt::get_newline_prompt() {
  if (g_current_theme.empty()) {
    if (!g_theme->get_enabled()) {
      return " ";
    } else {
      g_theme->load_theme("default");
    }
  }
  
  // Get all variables
  std::unordered_map<std::string, std::string> vars = get_all_variables();

  return g_theme->get_newline_prompt(vars);
}

std::string Prompt::get_title_prompt() {
  if (g_current_theme.empty()) {
    if (!g_theme->get_enabled()) {
      return info.get_basic_title();
    } else {
      g_theme->load_theme("default");
    }
  }
  std::string prompt_format = g_theme->get_terminal_title_format();

  // Get all variables
  std::unordered_map<std::string, std::string> vars = get_all_variables();

  for (const auto& [key, value] : vars) {
    prompt_format = replace_placeholder(prompt_format, "{" + key + "}", value);
  }

  return prompt_format;
}

std::string Prompt::replace_placeholder(const std::string& format,
                                        const std::string& placeholder,
                                        const std::string& value) {
  std::string result = format;
  size_t pos = 0;
  while ((pos = result.find(placeholder, pos)) != std::string::npos) {
    result.replace(pos, placeholder.length(), value);
    pos += value.length();
  }
  return result;
}

// Helper method to get all available variables from all segment types
std::unordered_map<std::string, std::string> Prompt::get_all_variables() {
  // Get git repository info if available
  std::filesystem::path repo_root;
  bool is_git_repo = info.is_git_repository(repo_root);

  // Collect all segments to ensure all variables are available
  std::vector<nlohmann::json> all_segments;
  all_segments.insert(all_segments.end(), g_theme->ps1_segments.begin(), g_theme->ps1_segments.end());
  all_segments.insert(all_segments.end(), g_theme->git_segments.begin(), g_theme->git_segments.end());
  all_segments.insert(all_segments.end(), g_theme->ai_segments.begin(), g_theme->ai_segments.end());
  all_segments.insert(all_segments.end(), g_theme->newline_segments.begin(), g_theme->newline_segments.end());
  
  // Add the terminal title format as a segment to check
  nlohmann::json title_segment;
  title_segment["content"] = g_theme->get_terminal_title_format();
  all_segments.push_back(title_segment);

  // Get all variables
  return info.get_variables(all_segments, is_git_repo, repo_root);
}
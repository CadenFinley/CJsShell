#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "prompt.h"
#include "prompt_info.h"
#include "theme.h"
#include "theme_command.h"

int preview_theme(const std::string& theme_name) {
  auto temp_theme = std::make_unique<Theme>(
      cjsh_filesystem::g_cjsh_theme_path.string(), true);

  std::string theme_file =
      cjsh_filesystem::g_cjsh_theme_path.string() + "/" + theme_name + ".json";

  if (!std::filesystem::exists(theme_file)) {
    std::cerr << "Error: Theme '" << theme_name << "' not found." << std::endl;
    return 1;
  }

  if (!temp_theme->load_theme(theme_name, false)) {
    std::cerr << "Error: Failed to load theme '" << theme_name << "'."
              << std::endl;
    return 1;
  }

  std::cout << "\nPreview of theme '" << theme_name << "':" << std::endl;
  temp_theme->view_theme_requirements(theme_name);
  std::cout << "==========================================\n" << std::endl;

  PromptInfo prompt_info;
  Prompt prompt;

  std::filesystem::path repo_root = std::filesystem::current_path();
  bool is_git_repo = prompt.is_git_repository(repo_root);

  std::vector<nlohmann::json> all_segments;
  all_segments.insert(all_segments.end(), temp_theme->ps1_segments.begin(),
                      temp_theme->ps1_segments.end());
  all_segments.insert(all_segments.end(), temp_theme->git_segments.begin(),
                      temp_theme->git_segments.end());
  all_segments.insert(all_segments.end(), temp_theme->ai_segments.begin(),
                      temp_theme->ai_segments.end());
  all_segments.insert(all_segments.end(), temp_theme->newline_segments.begin(),
                      temp_theme->newline_segments.end());
  nlohmann::json title_segment;
  title_segment["content"] = temp_theme->get_terminal_title_format();
  all_segments.push_back(title_segment);

  auto vars = prompt_info.get_variables(all_segments, is_git_repo, repo_root);

  vars["AI_MODEL"] = "AI_MODEL";
  vars["AI_AGENT_TYPE"] = "AI_AGENT_TYPE";
  vars["AI_DIVIDER"] = ">";
  vars["AI_CONTEXT"] = std::filesystem::current_path().string() + "/";
  vars["AI_CONTEXT_COMPARISON"] = "✔";

  if (!is_git_repo) {
    vars["GIT_BRANCH"] = "main";
    vars["GIT_STATUS"] = "*+";
    vars["LOCAL_PATH"] = vars["DIRECTORY"];
  }

  std::cout << "Terminal Title: ";
  std::string title_format = temp_theme->get_terminal_title_format();
  for (const auto& [key, value] : vars) {
    size_t pos = 0;
    std::string placeholder = "{" + key + "}";
    while ((pos = title_format.find(placeholder, pos)) != std::string::npos) {
      title_format.replace(pos, placeholder.length(), value);
      pos += value.length();
    }
  }
  std::cout << title_format << std::endl;
  std::cout << std::endl;

  std::cout << "Standard Prompt:" << std::endl;
  std::cout << temp_theme->get_ps1_prompt_format(vars) << std::endl;
  std::cout << std::endl;

  std::cout << "Git Prompt:" << std::endl;
  std::cout << temp_theme->get_git_prompt_format(vars) << std::endl;
  std::cout << std::endl;

  std::cout << "AI Prompt:" << std::endl;
  std::cout << temp_theme->get_ai_prompt_format(vars) << std::endl;
  std::cout << std::endl;

  if (temp_theme->uses_newline()) {
    std::cout << "Newline Prompt:" << std::endl;
    std::cout << temp_theme->get_newline_prompt(vars) << std::endl;
    std::cout << std::endl;
  }

  std::cout << "==========================================" << std::endl;
  std::cout << "Note: Actual appearance may vary based on your terminal and "
               "environment.\n"
            << std::endl;

  return 0;
}

#include "prompt.h"
#include "main.h"
#include "theme.h"
#include <iostream>
#include <fstream>
#include <regex>

Prompt::Prompt() {
  last_git_status_check = std::chrono::steady_clock::now() - std::chrono::seconds(30);
  is_git_status_check_running = false;
}

Prompt::~Prompt() {
}

std::string Prompt::get_prompt() {
  struct passwd *pw = getpwuid(getuid());
  std::string username = pw ? pw->pw_name : "user";
  std::string prompt_format;
  
  std::filesystem::path current_path = std::filesystem::current_path();
  std::filesystem::path git_head_path;
  while (!is_root_path(current_path)) {
    git_head_path = current_path / ".git" / "HEAD";
    if (std::filesystem::exists(git_head_path)) {
        break;
    }
    current_path = current_path.parent_path();
  }

  bool is_git_repo = std::filesystem::exists(git_head_path);
  if (is_git_repo) {
    prompt_format = g_theme->get_git_prompt_format();
    std::string git_info;
    
    try {
      std::ifstream head_file(git_head_path);
      std::string line;
      std::regex head_pattern("ref: refs/heads/(.*)");
      std::smatch match;
      std::string branch_name;
      while (std::getline(head_file, line)) {
        if (std::regex_search(line, match, head_pattern)) {
          branch_name = match[1];
          break;
        }
      }

      std::string status_symbols = "";
      std::string git_dir = current_path.string();
      bool is_clean_repo = true;
      
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_git_status_check).count();
      
      if ((elapsed > 30 || cached_git_dir != git_dir) && !is_git_status_check_running) {
        std::lock_guard<std::mutex> lock(git_status_mutex);
        cached_git_dir = git_dir;
        cached_status_symbols = "*";
        cached_is_clean_repo = false;
        last_git_status_check = std::chrono::steady_clock::now();
        status_symbols = cached_status_symbols;
        is_clean_repo = cached_is_clean_repo;
      } else {
        std::lock_guard<std::mutex> lock(git_status_mutex);
        status_symbols = cached_status_symbols;
        is_clean_repo = cached_is_clean_repo;
      }
      
      // Extract the repo name from the git repository root directory
      std::string repo_name = current_path.filename().string();
      std::string status_info;
      
      if (is_clean_repo) {
        status_info = " ✓";
      } else {
        status_info = " " + status_symbols;
      }

      if(branch_name.empty()) {
        branch_name = "unknown";
      }
      
      // Replace placeholders with actual values
      char hostname[256];
      gethostname(hostname, 256);
      
      prompt_format = replace_placeholder(prompt_format, "{USERNAME}", username);
      prompt_format = replace_placeholder(prompt_format, "{HOSTNAME}", hostname);
      prompt_format = replace_placeholder(prompt_format, "{PATH}", get_current_file_path());
      prompt_format = replace_placeholder(prompt_format, "{DIRECTORY}", get_current_file_name());
      prompt_format = replace_placeholder(prompt_format, "{REPO_NAME}", repo_name);
      prompt_format = replace_placeholder(prompt_format, "{GIT_BRANCH}", branch_name);
      prompt_format = replace_placeholder(prompt_format, "{GIT_STATUS}", status_info);
      
      return prompt_format;
    } catch (const std::exception& e) {
      std::cerr << "Error reading git HEAD file: " << e.what() << std::endl;
    }
  }
  prompt_format = g_theme->get_ps1_prompt_format();
  
  // Replace placeholders for non-git prompt
  char hostname[256];
  gethostname(hostname, 256);
  
  prompt_format = replace_placeholder(prompt_format, "{USERNAME}", username);
  prompt_format = replace_placeholder(prompt_format, "{HOSTNAME}", hostname);
  prompt_format = replace_placeholder(prompt_format, "{PATH}", get_current_file_path());
  prompt_format = replace_placeholder(prompt_format, "{DIRECTORY}", get_current_file_name());
  
  return prompt_format;
}

std::string Prompt::get_ai_prompt() {
  std::string modelInfo = g_ai->getModel();
  std::string modeInfo = g_ai->getAssistantType();
            
  if (modelInfo.empty()) modelInfo = "Unknown";
  if (modeInfo.empty()) modeInfo = "Chat";
  
  std::string prompt_format = g_theme->get_ai_prompt_format();
  
  // Replace AI-specific placeholders
  prompt_format = replace_placeholder(prompt_format, "{AI_MODEL}", modelInfo);
  prompt_format = replace_placeholder(prompt_format, "{AI_AGENT_TYPE}", modeInfo);
  prompt_format = replace_placeholder(prompt_format, "{AI_DIVIDER}", ">");
  
  return prompt_format;
}

// Helper method to replace placeholders in format strings
std::string Prompt::replace_placeholder(const std::string& format, const std::string& placeholder, const std::string& value) {
  std::string result = format;
  size_t pos = 0;
  while ((pos = result.find(placeholder, pos)) != std::string::npos) {
    result.replace(pos, placeholder.length(), value);
    pos += value.length();
  }
  return result;
}

bool Prompt::is_root_path(const std::filesystem::path& path) {
  return path == path.root_path();
}

std::string Prompt::get_current_file_path() {
  std::string path = std::filesystem::current_path().string();
  
  if (path == "/") {
    return "/";
  }
  
  char* home_dir = getenv("HOME");
  if (home_dir) {
    std::string home_path = home_dir;
    if (path == home_path) {
      return "~";
    } else if (path.find(home_path + "/") == 0) {
      return "~" + path.substr(home_path.length());
    }
  }
  
  return path;
}

std::string Prompt::get_current_file_name() {
  std::string current_directory = get_current_file_path();
  
  if (current_directory == "/") {
    return "/";
  }
  
  if (current_directory == "~") {
    return "~";
  }
  
  if (current_directory.find("~/") == 0) {
    std::string relative_path = current_directory.substr(2);
    if (relative_path.empty()) {
      return "~";
    }
    size_t last_slash = relative_path.find_last_of('/');
    if (last_slash != std::string::npos) {
      return relative_path.substr(last_slash + 1);
    }
    return relative_path;
  }
  
  std::string current_file_name = std::filesystem::path(current_directory).filename().string();
  if (current_file_name.empty()) {
    return "/";
  }
  return current_file_name;
}
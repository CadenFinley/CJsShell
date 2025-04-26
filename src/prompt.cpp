#include "prompt.h"
#include "main.h"

Prompt::Prompt() {
  last_git_status_check = std::chrono::steady_clock::now() - std::chrono::seconds(30);
  is_git_status_check_running = false;
  display_whole_path = false;
}

Prompt::~Prompt() {
  // nuthin
}

std::string Prompt::get_prompt() {
  // Get current username
  struct passwd *pw = getpwuid(getuid());
  std::string username = pw ? pw->pw_name : "user";
  
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
      
      std::string repo_name = display_whole_path ? get_current_file_path() : get_current_file_name();
      std::string status_info;
      
      if (is_clean_repo) {
        status_info = " ✓";
      } else {
        status_info = " " + status_symbols;
      }
      
      git_info = DIRECTORY_COLOR + " git:(" + RESET_COLOR + BRANCH_COLOR + branch_name + RESET_COLOR;
      
      if (is_clean_repo) {
        git_info += DIRECTORY_COLOR + status_info + RESET_COLOR;
      } else if (!status_symbols.empty()) {
        git_info += DIRECTORY_COLOR + status_info + RESET_COLOR;
      }
      
      git_info += DIRECTORY_COLOR + ")" + RESET_COLOR;
      
      return SHELL_COLOR + username + RESET_COLOR + " " +
             GIT_COLOR + repo_name + RESET_COLOR + git_info;
    } catch (const std::exception& e) {
      std::cerr << "Error reading git HEAD file: " << e.what() << std::endl;
    }
  }
  
  // If not a git repo or if there was an error, return a simple prompt
  if (display_whole_path) {
    return SHELL_COLOR + username + RESET_COLOR + " " + 
           DIRECTORY_COLOR + get_current_file_path() + RESET_COLOR;
  } else {
    return SHELL_COLOR + username + RESET_COLOR + " " + 
           DIRECTORY_COLOR + get_current_file_name() + RESET_COLOR;
  }
}

std::string Prompt::get_ai_prompt() {
  // TODO
  std::string modelInfo = g_ai -> getModel();
  std::string modeInfo = g_ai -> getAssistantType();
            
  if (modelInfo.empty()) modelInfo = "Unknown";
  if (modeInfo.empty()) modeInfo = "Chat";
  //return g_theme -> ai_prompt_divider_color + "[" + g_theme -> ai_prompt_model_color + modelInfo + g_theme -> ai_prompt_divider_color + " | " + g_theme -> ai_prompt_info + modeInfo + g_theme -> ai_prompt_divider_color + "] >" + c_reset_color;
  return "AI >";
}

bool Prompt::is_root_path(const std::filesystem::path& path) {
  return path == path.root_path();
}

std::string Prompt::get_current_file_path() {
  std::string path = std::filesystem::current_path().string();
  
  // Check if path is root
  if (path == "/") {
    return "/";
  }
  
  // Check if path is in home directory and replace with ~
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
  
  // For root directory
  if (current_directory == "/") {
    return "/";
  }
  
  // For home directory
  if (current_directory == "~") {
    return "~";
  }
  
  // If it's a path starting with ~/ extract the last component
  if (current_directory.find("~/") == 0) {
    std::string relative_path = current_directory.substr(2); // Skip "~/"
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
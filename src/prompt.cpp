#include "prompt.h"
#include "main.h"
#include "theme.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <iomanip>
#include <ctime>
#include <sstream>

/*
 * Available prompt placeholders:
 * -----------------------------
 * Standard prompt placeholders (PS1):
 * {USERNAME}   - Current user's name
 * {HOSTNAME}   - System hostname
 * {PATH}       - Current working directory (with ~ for home)
 * {DIRECTORY}  - Name of the current directory
 * {TIME}       - Current time (HH:MM:SS)
 * {DATE}       - Current date (YYYY-MM-DD)
 * {SHELL} - Name of the shell
 * {SHELL_VER}  - Version of the shell
 * 
 * Git prompt additional placeholders:
 * {LOCAL_PATH} - Local path of the git repository
 * {GIT_BRANCH} - Current Git branch
 * {GIT_STATUS} - Git status (✓ for clean, * for dirty)
 * 
 * AI prompt placeholders:
 * {AI_MODEL}      - Current AI model name
 * {AI_AGENT_TYPE} - AI assistant type (Chat, etc.)
 * {AI_DIVIDER}    - Divider for AI prompt (>)
 * 
 * Terminal title placeholders:
 * {SHELL}     - Terminal name
 * {USERNAME}  - Current user's name
 * {HOSTNAME}  - System hostname
 * {DIRECTORY} - Name of the current directory
 * {PATH}      - Current working directory (with ~ for home)
 * {TIME}      - Current time (HH:MM:SS)
 * {DATE}      - Current date (YYYY-MM-DD)
 */

Prompt::Prompt() {
  last_git_status_check = std::chrono::steady_clock::now() - std::chrono::seconds(30);
  is_git_status_check_running = false;
}

Prompt::~Prompt() {
}

bool Prompt::is_variable_used(const std::string& var_name, const std::vector<nlohmann::json>& segments) {
  // Check if variable is used in format string
  std::string placeholder = "{" + var_name + "}";
  
  // Check if variable is used in any segment
  for (const auto& segment : segments) {
    if (segment.contains("content")) {
      std::string content = segment["content"];
      if (content.find(placeholder) != std::string::npos) {
        return true;
      }
    }
  }
  
  return false;
}

std::string Prompt::get_prompt() {
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
  
  // Variables to be passed to prompt renderer
  std::unordered_map<std::string, std::string> vars;
  
  // Base format and segments to check for variable usage
  std::string format_to_check;
  std::vector<nlohmann::json> segments_to_check;
  
  if (is_git_repo) {
    segments_to_check = g_theme->git_segments;
  } else {
    segments_to_check = g_theme->ps1_segments;
  }
  
  // Only calculate needed variables
  if (is_variable_used("USERNAME", segments_to_check)) {
    vars["USERNAME"] = username;
  }
  if (is_variable_used("HOSTNAME", segments_to_check)) {
    char hostname[256];
    gethostname(hostname, 256);
    vars["HOSTNAME"] = hostname;
  }
  
  if (is_variable_used("PATH", segments_to_check)) {
    vars["PATH"] = get_current_file_path();
  }
  
  if (is_variable_used("DIRECTORY", segments_to_check)) {
    vars["DIRECTORY"] = get_current_file_name();
  }
  
  if (is_variable_used("TIME", segments_to_check)) {
    vars["TIME"] = get_current_time();
  }
  
  if (is_variable_used("DATE", segments_to_check)) {
    vars["DATE"] = get_current_date();
  }
  
  if (is_variable_used("SHELL", segments_to_check)) {
    vars["SHELL"] = get_shell();
  }
  
  if (is_variable_used("SHELL_VER", segments_to_check)) {
    vars["SHELL_VER"] = get_shell_version();
  }

  if (is_git_repo) {
    try {
      std::ifstream head_file(git_head_path);
      std::string line;
      std::regex head_pattern("ref: refs/heads/(.*)");
      std::smatch match;
      std::string branch_name;
      
      // Only read branch name if it's needed
      if (is_variable_used("GIT_BRANCH", segments_to_check)) {
        while (std::getline(head_file, line)) {
          if (std::regex_search(line, match, head_pattern)) {
            branch_name = match[1];
            break;
          }
        }
        
        if(branch_name.empty()) {
          branch_name = "unknown";
        }
        
        vars["GIT_BRANCH"] = branch_name;
      }

      // Only check git status if it's needed
      if (is_variable_used("GIT_STATUS", segments_to_check) || 
          is_variable_used("LOCAL_PATH", segments_to_check)) {
        
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
        
        std::string status_info;
        
        if (is_clean_repo) {
          status_info = " ✓";
        } else {
          status_info = " " + status_symbols;
        }
        
        vars["GIT_STATUS"] = status_info;
        
        if (is_variable_used("LOCAL_PATH", segments_to_check)) {
          std::filesystem::path cwd = std::filesystem::current_path();
          std::string repo_root_path = current_path.string();
          std::string repo_root_name = current_path.filename().string();
          std::string current_path_str = cwd.string();
          
          if (current_path_str == repo_root_path) {
            vars["LOCAL_PATH"] = repo_root_name;
          } else if (current_path_str.find(repo_root_path) == 0) {
            std::string relative_path = current_path_str.substr(repo_root_path.length());
            if (!relative_path.empty() && relative_path[0] == '/') {
              relative_path = relative_path.substr(1);
            }
            relative_path = repo_root_name + (relative_path.empty() ? "" : "/" + relative_path);
            vars["LOCAL_PATH"] = relative_path;
          } else {
            vars["LOCAL_PATH"] = "/";
          }
        }
      }
      return g_theme->get_git_prompt_format(vars);
    } catch (const std::exception& e) {
      std::cerr << "Error reading git HEAD file: " << e.what() << std::endl;
    }
  }
  
  return g_theme->get_ps1_prompt_format(vars);
}

std::string Prompt::get_ai_prompt() {
  std::string modelInfo = g_ai->getModel();
  std::string modeInfo = g_ai->getAssistantType();
            
  if (modelInfo.empty()) modelInfo = "Unknown";
  if (modeInfo.empty()) modeInfo = "Chat";
  
  std::unordered_map<std::string, std::string> vars;
  std::vector<nlohmann::json> segments_to_check = g_theme->ai_segments;
  
  if (is_variable_used("AI_MODEL", segments_to_check)) {
    vars["AI_MODEL"] = modelInfo;
  }
  
  if (is_variable_used("AI_AGENT_TYPE", segments_to_check)) {
    vars["AI_AGENT_TYPE"] = modeInfo;
  }
  
  if (is_variable_used("AI_DIVIDER", segments_to_check)) {
    vars["AI_DIVIDER"] = ">";
  }
  
  if (is_variable_used("TIME", segments_to_check)) {
    vars["TIME"] = get_current_time();
  }
  
  if (is_variable_used("DATE", segments_to_check)) {
    vars["DATE"] = get_current_date();
  }
  
  return g_theme->get_ai_prompt_format(vars);
}

std::string Prompt::get_newline_prompt() {
  std::unordered_map<std::string, std::string> vars;
  std::vector<nlohmann::json> segments_to_check = g_theme->newline_segments;
  
  if (is_variable_used("USERNAME", segments_to_check)) {
    vars["USERNAME"] = getenv("USER");
  }
  
  if (is_variable_used("HOSTNAME", segments_to_check)) {
    char hostname[256];
    gethostname(hostname, 256);
    vars["HOSTNAME"] = hostname;
  }
  
  if (is_variable_used("PATH", segments_to_check)) {
    vars["PATH"] = get_current_file_path();
  }
  
  if (is_variable_used("DIRECTORY", segments_to_check)) {
    vars["DIRECTORY"] = get_current_file_name();
  }
  
  return g_theme->get_newline_prompt(vars);
}

std::string Prompt::get_title_prompt() {
  std::string prompt_format = g_theme->get_terminal_title_format();
  std::unordered_map<std::string, std::string> vars;
  
  if (prompt_format.find("{SHELL}") != std::string::npos) {
    vars["SHELL"] = get_shell();
  }
  
  if (prompt_format.find("{USERNAME}") != std::string::npos) {
    vars["USERNAME"] = getenv("USER");
  }
  
  if (prompt_format.find("{HOSTNAME}") != std::string::npos) {
    char hostname[256];
    gethostname(hostname, 256);
    vars["HOSTNAME"] = hostname;
  }
  
  if (prompt_format.find("{DIRECTORY}") != std::string::npos) {
    vars["DIRECTORY"] = get_current_file_name();
  }
  
  if (prompt_format.find("{PATH}") != std::string::npos) {
    vars["PATH"] = get_current_file_path();
  }
  
  if (prompt_format.find("{TIME}") != std::string::npos) {
    vars["TIME"] = get_current_time();
  }
  
  if (prompt_format.find("{DATE}") != std::string::npos) {
    vars["DATE"] = get_current_date();
  }
  
  if (prompt_format.find("{SHELL}") != std::string::npos) {
    vars["SHELL"] = get_shell();
  }
  
  if (prompt_format.find("{SHELL_VER}") != std::string::npos) {
    vars["SHELL_VER"] = get_shell_version();
  }
  
  for (const auto& [key, value] : vars) {
    prompt_format = replace_placeholder(prompt_format, "{" + key + "}", value);
  }
  
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

std::string Prompt::get_current_time() {
  auto now = std::chrono::system_clock::now();
  auto time_now = std::chrono::system_clock::to_time_t(now);
  struct tm time_info;
  localtime_r(&time_now, &time_info);
  
  std::stringstream time_stream;
  time_stream << std::setfill('0') << std::setw(2) << time_info.tm_hour << ":"
              << std::setfill('0') << std::setw(2) << time_info.tm_min << ":"
              << std::setfill('0') << std::setw(2) << time_info.tm_sec;
  
  return time_stream.str();
}

std::string Prompt::get_current_date() {
  auto now = std::chrono::system_clock::now();
  auto time_now = std::chrono::system_clock::to_time_t(now);
  struct tm time_info;
  localtime_r(&time_now, &time_info);
  
  std::stringstream date_stream;
  date_stream << (time_info.tm_year + 1900) << "-"
              << std::setfill('0') << std::setw(2) << (time_info.tm_mon + 1) << "-"
              << std::setfill('0') << std::setw(2) << time_info.tm_mday;
  
  return date_stream.str();
}

std::string Prompt::get_shell() {
  return "cjsh";
}

std::string Prompt::get_shell_version() {
  return c_version;
}
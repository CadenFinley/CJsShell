#include "prompt_info.h"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>

#include "cjsh.h"

/* Available prompt placeholders:
 * -----------------------------
 * Standard prompt placeholders (PS1):
 * {USERNAME}   - Current user's name
 * {HOSTNAME}   - System hostname
 * {PATH}       - Current working directory (with ~ for home)
 * {DIRECTORY}  - Name of the current directory
 * {TIME12}     - Current time (HH:MM:SS) in 12 hour format
 * {TIME24}, {TIME} - Current time (HH:MM:SS) in 24 hour format
 * {DATE}       - Current date (YYYY-MM-DD)
 * {DAY}        - Current day of the month (1-31)
 * {MONTH}      - Current month (1-12)
 * {YEAR}       - Current year (YYYY)
 * {DAY_NAME}   - Name of the current day (e.g., Monday)
 * {MONTH_NAME} - Name of the current month (e.g., September)
 * {SHELL}      - Name of the shell
 * {SHELL_VER}  - Version of the shell
 *
 * Git prompt additional placeholders:
 * {LOCAL_PATH} - Local path of the git repository
 * {GIT_BRANCH} - Current Git branch
 * {GIT_STATUS} - Git status (✓ for clean, * for dirty)
 * {GIT_AHEAD}  - Number of commits ahead of remote
 * {GIT_BEHIND} - Number of commits behind remote
 * {GIT_STASHES} - Number of stashes in the repository
 * {GIT_STAGED} - Has staged changes (✓ or empty)
 * {GIT_CHANGES} - Number of uncommitted changes
 * {GIT_REMOTE} - Remote URL of the current repo
 * {GIT_TAG} - Current Git tag (if any)
 * {GIT_LAST_COMMIT} - Last commit hash or message
 * {GIT_AUTHOR} - Author of the last commit
 *
 * System information placeholders:
 * {OS_INFO}     - Operating system name and version
 * {KERNEL_VER}  - Kernel version
 * {CPU_USAGE}   - Current CPU usage percentage
 * {MEM_USAGE}   - Current memory usage percentage
 * {BATTERY}     - Battery percentage and charging status
 * {UPTIME}      - System uptime
 * {DISK_USAGE}  - Disk usage of current directory or root
 * {SWAP_USAGE}  - Swap memory usage
 * {LOAD_AVG}    - System load average
 *
 * Environment information placeholders:
 * {TERM_TYPE}   - Terminal type (e.g., xterm, screen)
 * {TERM_SIZE}   - Terminal dimensions (columns x rows)
 * {LANG_VER:X}  - Version of language X (python, node, ruby, go, rust)
 * {VIRTUAL_ENV} - Name of active virtual environment, if any
 * {BG_JOBS}     - Number of background jobs
 * {STATUS}      - Last command exit code
 *
 * Network information placeholders:
 * {IP_LOCAL}    - Local IP address
 * {IP_EXTERNAL} - External IP address
 * {VPN_STATUS}  - VPN connection status (on/off)
 * {NET_IFACE}   - Active network interface
 *
 * AI prompt placeholders:
 * {AI_MODEL}      - Current AI model name
 * {AI_AGENT_TYPE} - AI assistant type (Chat, etc.)
 * {AI_DIVIDER}    - Divider for AI prompt (>)
 * {AI_CONTEXT}    - Current working directory path
 * {AI_CONTEXT_COMPARISON} - Check mark for when the context is local and equal
 * to current directory, ✔ and ✖ for when it is not
 */
std::string PromptInfo::get_git_remote(const std::filesystem::path& repo_root) {
  std::string cmd =
      "git -C '" + repo_root.string() + "' remote get-url origin 2>/dev/null";
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp)
    return "";
  char buffer[256];
  std::string result = "";
  while (fgets(buffer, sizeof(buffer), fp) != NULL)
    result += buffer;
  pclose(fp);
  if (!result.empty() && result.back() == '\n')
    result.pop_back();
  return result;
}

std::string PromptInfo::get_git_tag(const std::filesystem::path& repo_root) {
  std::string cmd = "git -C '" + repo_root.string() +
                    "' describe --tags --abbrev=0 2>/dev/null";
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp)
    return "";
  char buffer[128];
  std::string result = "";
  while (fgets(buffer, sizeof(buffer), fp) != NULL)
    result += buffer;
  pclose(fp);
  if (!result.empty() && result.back() == '\n')
    result.pop_back();
  return result;
}

std::string PromptInfo::get_git_last_commit(
    const std::filesystem::path& repo_root) {
  std::string cmd = "git -C '" + repo_root.string() +
                    "' log -1 --pretty=format:%h:%s 2>/dev/null";
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp)
    return "";
  char buffer[256];
  std::string result = "";
  while (fgets(buffer, sizeof(buffer), fp) != NULL)
    result += buffer;
  pclose(fp);
  return result;
}

std::string PromptInfo::get_git_author(const std::filesystem::path& repo_root) {
  std::string cmd = "git -C '" + repo_root.string() +
                    "' log -1 --pretty=format:%an 2>/dev/null";
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp)
    return "";
  char buffer[128];
  std::string result = "";
  while (fgets(buffer, sizeof(buffer), fp) != NULL)
    result += buffer;
  pclose(fp);
  if (!result.empty() && result.back() == '\n')
    result.pop_back();
  return result;
}

std::string PromptInfo::get_disk_usage(const std::filesystem::path& path) {
  std::string cmd = "df -h '" + path.string() + "' | awk 'NR==2{print $5}'";
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp)
    return "";
  char buffer[32];
  std::string result = "";
  while (fgets(buffer, sizeof(buffer), fp) != NULL)
    result += buffer;
  pclose(fp);
  if (!result.empty() && result.back() == '\n')
    result.pop_back();
  return result;
}

std::string PromptInfo::get_swap_usage() {
#ifdef __APPLE__
  std::string cmd = "sysctl vm.swapusage | awk '{print $3}'";
#elif defined(__linux__)
  std::string cmd = "free | grep Swap | awk '{print $3 \" / \" $2}'";
#else
  return "Unknown";
#endif
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp)
    return "";
  char buffer[32];
  std::string result = "";
  while (fgets(buffer, sizeof(buffer), fp) != NULL)
    result += buffer;
  pclose(fp);
  if (!result.empty() && result.back() == '\n')
    result.pop_back();
  return result;
}

std::string PromptInfo::get_load_avg() {
  std::string cmd = "uptime | awk -F'load averages?: ' '{print $2}'";
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp)
    return "";
  char buffer[64];
  std::string result = "";
  while (fgets(buffer, sizeof(buffer), fp) != NULL)
    result += buffer;
  pclose(fp);
  if (!result.empty() && result.back() == '\n')
    result.pop_back();
  return result;
}

PromptInfo::PromptInfo() {
  if (g_debug_mode)
    std::cerr << "DEBUG: PromptInfo constructor START" << std::endl;

  last_git_status_check =
      std::chrono::steady_clock::now() - std::chrono::seconds(30);
  is_git_status_check_running = false;
  cached_is_clean_repo = true;

  if (g_debug_mode)
    std::cerr << "DEBUG: PromptInfo constructor END" << std::endl;
}

PromptInfo::~PromptInfo() {
  if (g_debug_mode)
    std::cerr << "DEBUG: PromptInfo destructor" << std::endl;
}

std::string PromptInfo::get_basic_prompt() {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_basic_prompt START" << std::endl;

  std::string username = get_username();
  std::string hostname = get_hostname();
  std::string cwd = get_current_file_path();

  // Pre-calculate size and reserve to avoid reallocations
  size_t size = username.length() + hostname.length() + cwd.length() +
                10;  // extra for formatting
  std::string prompt;
  prompt.reserve(size);

  prompt = username + "@" + hostname + " : " + cwd + " $ ";

  if (g_debug_mode)
    std::cerr << "DEBUG: get_basic_prompt END" << std::endl;
  return prompt;
}

std::string PromptInfo::get_basic_ai_prompt() {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_basic_ai_prompt START" << std::endl;

  std::string cwd = get_current_file_path();
  std::string ai_model = g_ai->get_model();
  std::string ai_context = g_ai->get_save_directory();
  std::string ai_type = g_ai->get_assistant_type();
  std::string ai_context_comparison =
      (std::filesystem::current_path().string() + "/" == ai_context) ? "✔"
                                                                     : "✖";

  // Pre-calculate size and reserve to avoid reallocations
  size_t size = ai_model.length() + ai_context.length() + cwd.length() +
                ai_type.length() + ai_context_comparison.length() + 10;
  std::string prompt;
  prompt.reserve(size);

  prompt = ai_model + " " + ai_context + " " + ai_context_comparison + " " +
           cwd + " " + ai_type + " > ";

  if (g_debug_mode)
    std::cerr << "DEBUG: get_basic_ai_prompt END" << std::endl;
  return prompt;
}

std::string PromptInfo::get_basic_title() {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_basic_title START/END" << std::endl;
  return "cjsh v" + c_version + " " + get_current_file_path();
}

bool PromptInfo::is_variable_used(const std::string& var_name,
                                  const std::vector<nlohmann::json>& segments) {
  if (g_debug_mode)
    std::cerr << "DEBUG: is_variable_used START: " << var_name << std::endl;

  std::string placeholder = "{" + var_name + "}";

  // Use a static cache to avoid repetitive checks for the same variables
  static std::unordered_map<std::string, bool> cache;
  static std::mutex cache_mutex;

  // Create a unique key based on the variable name and segment sizes (rough
  // approximation)
  std::string cache_key = var_name + "_" + std::to_string(segments.size());

  {
    std::lock_guard<std::mutex> lock(cache_mutex);
    auto it = cache.find(cache_key);
    if (it != cache.end()) {
      if (g_debug_mode)
        std::cerr << "DEBUG: is_variable_used END (cached): " << var_name
                  << " = " << (it->second ? "true" : "false") << std::endl;
      return it->second;
    }
  }

  // If not in cache, search through all segments
  for (const auto& segment : segments) {
    if (segment.contains("content")) {
      std::string content = segment["content"];
      if (content.find(placeholder) != std::string::npos) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        cache[cache_key] = true;
        if (g_debug_mode)
          std::cerr << "DEBUG: is_variable_used END: " << var_name << " = true"
                    << std::endl;
        return true;
      }
    }
  }

  std::lock_guard<std::mutex> lock(cache_mutex);
  cache[cache_key] = false;
  if (g_debug_mode)
    std::cerr << "DEBUG: is_variable_used END: " << var_name << " = false"
              << std::endl;
  return false;
}

bool PromptInfo::is_git_repository(std::filesystem::path& repo_root) {
  if (g_debug_mode)
    std::cerr << "DEBUG: is_git_repository START" << std::endl;

  // Cache this result using the current path as the key
  std::string current_path_str = std::filesystem::current_path().string();
  std::string cache_key = "is_git_repo_" + current_path_str;

  std::string cached_result = get_cached_value(
      cache_key,
      [this, &repo_root]() -> std::string {
        std::filesystem::path current_path = std::filesystem::current_path();
        std::filesystem::path git_head_path;

        repo_root = current_path;

        while (!is_root_path(repo_root)) {
          git_head_path = repo_root / ".git" / "HEAD";
          if (std::filesystem::exists(git_head_path)) {
            return repo_root.string() + ",true";
          }
          repo_root = repo_root.parent_path();
        }

        return "not_found,false";
      },
      300);  // Cache for 5 minutes since repo status rarely changes

  // Parse the result
  size_t comma_pos = cached_result.find(',');
  if (comma_pos != std::string::npos) {
    std::string path_str = cached_result.substr(0, comma_pos);
    std::string is_repo_str = cached_result.substr(comma_pos + 1);

    if (is_repo_str == "true" && path_str != "not_found") {
      repo_root = std::filesystem::path(path_str);
      if (g_debug_mode)
        std::cerr << "DEBUG: is_git_repository END: true, repo_root="
                  << repo_root.string() << std::endl;
      return true;
    }
  }

  if (g_debug_mode)
    std::cerr << "DEBUG: is_git_repository END: false" << std::endl;
  return false;
}

std::string PromptInfo::get_git_branch(
    const std::filesystem::path& git_head_path) {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_git_branch START: " << git_head_path.string()
              << std::endl;

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

    if (branch_name.empty()) {
      branch_name = "unknown";
    }

    if (g_debug_mode)
      std::cerr << "DEBUG: get_git_branch END: " << branch_name << std::endl;
    return branch_name;
  } catch (const std::exception& e) {
    std::cerr << "Error reading git HEAD file: " << e.what() << std::endl;
    if (g_debug_mode)
      std::cerr << "DEBUG: get_git_branch END: exception occurred" << std::endl;
    return "unknown";
  }
}

std::string PromptInfo::get_git_status(const std::filesystem::path& repo_root) {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_git_status START: " << repo_root.string()
              << std::endl;

  std::string status_symbols = "";
  std::string git_dir = repo_root.string();
  bool is_clean_repo = true;

  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                     now - last_git_status_check)
                     .count();

  // Increase cache time to 60 seconds to reduce overhead
  if ((elapsed > 60 || cached_git_dir != git_dir) &&
      !is_git_status_check_running) {
    is_git_status_check_running = true;

    // Run a faster git status command that exits as soon as it finds a change
    std::string command =
        "cd " + git_dir +
        " && git diff --no-ext-diff --quiet || echo 'modified'";

    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
      char buffer[128];
      std::string result = "";

      while (!feof(pipe)) {
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
          result += buffer;
        }
      }

      pclose(pipe);

      std::lock_guard<std::mutex> lock(git_status_mutex);
      cached_git_dir = git_dir;
      if (result.empty() || result == "\n") {
        cached_status_symbols = "✓";
        cached_is_clean_repo = true;
      } else {
        cached_status_symbols = "✖";
        cached_is_clean_repo = false;
      }
      last_git_status_check = std::chrono::steady_clock::now();
      status_symbols = cached_status_symbols;
      is_clean_repo = cached_is_clean_repo;
      is_git_status_check_running = false;
    } else {
      std::lock_guard<std::mutex> lock(git_status_mutex);
      cached_status_symbols = "?";
      cached_is_clean_repo = false;
      status_symbols = cached_status_symbols;
      is_clean_repo = cached_is_clean_repo;
      is_git_status_check_running = false;
    }
  } else {
    std::lock_guard<std::mutex> lock(git_status_mutex);
    status_symbols = cached_status_symbols;
    is_clean_repo = cached_is_clean_repo;
  }

  if (g_debug_mode)
    std::cerr << "DEBUG: get_git_status END: is_clean_repo="
              << (is_clean_repo ? "true" : "false") << std::endl;

  if (is_clean_repo) {
    return " ✓";
  } else {
    return " " + status_symbols;
  }
}

std::string PromptInfo::get_local_path(const std::filesystem::path& repo_root) {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_local_path START: repo_root=" << repo_root.string()
              << std::endl;

  std::filesystem::path cwd = std::filesystem::current_path();
  std::string repo_root_path = repo_root.string();
  std::string repo_root_name = repo_root.filename().string();
  std::string current_path_str = cwd.string();

  std::string result;
  if (current_path_str == repo_root_path) {
    result = repo_root_name;
  } else if (current_path_str.find(repo_root_path + "/") == 0) {
    std::string relative_path =
        current_path_str.substr(repo_root_path.length());
    if (!relative_path.empty() && relative_path[0] == '/') {
      relative_path = relative_path.substr(1);
    }
    relative_path =
        repo_root_name + (relative_path.empty() ? "" : "/" + relative_path);
    result = relative_path;
  } else {
    result = "/";
  }

  if (g_debug_mode)
    std::cerr << "DEBUG: get_local_path END: " << result << std::endl;
  return result;
}

bool PromptInfo::is_root_path(const std::filesystem::path& path) {
  if (g_debug_mode)
    std::cerr << "DEBUG: is_root_path: " << path.string() << " = "
              << (path == path.root_path() ? "true" : "false") << std::endl;
  return path == path.root_path();
}

std::string PromptInfo::get_current_file_path() {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_current_file_path START" << std::endl;

  std::string path = std::filesystem::current_path().string();

  if (path == "/") {
    if (g_debug_mode)
      std::cerr << "DEBUG: get_current_file_path END: /" << std::endl;
    return "/";
  }

  char* home_dir = getenv("HOME");
  if (home_dir) {
    std::string home_path = home_dir;
    if (path == home_path) {
      if (g_debug_mode)
        std::cerr << "DEBUG: get_current_file_path END: ~" << std::endl;
      return "~";
    } else if (path.find(home_path + "/") == 0) {
      std::string result = "~" + path.substr(home_path.length());
      if (g_debug_mode)
        std::cerr << "DEBUG: get_current_file_path END: " << result
                  << std::endl;
      return result;
    }
  }

  if (g_debug_mode)
    std::cerr << "DEBUG: get_current_file_path END: " << path << std::endl;
  return path;
}

std::string PromptInfo::get_current_file_name() {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_current_file_name START" << std::endl;

  std::string current_directory = get_current_file_path();

  if (current_directory == "/") {
    if (g_debug_mode)
      std::cerr << "DEBUG: get_current_file_name END: /" << std::endl;
    return "/";
  }

  if (current_directory == "~") {
    if (g_debug_mode)
      std::cerr << "DEBUG: get_current_file_name END: ~" << std::endl;
    return "~";
  }

  if (current_directory.find("~/") == 0) {
    std::string relative_path = current_directory.substr(2);
    if (relative_path.empty()) {
      if (g_debug_mode)
        std::cerr << "DEBUG: get_current_file_name END: ~" << std::endl;
      return "~";
    }
    size_t last_slash = relative_path.find_last_of('/');
    if (last_slash != std::string::npos) {
      std::string result = relative_path.substr(last_slash + 1);
      if (g_debug_mode)
        std::cerr << "DEBUG: get_current_file_name END: " << result
                  << std::endl;
      return result;
    }
    if (g_debug_mode)
      std::cerr << "DEBUG: get_current_file_name END: " << relative_path
                << std::endl;
    return relative_path;
  }

  std::string current_file_name =
      std::filesystem::path(current_directory).filename().string();
  if (current_file_name.empty()) {
    if (g_debug_mode)
      std::cerr << "DEBUG: get_current_file_name END: /" << std::endl;
    return "/";
  }

  if (g_debug_mode)
    std::cerr << "DEBUG: get_current_file_name END: " << current_file_name
              << std::endl;
  return current_file_name;
}

std::string PromptInfo::get_username() {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_username START" << std::endl;

  struct passwd* pw = getpwuid(getuid());
  std::string result = pw ? pw->pw_name : "user";

  if (g_debug_mode)
    std::cerr << "DEBUG: get_username END: " << result << std::endl;
  return result;
}

std::string PromptInfo::get_hostname() {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_hostname START" << std::endl;

  char hostname[256];
  gethostname(hostname, 256);

  if (g_debug_mode)
    std::cerr << "DEBUG: get_hostname END: " << hostname << std::endl;
  return hostname;
}

std::string PromptInfo::get_current_time(bool twelve_hour_format) {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_current_time START: twelve_hour_format="
              << (twelve_hour_format ? "true" : "false") << std::endl;

  auto now = std::chrono::system_clock::now();
  auto time_now = std::chrono::system_clock::to_time_t(now);
  struct tm time_info;
  localtime_r(&time_now, &time_info);

  std::stringstream time_stream;
  int hour = time_info.tm_hour;
  int display_hour = hour;
  std::string suffix;

  if (twelve_hour_format) {
    suffix = (hour >= 12) ? " PM" : " AM";
    display_hour = hour % 12;
    if (display_hour == 0) {
      display_hour = 12;
    }
  }

  time_stream << std::setfill('0') << std::setw(2) << display_hour << ":"
              << std::setfill('0') << std::setw(2) << time_info.tm_min << ":"
              << std::setfill('0') << std::setw(2) << time_info.tm_sec;

  if (twelve_hour_format) {
    time_stream << suffix;
  }

  std::string result = time_stream.str();
  if (g_debug_mode)
    std::cerr << "DEBUG: get_current_time END: " << result << std::endl;
  return result;
}

std::string PromptInfo::get_current_date() {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_current_date START" << std::endl;

  auto now = std::chrono::system_clock::now();
  auto time_now = std::chrono::system_clock::to_time_t(now);
  struct tm time_info;
  localtime_r(&time_now, &time_info);

  std::stringstream date_stream;
  date_stream << (time_info.tm_year + 1900) << "-" << std::setfill('0')
              << std::setw(2) << (time_info.tm_mon + 1) << "-"
              << std::setfill('0') << std::setw(2) << time_info.tm_mday;

  std::string result = date_stream.str();
  if (g_debug_mode)
    std::cerr << "DEBUG: get_current_date END: " << result << std::endl;
  return result;
}

int PromptInfo::get_current_day() {
  auto now = std::chrono::system_clock::now();
  auto time_now = std::chrono::system_clock::to_time_t(now);
  struct tm time_info;
  localtime_r(&time_now, &time_info);
  return time_info.tm_mday;
}

int PromptInfo::get_current_month() {
  auto now = std::chrono::system_clock::now();
  auto time_now = std::chrono::system_clock::to_time_t(now);
  struct tm time_info;
  localtime_r(&time_now, &time_info);
  return time_info.tm_mon + 1;
}

int PromptInfo::get_current_year() {
  auto now = std::chrono::system_clock::now();
  auto time_now = std::chrono::system_clock::to_time_t(now);
  struct tm time_info;
  localtime_r(&time_now, &time_info);
  return time_info.tm_year + 1900;
}

std::string PromptInfo::get_current_day_name() {
  static const char* day_names[] = {"Sunday",    "Monday",   "Tuesday",
                                    "Wednesday", "Thursday", "Friday",
                                    "Saturday"};
  auto now = std::chrono::system_clock::now();
  auto time_now = std::chrono::system_clock::to_time_t(now);
  struct tm time_info;
  localtime_r(&time_now, &time_info);
  return day_names[time_info.tm_wday];
}

std::string PromptInfo::get_current_month_name() {
  static const char* month_names[] = {
      "January", "February", "March",     "April",   "May",      "June",
      "July",    "August",   "September", "October", "November", "December"};
  auto now = std::chrono::system_clock::now();
  auto time_now = std::chrono::system_clock::to_time_t(now);
  struct tm time_info;
  localtime_r(&time_now, &time_info);
  return month_names[time_info.tm_mon];
}

std::string PromptInfo::get_shell() {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_shell START/END: cjsh" << std::endl;
  return "cjsh";
}

std::string PromptInfo::get_shell_version() {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_shell_version START/END: " << c_version
              << std::endl;
  return c_version;
}

int PromptInfo::get_git_ahead_behind(const std::filesystem::path& repo_root,
                                     int& ahead, int& behind) {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_git_ahead_behind START: " << repo_root.string()
              << std::endl;

  ahead = 0;
  behind = 0;

  // Cache git ahead/behind for 2 minutes as it's an expensive operation
  std::string cache_key = "git_ahead_behind_" + repo_root.string();
  std::string result = get_cached_value(
      cache_key,
      [&repo_root]() -> std::string {
        try {
          std::filesystem::path git_head_path = repo_root / ".git" / "HEAD";
          std::string branch;

          // Read HEAD file directly instead of calling get_git_branch
          std::ifstream head_file(git_head_path);
          std::string line;
          std::regex head_pattern("ref: refs/heads/(.*)");
          std::smatch match;

          while (std::getline(head_file, line)) {
            if (std::regex_search(line, match, head_pattern)) {
              branch = match[1];
              break;
            }
          }

          if (branch.empty()) {
            return "0,0";  // Return default if can't determine branch
          }

          std::string command =
              "cd " + repo_root.string() +
              " && git rev-list --left-right --count @{u}...HEAD 2>/dev/null";

          FILE* pipe = popen(("sh -c '" + command + "'").c_str(), "r");
          if (!pipe) {
            return "0,0";
          }

          char buffer[128];
          std::string cmdResult = "";

          while (!feof(pipe)) {
            if (fgets(buffer, 128, pipe) != nullptr) {
              cmdResult += buffer;
            }
          }

          pclose(pipe);

          // Trim whitespace
          cmdResult.erase(cmdResult.find_last_not_of(" \n\r\t") + 1);

          // Parse result
          std::istringstream iss(cmdResult);
          int b, a;
          iss >> b >> a;

          return std::to_string(b) + "," + std::to_string(a);
        } catch (const std::exception& e) {
          if (g_debug_mode)
            std::cerr << "DEBUG: Error getting git ahead/behind status: "
                      << e.what() << std::endl;
          return "0,0";
        }
      },
      120);  // Cache for 2 minutes

  // Parse the cached result
  size_t comma_pos = result.find(',');
  if (comma_pos != std::string::npos) {
    try {
      behind = std::stoi(result.substr(0, comma_pos));
      ahead = std::stoi(result.substr(comma_pos + 1));
      if (g_debug_mode)
        std::cerr << "DEBUG: get_git_ahead_behind END: ahead=" << ahead
                  << ", behind=" << behind << std::endl;
      return 0;
    } catch (const std::exception&) {
      if (g_debug_mode)
        std::cerr << "DEBUG: get_git_ahead_behind END: exception during parsing"
                  << std::endl;
      return -1;
    }
  }

  if (g_debug_mode)
    std::cerr << "DEBUG: get_git_ahead_behind END: failed" << std::endl;
  return -1;
}

int PromptInfo::get_git_stash_count(const std::filesystem::path& repo_root) {
  // Cache git stash count for 2 minutes
  std::string cache_key = "git_stash_" + repo_root.string();
  return std::stoi(get_cached_value(
      cache_key,
      [&repo_root]() -> std::string {
        try {
          std::string command =
              "cd " + repo_root.string() + " && git stash list | wc -l";

          FILE* pipe = popen(("sh -c '" + command + "'").c_str(), "r");
          if (!pipe) {
            return "0";
          }

          char buffer[128];
          std::string result = "";

          while (!feof(pipe)) {
            if (fgets(buffer, 128, pipe) != nullptr) {
              result += buffer;
            }
          }

          pclose(pipe);

          // Trim whitespace
          result.erase(result.find_last_not_of(" \n\r\t") + 1);

          return result;
        } catch (const std::exception&) {
          return "0";
        }
      },
      120));  // Cache for 2 minutes
}

bool PromptInfo::get_git_has_staged_changes(
    const std::filesystem::path& repo_root) {
  // Cache staged changes status for 60 seconds
  std::string cache_key = "git_staged_" + repo_root.string();
  return get_cached_value(
             cache_key,
             [&repo_root]() -> std::string {
               try {
                 std::string command =
                     "cd " + repo_root.string() +
                     " && git diff --cached --quiet && echo 0 || echo 1";

                 FILE* pipe = popen(("sh -c '" + command + "'").c_str(), "r");
                 if (!pipe) {
                   return "0";
                 }

                 char buffer[128];
                 std::string result = "";

                 while (!feof(pipe)) {
                   if (fgets(buffer, 128, pipe) != nullptr) {
                     result += buffer;
                   }
                 }

                 pclose(pipe);

                 // Trim whitespace
                 result.erase(result.find_last_not_of(" \n\r\t") + 1);

                 return result;
               } catch (const std::exception&) {
                 return "0";
               }
             },
             60) == "1";  // Cache for 60 seconds
}

int PromptInfo::get_git_uncommitted_changes(
    const std::filesystem::path& repo_root) {
  // Use a faster command and cache the result for 60 seconds
  std::string cache_key = "git_changes_" + repo_root.string();
  return std::stoi(get_cached_value(
      cache_key,
      [&repo_root]() -> std::string {
        try {
          // Use --porcelain for machine-readable output
          std::string command =
              "cd " + repo_root.string() + " && git status --porcelain | wc -l";

          FILE* pipe = popen(("sh -c '" + command + "'").c_str(), "r");
          if (!pipe) {
            return "0";
          }

          char buffer[128];
          std::string result = "";

          while (!feof(pipe)) {
            if (fgets(buffer, 128, pipe) != nullptr) {
              result += buffer;
            }
          }

          pclose(pipe);

          // Trim whitespace
          result.erase(result.find_last_not_of(" \n\r\t") + 1);

          return result;
        } catch (const std::exception&) {
          return "0";
        }
      },
      60));  // Cache for 60 seconds
}

std::unordered_map<std::string, std::string> PromptInfo::get_variables(
    const std::vector<nlohmann::json>& segments, bool is_git_repo,
    const std::filesystem::path& repo_root) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Getting prompt variables, is_git_repo=" << is_git_repo
              << std::endl;

  std::unordered_map<std::string, std::string> vars;

  // Create a set of needed variables to avoid computing unused ones
  std::unordered_set<std::string> needed_vars;
  for (const auto& segment : segments) {
    if (segment.contains("content")) {
      std::string content = segment["content"];
      // Look for placeholders like {VAR_NAME}
      std::regex placeholder_pattern("\\{([^}]+)\\}");
      std::smatch matches;
      std::string::const_iterator search_start(content.cbegin());

      while (std::regex_search(search_start, content.cend(), matches,
                               placeholder_pattern)) {
        needed_vars.insert(matches[1]);
        search_start = matches.suffix().first;
      }
    }
  }

  // Fast path for basic info that doesn't change often
  if (needed_vars.count("USERNAME")) {
    vars["USERNAME"] = get_username();
  }

  if (needed_vars.count("HOSTNAME")) {
    vars["HOSTNAME"] = get_hostname();
  }

  if (needed_vars.count("SHELL")) {
    vars["SHELL"] = get_shell();
  }

  if (needed_vars.count("SHELL_VER")) {
    vars["SHELL_VER"] = get_shell_version();
  }

  // Path information (moderate cost)
  if (needed_vars.count("PATH")) {
    vars["PATH"] = get_current_file_path();
  }

  if (needed_vars.count("DIRECTORY")) {
    vars["DIRECTORY"] = get_current_file_name();
  }

  // Time information (low cost)
  if (needed_vars.count("TIME") || needed_vars.count("TIME24")) {
    vars["TIME"] = get_current_time(false);
    vars["TIME24"] = vars["TIME"];
  }

  if (needed_vars.count("TIME12")) {
    vars["TIME12"] = get_current_time(true);
  }

  if (needed_vars.count("DATE")) {
    vars["DATE"] = get_current_date();
  }

  if (needed_vars.count("DAY")) {
    vars["DAY"] = std::to_string(get_current_day());
  }
  if (needed_vars.count("MONTH")) {
    vars["MONTH"] = std::to_string(get_current_month());
  }
  if (needed_vars.count("YEAR")) {
    vars["YEAR"] = std::to_string(get_current_year());
  }
  if (needed_vars.count("DAY_NAME")) {
    vars["DAY_NAME"] = get_current_day_name();
  }
  if (needed_vars.count("MONTH_NAME")) {
    vars["MONTH_NAME"] = get_current_month_name();
  }

  // System information (potentially high cost, already cached)
  if (needed_vars.count("OS_INFO")) {
    vars["OS_INFO"] = get_os_info();
  }

  if (needed_vars.count("KERNEL_VER")) {
    vars["KERNEL_VER"] = get_kernel_version();
  }

  if (needed_vars.count("CPU_USAGE")) {
    vars["CPU_USAGE"] = std::to_string(static_cast<int>(get_cpu_usage())) + "%";
  }

  if (needed_vars.count("MEM_USAGE")) {
    vars["MEM_USAGE"] =
        std::to_string(static_cast<int>(get_memory_usage())) + "%";
  }

  if (needed_vars.count("BATTERY")) {
    vars["BATTERY"] = get_battery_status();
  }

  if (needed_vars.count("UPTIME")) {
    vars["UPTIME"] = get_uptime();
  }

  if (needed_vars.count("TERM_TYPE")) {
    vars["TERM_TYPE"] = get_terminal_type();
  }

  if (needed_vars.count("TERM_SIZE")) {
    auto [width, height] = get_terminal_dimensions();
    vars["TERM_SIZE"] = std::to_string(width) + "x" + std::to_string(height);
  }

  // Language version checks (high cost)
  for (const auto& var_name : needed_vars) {
    if (var_name.substr(0, 9) == "LANG_VER:") {
      std::string lang = var_name.substr(9);
      vars[var_name] = get_active_language_version(lang);
    }
  }

  if (needed_vars.count("VIRTUAL_ENV")) {
    std::string env_name;
    if (is_in_virtual_environment(env_name)) {
      vars["VIRTUAL_ENV"] = env_name;
    } else {
      vars["VIRTUAL_ENV"] = "";
    }
  }

  if (needed_vars.count("BG_JOBS")) {
    int job_count = get_background_jobs_count();
    vars["BG_JOBS"] = job_count > 0 ? std::to_string(job_count) : "";
  }

  // Add last exit code placeholder
  if (needed_vars.count("STATUS")) {
    char* status_env = getenv("STATUS");
    vars["STATUS"] = status_env ? std::string(status_env) : "0";
  }

  // Network information (potentially high cost, already cached)
  if (needed_vars.count("IP_LOCAL")) {
    vars["IP_LOCAL"] = get_ip_address(false);
  }

  if (needed_vars.count("IP_EXTERNAL")) {
    vars["IP_EXTERNAL"] = get_ip_address(true);
  }

  if (needed_vars.count("VPN_STATUS")) {
    vars["VPN_STATUS"] = is_vpn_active() ? "on" : "off";
  }

  if (needed_vars.count("NET_IFACE")) {
    vars["NET_IFACE"] = get_active_network_interface();
  }

  // Git information (potentially high cost, already cached)
  if (is_git_repo) {
    std::filesystem::path git_head_path = repo_root / ".git" / "HEAD";

    if (needed_vars.count("GIT_BRANCH")) {
      vars["GIT_BRANCH"] = get_git_branch(git_head_path);
    }

    if (needed_vars.count("GIT_STATUS")) {
      vars["GIT_STATUS"] = get_git_status(repo_root);
    }

    if (needed_vars.count("LOCAL_PATH")) {
      vars["LOCAL_PATH"] = get_local_path(repo_root);
    }

    if (needed_vars.count("GIT_AHEAD") || needed_vars.count("GIT_BEHIND")) {
      int ahead = 0, behind = 0;
      if (get_git_ahead_behind(repo_root, ahead, behind) == 0) {
        vars["GIT_AHEAD"] = std::to_string(ahead);
        vars["GIT_BEHIND"] = std::to_string(behind);
      } else {
        vars["GIT_AHEAD"] = "0";
        vars["GIT_BEHIND"] = "0";
      }
    }

    if (needed_vars.count("GIT_STASHES")) {
      vars["GIT_STASHES"] = std::to_string(get_git_stash_count(repo_root));
    }

    if (needed_vars.count("GIT_STAGED")) {
      vars["GIT_STAGED"] = get_git_has_staged_changes(repo_root) ? "✓" : "";
    }

    if (needed_vars.count("GIT_CHANGES")) {
      vars["GIT_CHANGES"] =
          std::to_string(get_git_uncommitted_changes(repo_root));
    }
  }

  // Plugin variables
  if (g_plugin) {
    for (const auto& plugin_name : g_plugin->get_enabled_plugins()) {
      plugin_data* pd = g_plugin->get_plugin_data(plugin_name);
      if (!pd)
        continue;
      for (const auto& kv : pd->prompt_variables) {
        const std::string& tag = kv.first;
        auto func = kv.second;
        if (vars.find(tag) == vars.end() && needed_vars.count(tag)) {
          plugin_string_t res = func();
          std::string value;
          if (res.length > 0)
            value = std::string(res.data, res.length);
          else if (res.data)
            value = std::string(res.data);
          else
            value = "";
          if (pd->free_memory && res.data)
            pd->free_memory(res.data);
          vars[tag] = value;
        }
      }
    }
  }

  return vars;
}

int PromptInfo::get_background_jobs_count() {
  // Cache background jobs count for a short period (2 seconds)
  return std::stoi(get_cached_value(
      "bg_jobs_count",
      []() -> std::string {
        FILE* fp = popen("sh -c 'jobs -p | wc -l'", "r");
        if (!fp)
          return "0";

        char buffer[32];
        std::string result = "";
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
          result = buffer;
        }
        pclose(fp);

        // Trim whitespace
        result.erase(result.find_last_not_of(" \n\r\t") + 1);

        return result;
      },
      2));  // Cache for 2 seconds
}

std::string PromptInfo::get_os_info() {
  return get_cached_value(
      "os_info",
      []() -> std::string {
#ifdef __APPLE__
        FILE* fp = popen("sh -c 'sw_vers -productName'", "r");
        if (!fp)
          return "Unknown";

        char buffer[128];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
          result += buffer;
        }
        pclose(fp);

        if (!result.empty() && result[result.length() - 1] == '\n') {
          result.erase(result.length() - 1);
        }

        fp = popen("sh -c 'sw_vers -productVersion'", "r");
        if (fp) {
          std::string version = "";
          while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            version += buffer;
          }
          pclose(fp);

          if (!version.empty() && version[version.length() - 1] == '\n') {
            version.erase(version.length() - 1);
          }

          result += " " + version;
        }

        return result;
#elif defined(__linux__)
        FILE* fp = popen(
            "sh -c 'cat /etc/os-release | grep PRETTY_NAME | cut -d \"\\\"\" "
            "-f 2'",
            "r");
        if (!fp)
          return "Linux";

        char buffer[128];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
          result += buffer;
        }
        pclose(fp);

        // Remove newline
        if (!result.empty() && result[result.length() - 1] == '\n') {
          result.erase(result.length() - 1);
        }

        return result;
#else
        return "Unknown OS";
#endif
      },
      3600);  // OS info won't change, cache for 1 hour
}

std::string PromptInfo::get_kernel_version() {
  return get_cached_value(
      "kernel_version",
      []() -> std::string {
        FILE* fp = popen("sh -c 'uname -r'", "r");
        if (!fp)
          return "Unknown";

        char buffer[128];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
          result += buffer;
        }
        pclose(fp);

        // Remove newline
        if (!result.empty() && result[result.length() - 1] == '\n') {
          result.erase(result.length() - 1);
        }

        return result;
      },
      3600);  // Kernel version rarely changes, cache for 1 hour
}

float PromptInfo::get_cpu_usage() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Getting CPU usage" << std::endl;

  // Cache CPU usage for 5 seconds as it doesn't change that rapidly
  return std::stof(get_cached_value(
      "cpu_usage",
      []() -> std::string {
#ifdef __APPLE__
        FILE* fp = popen(
            "sh -c 'top -l 1 | grep \"CPU usage\" | awk \"{print \\$3}\" | cut "
            "-d\"%\" -f1'",
            "r");
#elif defined(__linux__)
        FILE* fp = popen(
            "sh -c 'top -bn1 | grep \"Cpu(s)\" | awk \"{print \\$2 + \\$4}\"'",
            "r");
#else
        return "0.0";
#endif

        if (!fp) {
          if (g_debug_mode)
            std::cerr << "DEBUG: Failed to popen for CPU usage" << std::endl;
          return "0.0";
        }

        char buffer[32];
        std::string resultStr = "";
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
          resultStr += buffer;
        }
        pclose(fp);

        // Remove newline if present
        if (!resultStr.empty() && resultStr[resultStr.length() - 1] == '\n') {
          resultStr.erase(resultStr.length() - 1);
        }

        return resultStr;
      },
      5));  // Cache for 5 seconds
}

float PromptInfo::get_memory_usage() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Getting memory usage" << std::endl;

  // Cache memory usage for 5 seconds as it doesn't change that rapidly
  return std::stof(get_cached_value(
      "memory_usage",
      []() -> std::string {
#ifdef __APPLE__
        FILE* fp = popen(
            "sh -c 'top -l 1 | grep PhysMem | awk \"{print \\$2}\" | cut "
            "-d\"M\" "
            "-f1'",
            "r");
#elif defined(__linux__)
        FILE* fp = popen(
            "sh -c 'free | grep Mem | awk \"{print \\$3/\\$2 * 100.0}\"'", "r");
#else
        return "0.0";
#endif

        if (!fp)
          return "0.0";

        char buffer[32];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
          result += buffer;
        }
        pclose(fp);

        // Remove newline if present
        if (!result.empty() && result[result.length() - 1] == '\n') {
          result.erase(result.length() - 1);
        }

        return result;
      },
      5));  // Cache for 5 seconds
}

std::string PromptInfo::get_battery_status() {
  // Cache battery status for 60 seconds - it doesn't need to update frequently
  return get_cached_value(
      "battery_status",
      []() -> std::string {
#ifdef __APPLE__
        // Combine commands to reduce number of popen calls
        FILE* fp = popen("sh -c 'pmset -g batt'", "r");
        if (!fp)
          return "Unknown";

        char buffer[256];
        std::string output = "";
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
          output += buffer;
        }
        pclose(fp);

        // Extract percentage
        std::string percentage = "Unknown";
        std::regex percentage_regex("(\\d+)%");
        std::smatch matches;
        if (std::regex_search(output, matches, percentage_regex)) {
          percentage = matches[1].str() + "%";
        }

        // Extract charging status
        std::string icon = "";
        if (output.find("charging") != std::string::npos) {
          icon = "⚡";
        } else if (output.find("discharging") != std::string::npos) {
          icon = "🔋";
        }

        return percentage + " " + icon;
#elif defined(__linux__)
        // Combine commands for Linux as well
        std::string capacity_path = "/sys/class/power_supply/BAT0/capacity";
        std::string status_path = "/sys/class/power_supply/BAT0/status";

        // Check if battery exists
        if (!std::filesystem::exists(capacity_path)) {
          return "Unknown";
        }

        // Read capacity file directly instead of using popen
        std::ifstream capacity_file(capacity_path);
        std::string percentage = "Unknown";
        if (capacity_file) {
          std::getline(capacity_file, percentage);
          percentage += "%";
        }

        // Read status file directly
        std::string icon = "";
        std::ifstream status_file(status_path);
        if (status_file) {
          std::string status;
          std::getline(status_file, status);
          if (status == "Charging") {
            icon = "⚡";
          } else if (status == "Discharging") {
            icon = "🔋";
          }
        }

        return percentage + " " + icon;
#else
        return "";
#endif
      },
      60);  // Cache for 60 seconds
}

std::string PromptInfo::get_uptime() {
  FILE* fp = popen(
      "sh -c 'uptime | awk \"{print \\$3 \\$4 \\$5}\" | sed \"s/,//g\"'", "r");
  if (!fp)
    return "Unknown";

  char buffer[128];
  std::string result = "";
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    result += buffer;
  }
  pclose(fp);

  // Remove newline
  if (!result.empty() && result[result.length() - 1] == '\n') {
    result.erase(result.length() - 1);
  }

  return result;
}

// Environment information implementations
std::string PromptInfo::get_terminal_type() {
  char* term = getenv("TERM");
  if (term) {
    return std::string(term);
  }
  return "Unknown";
}

std::pair<int, int> PromptInfo::get_terminal_dimensions() {
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  return {w.ws_col, w.ws_row};
}

std::string PromptInfo::get_active_language_version(
    const std::string& language) {
  std::string cmd;
  if (language == "python") {
    cmd =
        "command -v python >/dev/null 2>&1 && python --version 2>&1 || echo "
        "'Not installed'";
  } else if (language == "node" || language == "nodejs") {
    cmd =
        "command -v node >/dev/null 2>&1 && node --version 2>&1 || echo 'Not "
        "installed'";
  } else if (language == "ruby") {
    cmd =
        "command -v ruby >/dev/null 2>&1 && ruby --version 2>&1 || echo 'Not "
        "installed'";
  } else if (language == "go") {
    cmd =
        "command -v go >/dev/null 2>&1 && go version 2>&1 || echo 'Not "
        "installed'";
  } else if (language == "rust") {
    cmd =
        "command -v rustc >/dev/null 2>&1 && rustc --version 2>&1 || echo 'Not "
        "installed'";
  } else {
    return "Unknown";
  }

  FILE* fp = popen(("sh -c '" + cmd + "'").c_str(), "r");
  if (!fp)
    return "Unknown";

  char buffer[128];
  std::string result = "";
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    result += buffer;
  }
  pclose(fp);

  // Remove newline
  if (!result.empty() && result[result.length() - 1] == '\n') {
    result.erase(result.length() - 1);
  }

  // If result contains 'Not installed', return that
  if (result.find("Not installed") != std::string::npos) {
    return "Not installed";
  }

  // If result is empty, return 'Unknown'
  if (result.empty()) {
    return "Unknown";
  }

  return result;
}

bool PromptInfo::is_in_virtual_environment(std::string& env_name) {
  // Check Python virtual environment
  char* python_env = getenv("VIRTUAL_ENV");
  if (python_env) {
    std::string path(python_env);
    size_t last_slash = path.find_last_of("/\\");
    if (last_slash != std::string::npos) {
      env_name = path.substr(last_slash + 1);
    } else {
      env_name = path;
    }
    return true;
  }

  // Check Node.js nvm
  char* node_env = getenv("NVM_DIR");
  if (node_env) {
    env_name = "nvm";
    return true;
  }

  // Check Ruby rbenv
  char* ruby_env = getenv("RBENV_VERSION");
  if (ruby_env) {
    env_name = "rbenv:" + std::string(ruby_env);
    return true;
  }

  return false;
}

// Network information implementations
std::string PromptInfo::get_ip_address(bool external) {
  std::string cache_key = external ? "external_ip" : "local_ip";
  int ttl = external ? 300 : 60;  // Cache external IP longer (5 min vs 1 min)

  return get_cached_value(
      cache_key,
      [external]() -> std::string {
        if (external) {
          FILE* fp = popen("sh -c 'curl -s -m 2 icanhazip.com'",
                           "r");  // Add timeout to curl
          if (!fp)
            return "Unknown";

          char buffer[64];
          std::string result = "";
          while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            result += buffer;
          }
          pclose(fp);

          // Remove newline
          if (!result.empty() && result[result.length() - 1] == '\n') {
            result.erase(result.length() - 1);
          }

          return result;
        } else {
#ifdef __APPLE__
          FILE* fp = popen(
              "sh -c 'ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr "
              "en1'",
              "r");
#elif defined(__linux__)
          // Try multiple methods to get local IP address for better
          // compatibility
          FILE* fp = popen(
              "sh -c '"
              "hostname -I 2>/dev/null | awk \"{print \\$1}\" || "
              "ip route get 1.1.1.1 2>/dev/null | awk \"{print \\$7}\" | head "
              "-1 || "
              "ip addr show 2>/dev/null | grep -oP "
              "\"(?<=inet\\s)\\d+(\\.\\d+){3}\" | grep -v 127.0.0.1 | head -1 "
              "|| "
              "ifconfig 2>/dev/null | grep -oP "
              "\"(?<=inet\\s)\\d+(\\.\\d+){3}\" | grep -v 127.0.0.1 | head -1"
              "'",
              "r");
#else
          return "Unknown";
#endif

          if (!fp)
            return "Unknown";

          char buffer[64];
          std::string result = "";
          while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            result += buffer;
          }
          pclose(fp);

          // Remove newline
          if (!result.empty() && result[result.length() - 1] == '\n') {
            result.erase(result.length() - 1);
          }

          return result;
        }
      },
      ttl);
}

bool PromptInfo::is_vpn_active() {
  return get_cached_value(
             "vpn_active",
             []() -> std::string {
#ifdef __APPLE__
               FILE* fp = popen(
                   "sh -c 'scutil --nc list | grep Connected | wc -l'", "r");
#elif defined(__linux__)
               FILE* fp = popen(
                   "sh -c 'ip tuntap show | grep -q tun && echo 1 || echo 0'",
                   "r");
#else
               return "0";
#endif

               if (!fp)
                 return "0";

               char buffer[16];
               std::string result = "";
               while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                 result += buffer;
               }
               pclose(fp);

               // Trim whitespace
               result.erase(result.find_last_not_of(" \n\r\t") + 1);

               return result;
             },
             60) == "1" ||
         get_cached_value(
             "vpn_active", []() -> std::string { return ""; }, 60) == "true";
}

std::string PromptInfo::get_active_network_interface() {
  return get_cached_value(
      "active_network_interface",
      []() -> std::string {
#ifdef __APPLE__
        FILE* fp = popen(
            "sh -c 'route get default | grep interface | awk \"{print \\$2}\"'",
            "r");
#elif defined(__linux__)
        FILE* fp = popen(
            "sh -c 'ip route | grep default | awk \"{print \\$5}\" | head -n1'",
            "r");
#else
        return "Unknown";
#endif

        if (!fp)
          return "Unknown";

        char buffer[32];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
          result += buffer;
        }
        pclose(fp);

        // Remove newline
        if (!result.empty() && result[result.length() - 1] == '\n') {
          result.erase(result.length() - 1);
        }

        return result;
      },
      120);  // Network interface rarely changes, cache for 2 minutes
}
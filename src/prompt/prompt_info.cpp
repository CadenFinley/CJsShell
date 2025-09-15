#include "prompt_info.h"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <unordered_set>

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
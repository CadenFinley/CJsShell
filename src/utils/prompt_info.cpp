#include "prompt_info.h"
#include "main.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <iomanip>
#include <ctime>
#include <sstream>

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
 * 
 * System information placeholders:
 * {OS_INFO}     - Operating system name and version
 * {KERNEL_VER}  - Kernel version
 * {CPU_USAGE}   - Current CPU usage percentage
 * {MEM_USAGE}   - Current memory usage percentage
 * {BATTERY}     - Battery percentage and charging status
 * {UPTIME}      - System uptime
 * 
 * Environment information placeholders:
 * {TERM_TYPE}   - Terminal type (e.g., xterm, screen)
 * {TERM_SIZE}   - Terminal dimensions (columns x rows)
 * {LANG_VER:X}  - Version of language X (python, node, ruby, go, rust)
 * {VIRTUAL_ENV} - Name of active virtual environment, if any
 * {BG_JOBS}     - Number of background jobs
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
 */

PromptInfo::PromptInfo() {
    last_git_status_check = std::chrono::steady_clock::now() - std::chrono::seconds(30);
    is_git_status_check_running = false;
    cached_is_clean_repo = true;
}

PromptInfo::~PromptInfo() {
}

bool PromptInfo::is_variable_used(const std::string& var_name, const std::vector<nlohmann::json>& segments) {
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

bool PromptInfo::is_git_repository(std::filesystem::path& repo_root) {
    if (g_debug_mode) std::cerr << "DEBUG: Checking if path is git repository" << std::endl;
    
    std::filesystem::path current_path = std::filesystem::current_path();
    std::filesystem::path git_head_path;
    
    repo_root = current_path;
    
    while (!is_root_path(repo_root)) {
        git_head_path = repo_root / ".git" / "HEAD";
        if (std::filesystem::exists(git_head_path)) {
            return true;
        }
        repo_root = repo_root.parent_path();
    }
    
    return false;
}

std::string PromptInfo::get_git_branch(const std::filesystem::path& git_head_path) {
    if (g_debug_mode) std::cerr << "DEBUG: Getting git branch from " << git_head_path.string() << std::endl;
    
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
        
        if(branch_name.empty()) {
            branch_name = "unknown";
        }
        
        return branch_name;
    } catch (const std::exception& e) {
        std::cerr << "Error reading git HEAD file: " << e.what() << std::endl;
        return "unknown";
    }
}

std::string PromptInfo::get_git_status(const std::filesystem::path& repo_root) {
    if (g_debug_mode) std::cerr << "DEBUG: Getting git status for " << repo_root.string() << std::endl;
    
    std::string status_symbols = "";
    std::string git_dir = repo_root.string();
    bool is_clean_repo = true;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_git_status_check).count();
    
    if ((elapsed > 30 || cached_git_dir != git_dir) && !is_git_status_check_running) {
        is_git_status_check_running = true;
        std::string command = "cd " + git_dir + " && git status --porcelain";
        
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
            
            // Lock to update cache
            std::lock_guard<std::mutex> lock(git_status_mutex);
            cached_git_dir = git_dir;
            if (result.empty()) {
                // No changes detected, repo is clean
                cached_status_symbols = "✓";
                cached_is_clean_repo = true;
            } else {
                // Changes detected, repo is dirty
                cached_status_symbols = "*";
                cached_is_clean_repo = false;
            }
            
            last_git_status_check = std::chrono::steady_clock::now();
            status_symbols = cached_status_symbols;
            is_clean_repo = cached_is_clean_repo;
            is_git_status_check_running = false;
        } else {
            // Command failed, revert to default
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
    
    if (is_clean_repo) {
        return " ✓";
    } else {
        return " " + status_symbols;
    }
}

std::string PromptInfo::get_local_path(const std::filesystem::path& repo_root) {
    std::filesystem::path cwd = std::filesystem::current_path();
    std::string repo_root_path = repo_root.string();
    std::string repo_root_name = repo_root.filename().string();
    std::string current_path_str = cwd.string();
    
    if (current_path_str == repo_root_path) {
        return repo_root_name;
    } else if (current_path_str.find(repo_root_path + "/") == 0) {
        std::string relative_path = current_path_str.substr(repo_root_path.length());
        if (!relative_path.empty() && relative_path[0] == '/') {
            relative_path = relative_path.substr(1);
        }
        relative_path = repo_root_name + (relative_path.empty() ? "" : "/" + relative_path);
        return relative_path;
    } else {
        return "/";
    }
}

bool PromptInfo::is_root_path(const std::filesystem::path& path) {
    return path == path.root_path();
}

std::string PromptInfo::get_current_file_path() {
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

std::string PromptInfo::get_current_file_name() {
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

std::string PromptInfo::get_username() {
    struct passwd *pw = getpwuid(getuid());
    return pw ? pw->pw_name : "user";
}

std::string PromptInfo::get_hostname() {
    char hostname[256];
    gethostname(hostname, 256);
    return hostname;
}

std::string PromptInfo::get_current_time(bool twelve_hour_format) {
    auto now       = std::chrono::system_clock::now();
    auto time_now  = std::chrono::system_clock::to_time_t(now);
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

    return time_stream.str();
}

std::string PromptInfo::get_current_date() {
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

std::string PromptInfo::get_shell() {
    return "cjsh";
}

std::string PromptInfo::get_shell_version() {
    return c_version;
}

int PromptInfo::get_git_ahead_behind(const std::filesystem::path& repo_root, int& ahead, int& behind) {
    if (g_debug_mode) std::cerr << "DEBUG: Getting git ahead/behind for " << repo_root.string() << std::endl;
    
    ahead = 0;
    behind = 0;
    
    try {
        std::filesystem::path git_head_path = repo_root / ".git" / "HEAD";
        std::string branch = get_git_branch(git_head_path);
        
        if (branch == "unknown") {
            if (g_debug_mode) std::cerr << "DEBUG: Unknown branch, cannot get ahead/behind" << std::endl;
            return -1;
        }
        
        // Get the current branch's remote tracking branch info
        std::string command = "cd " + repo_root.string() + 
                              " && git rev-list --left-right --count @{u}...HEAD 2>/dev/null";
        
        FILE* pipe = popen(("sh -c '" + command + "'").c_str(), "r");
        if (!pipe) {
            return -1;
        }
        
        char buffer[128];
        std::string result = "";
        
        while (!feof(pipe)) {
            if (fgets(buffer, 128, pipe) != nullptr) {
                result += buffer;
            }
        }
        
        pclose(pipe);
        
        // Parse the output which is in format "behind ahead"
        std::istringstream iss(result);
        iss >> behind >> ahead;
        
        if (g_debug_mode) std::cerr << "DEBUG: Git ahead/behind result: ahead=" << ahead << ", behind=" << behind << std::endl;
        return 0;
    } catch (const std::exception& e) {
        if (g_debug_mode) std::cerr << "DEBUG: Error getting git ahead/behind status: " << e.what() << std::endl;
        return -1;
    }
}

int PromptInfo::get_git_stash_count(const std::filesystem::path& repo_root) {
    try {
        std::string command = "cd " + repo_root.string() + 
                              " && git stash list | wc -l";
        
        FILE* pipe = popen(("sh -c '" + command + "'").c_str(), "r");
        if (!pipe) {
            return 0;
        }
        
        char buffer[128];
        std::string result = "";
        
        while (!feof(pipe)) {
            if (fgets(buffer, 128, pipe) != nullptr) {
                result += buffer;
            }
        }
        
        pclose(pipe);
        
        // Parse the output to get stash count
        return std::stoi(result);
    } catch (const std::exception& e) {
        std::cerr << "Error getting git stash count: " << e.what() << std::endl;
        return 0;
    }
}

bool PromptInfo::get_git_has_staged_changes(const std::filesystem::path& repo_root) {
    try {
        std::string command = "cd " + repo_root.string() + 
                              " && git diff --cached --quiet && echo 0 || echo 1";
        
        FILE* pipe = popen(("sh -c '" + command + "'").c_str(), "r");
        if (!pipe) {
            return false;
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
        
        return result == "1";
    } catch (const std::exception& e) {
        std::cerr << "Error checking git staged changes: " << e.what() << std::endl;
        return false;
    }
}

int PromptInfo::get_git_uncommitted_changes(const std::filesystem::path& repo_root) {
    try {
        std::string command = "cd " + repo_root.string() + 
                              " && git status --porcelain | wc -l";
        
        FILE* pipe = popen(("sh -c '" + command + "'").c_str(), "r");
        if (!pipe) {
            return 0;
        }
        
        char buffer[128];
        std::string result = "";
        
        while (!feof(pipe)) {
            if (fgets(buffer, 128, pipe) != nullptr) {
                result += buffer;
            }
        }
        
        pclose(pipe);
        
        // Parse the output to get change count
        return std::stoi(result);
    } catch (const std::exception& e) {
        std::cerr << "Error getting git uncommitted changes: " << e.what() << std::endl;
        return 0;
    }
}

std::unordered_map<std::string, std::string> PromptInfo::get_variables(
    const std::vector<nlohmann::json>& segments, 
    bool is_git_repo, 
    const std::filesystem::path& repo_root) {
    if (g_debug_mode) std::cerr << "DEBUG: Getting prompt variables, is_git_repo=" << is_git_repo << std::endl;
    
    std::unordered_map<std::string, std::string> vars;
    
    // Only calculate needed variables
    if (is_variable_used("USERNAME", segments)) {
        vars["USERNAME"] = get_username();
    }
    
    if (is_variable_used("HOSTNAME", segments)) {
        vars["HOSTNAME"] = get_hostname();
    }
    
    if (is_variable_used("PATH", segments)) {
        vars["PATH"] = get_current_file_path();
    }
    
    if (is_variable_used("DIRECTORY", segments)) {
        vars["DIRECTORY"] = get_current_file_name();
    }
    
    if (is_variable_used("TIME", segments)) {
        vars["TIME"] = get_current_time(false);
    }

    if (is_variable_used("TIME12", segments)) {
      vars["TIME12"] = get_current_time(true);
  }

    if (is_variable_used("TIME", segments)) {
      vars["TIME"] = get_current_time();
    }
    
    if (is_variable_used("DATE", segments)) {
        vars["DATE"] = get_current_date();
    }
    
    if (is_variable_used("SHELL", segments)) {
        vars["SHELL"] = get_shell();
    }
    
    if (is_variable_used("SHELL_VER", segments)) {
        vars["SHELL_VER"] = get_shell_version();
    }
    
    if (is_variable_used("OS_INFO", segments)) {
        vars["OS_INFO"] = get_os_info();
    }
    
    if (is_variable_used("KERNEL_VER", segments)) {
        vars["KERNEL_VER"] = get_kernel_version();
    }
    
    if (is_variable_used("CPU_USAGE", segments)) {
        vars["CPU_USAGE"] = std::to_string(static_cast<int>(get_cpu_usage())) + "%";
    }
    
    if (is_variable_used("MEM_USAGE", segments)) {
        vars["MEM_USAGE"] = std::to_string(static_cast<int>(get_memory_usage())) + "%";
    }
    
    if (is_variable_used("BATTERY", segments)) {
        vars["BATTERY"] = get_battery_status();
    }
    
    if (is_variable_used("UPTIME", segments)) {
        vars["UPTIME"] = get_uptime();
    }
    
    // Environment information variables
    if (is_variable_used("TERM_TYPE", segments)) {
        vars["TERM_TYPE"] = get_terminal_type();
    }
    
    if (is_variable_used("TERM_SIZE", segments)) {
        auto [width, height] = get_terminal_dimensions();
        vars["TERM_SIZE"] = std::to_string(width) + "x" + std::to_string(height);
    }
    
    // Check for language version variables like {LANG_VER:python}
    for (const auto& segment : segments) {
        if (segment.contains("content")) {
            std::string content = segment["content"];
            std::regex lang_pattern("\\{LANG_VER:([^}]+)\\}");
            std::smatch match;
            std::string::const_iterator search_start(content.cbegin());
            
            while (std::regex_search(search_start, content.cend(), match, lang_pattern)) {
                std::string lang = match[1];
                vars["LANG_VER:" + lang] = get_active_language_version(lang);
                search_start = match.suffix().first;
            }
        }
    }
    
    if (is_variable_used("VIRTUAL_ENV", segments)) {
        std::string env_name;
        if (is_in_virtual_environment(env_name)) {
            vars["VIRTUAL_ENV"] = env_name;
        } else {
            vars["VIRTUAL_ENV"] = "";
        }
    }
    
    if (is_variable_used("BG_JOBS", segments)) {
        int job_count = get_background_jobs_count();
        vars["BG_JOBS"] = job_count > 0 ? std::to_string(job_count) : "";
    }
    
    // Network information variables
    if (is_variable_used("IP_LOCAL", segments)) {
        vars["IP_LOCAL"] = get_ip_address(false);
    }
    
    if (is_variable_used("IP_EXTERNAL", segments)) {
        vars["IP_EXTERNAL"] = get_ip_address(true);
    }
    
    if (is_variable_used("VPN_STATUS", segments)) {
        vars["VPN_STATUS"] = is_vpn_active() ? "on" : "off";
    }
    
    if (is_variable_used("NET_IFACE", segments)) {
        vars["NET_IFACE"] = get_active_network_interface();
    }
    
    if (is_git_repo) {
        std::filesystem::path git_head_path = repo_root / ".git" / "HEAD";
        
        if (is_variable_used("GIT_BRANCH", segments)) {
            vars["GIT_BRANCH"] = get_git_branch(git_head_path);
        }
        
        if (is_variable_used("GIT_STATUS", segments)) {
            vars["GIT_STATUS"] = get_git_status(repo_root);
        }
        
        if (is_variable_used("LOCAL_PATH", segments)) {
            vars["LOCAL_PATH"] = get_local_path(repo_root);
        }
        
        // Add new git variables
        if (is_variable_used("GIT_AHEAD", segments) || is_variable_used("GIT_BEHIND", segments)) {
            int ahead = 0, behind = 0;
            if (get_git_ahead_behind(repo_root, ahead, behind) == 0) {
                vars["GIT_AHEAD"] = std::to_string(ahead);
                vars["GIT_BEHIND"] = std::to_string(behind);
            } else {
                vars["GIT_AHEAD"] = "0";
                vars["GIT_BEHIND"] = "0";
            }
        }
        
        if (is_variable_used("GIT_STASHES", segments)) {
            vars["GIT_STASHES"] = std::to_string(get_git_stash_count(repo_root));
        }
        
        if (is_variable_used("GIT_STAGED", segments)) {
            vars["GIT_STAGED"] = get_git_has_staged_changes(repo_root) ? "✓" : "";
        }
        
        if (is_variable_used("GIT_CHANGES", segments)) {
            vars["GIT_CHANGES"] = std::to_string(get_git_uncommitted_changes(repo_root));
        }
    }
    
    return vars;
}

int PromptInfo::get_background_jobs_count() {
    // This function needs to count the number of background jobs
    // We'll use a system command to get the count of background jobs
    FILE* fp = popen("sh -c 'jobs -p | wc -l'", "r");
    if (!fp) return 0;
    
    char buffer[32];
    std::string result = "";
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        result = buffer;
    }
    pclose(fp);
    
    // Trim whitespace
    result.erase(result.find_last_not_of(" \n\r\t") + 1);
    
    try {
        return std::stoi(result);
    } catch (const std::exception&) {
        return 0;
    }
}

// System information implementations
std::string PromptInfo::get_os_info() {
    return get_cached_value("os_info", []() -> std::string {
        #ifdef __APPLE__
            FILE* fp = popen("sh -c 'sw_vers -productName'", "r");
            if (!fp) return "Unknown";
            
            char buffer[128];
            std::string result = "";
            while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                result += buffer;
            }
            pclose(fp);
            
            // Remove newline
            if (!result.empty() && result[result.length()-1] == '\n') {
                result.erase(result.length()-1);
            }
            
            // Get version
            fp = popen("sh -c 'sw_vers -productVersion'", "r");
            if (fp) {
                std::string version = "";
                while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                    version += buffer;
                }
                pclose(fp);
                
                // Remove newline
                if (!version.empty() && version[version.length()-1] == '\n') {
                    version.erase(version.length()-1);
                }
                
                result += " " + version;
            }
            
            return result;
        #elif defined(__linux__)
            FILE* fp = popen("sh -c 'cat /etc/os-release | grep PRETTY_NAME | cut -d \"\\\"\" -f 2'", "r");
            if (!fp) return "Linux";
            
            char buffer[128];
            std::string result = "";
            while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                result += buffer;
            }
            pclose(fp);
            
            // Remove newline
            if (!result.empty() && result[result.length()-1] == '\n') {
                result.erase(result.length()-1);
            }
            
            return result;
        #else
            return "Unknown OS";
        #endif
    }, 3600); // OS info won't change, cache for 1 hour
}

std::string PromptInfo::get_kernel_version() {
    return get_cached_value("kernel_version", []() -> std::string {
        FILE* fp = popen("sh -c 'uname -r'", "r");
        if (!fp) return "Unknown";
        
        char buffer[128];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            result += buffer;
        }
        pclose(fp);
        
        // Remove newline
        if (!result.empty() && result[result.length()-1] == '\n') {
            result.erase(result.length()-1);
        }
        
        return result;
    }, 3600); // Kernel version rarely changes, cache for 1 hour
}

float PromptInfo::get_cpu_usage() {
    if (g_debug_mode) std::cerr << "DEBUG: Getting CPU usage" << std::endl;
    
    #ifdef __APPLE__
        FILE* fp = popen("sh -c 'top -l 1 | grep \"CPU usage\" | awk \"{print \\$3}\" | cut -d\"%\" -f1'", "r");
    #elif defined(__linux__)
        FILE* fp = popen("sh -c 'top -bn1 | grep \"Cpu(s)\" | awk \"{print \\$2 + \\$4}\"'", "r");
    #else
        return 0.0f;
    #endif
    
    if (!fp) {
        if (g_debug_mode) std::cerr << "DEBUG: Failed to popen for CPU usage" << std::endl;
        return 0.0f;
    }
    
    char buffer[32];
    std::string resultStr = "";
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        resultStr += buffer;
    }
    pclose(fp);
    
    try {
        float result = std::stof(resultStr);
        if (g_debug_mode) std::cerr << "DEBUG: CPU usage is " << result << "%" << std::endl;
        return result;
    } catch (const std::exception& e) {
        if (g_debug_mode) std::cerr << "DEBUG: Failed to parse CPU usage: " << e.what() << std::endl;
        return 0.0f;
    }
}

float PromptInfo::get_memory_usage() {
    if (g_debug_mode) std::cerr << "DEBUG: Getting memory usage" << std::endl;
    
    #ifdef __APPLE__
        FILE* fp = popen("sh -c 'top -l 1 | grep PhysMem | awk \"{print \\$2}\" | cut -d\"M\" -f1'", "r");
    #elif defined(__linux__)
        FILE* fp = popen("sh -c 'free | grep Mem | awk \"{print \\$3/\\$2 * 100.0}\"'", "r");
    #else
        return 0.0f;
    #endif
    
    if (!fp) return 0.0f;
    
    char buffer[32];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        result += buffer;
    }
    pclose(fp);
    
    try {
        return std::stof(result);
    } catch (const std::exception&) {
        return 0.0f;
    }
}

std::string PromptInfo::get_battery_status() {
    #ifdef __APPLE__
        FILE* fp = popen("sh -c 'pmset -g batt | grep -Eo \"\\\\d+%\"'", "r");
        if (!fp) return "Unknown";
        
        char buffer[32];
        std::string percentage = "";
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            percentage = buffer;
        }
        pclose(fp);
        
        // Remove newline
        if (!percentage.empty() && percentage[percentage.length()-1] == '\n') {
            percentage.erase(percentage.length()-1);
        }
        
        // Get charging status
        fp = popen("sh -c 'pmset -g batt | grep -Eo \";.*\" | cut -d \";\" -f2 | cut -d \" \" -f2'", "r");
        if (!fp) return percentage;
        
        std::string status = "";
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            status = buffer;
        }
        pclose(fp);
        
        // Remove newline
        if (!status.empty() && status[status.length()-1] == '\n') {
            status.erase(status.length()-1);
        }
        
        std::string icon = "";
        if (status == "charging") {
            icon = "⚡";
        } else if (status == "discharging") {
            icon = "🔋";
        }
        
        return percentage + " " + icon;
    #elif defined(__linux__)
        FILE* fp = popen("sh -c 'cat /sys/class/power_supply/BAT0/capacity 2>/dev/null'", "r");
        if (!fp) return "Unknown";
        
        char buffer[32];
        std::string percentage = "";
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            percentage = buffer;
        }
        pclose(fp);
        
        // Remove newline
        if (!percentage.empty() && percentage[percentage.length()-1] == '\n') {
            percentage.erase(percentage.length()-1);
        }
        
        // Get charging status
        fp = popen("sh -c 'cat /sys/class/power_supply/BAT0/status 2>/dev/null'", "r");
        if (!fp) return percentage + "%";
        
        std::string status = "";
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            status = buffer;
        }
        pclose(fp);
        
        // Remove newline
        if (!status.empty() && status[status.length()-1] == '\n') {
            status.erase(status.length()-1);
        }
        
        std::string icon = "";
        if (status == "Charging") {
            icon = "⚡";
        } else if (status == "Discharging") {
            icon = "🔋";
        }
        
        return percentage + "% " + icon;
    #else
        return "Unknown";
    #endif
}

std::string PromptInfo::get_uptime() {
    FILE* fp = popen("sh -c 'uptime | awk \"{print \\$3 \\$4 \\$5}\" | sed \"s/,//g\"'", "r");
    if (!fp) return "Unknown";
    
    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        result += buffer;
    }
    pclose(fp);
    
    // Remove newline
    if (!result.empty() && result[result.length()-1] == '\n') {
        result.erase(result.length()-1);
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

std::string PromptInfo::get_active_language_version(const std::string& language) {
    std::string cmd;
    if (language == "python") {
        cmd = "python --version 2>&1";
    } else if (language == "node" || language == "nodejs") {
        cmd = "node --version";
    } else if (language == "ruby") {
        cmd = "ruby --version | awk '{print $2}'";
    } else if (language == "go") {
        cmd = "go version | awk '{print $3}' | sed 's/go//'";
    } else if (language == "rust") {
        cmd = "rustc --version | awk '{print $2}'";
    } else {
        return "Unknown";
    }
    
    FILE* fp = popen(("sh -c '" + cmd + "'").c_str(), "r");
    if (!fp) return "Unknown";
    
    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        result += buffer;
    }
    pclose(fp);
    
    // Remove newline
    if (!result.empty() && result[result.length()-1] == '\n') {
        result.erase(result.length()-1);
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
    int ttl = external ? 300 : 60; // Cache external IP longer (5 min vs 1 min)
    
    return get_cached_value(cache_key, [external]() -> std::string {
        if (external) {
            FILE* fp = popen("sh -c 'curl -s -m 2 icanhazip.com'", "r"); // Add timeout to curl
            if (!fp) return "Unknown";
            
            char buffer[64];
            std::string result = "";
            while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                result += buffer;
            }
            pclose(fp);
            
            // Remove newline
            if (!result.empty() && result[result.length()-1] == '\n') {
                result.erase(result.length()-1);
            }
            
            return result;
        } else {
            #ifdef __APPLE__
                FILE* fp = popen("sh -c 'ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr en1'", "r");
            #elif defined(__linux__)
                FILE* fp = popen("sh -c 'hostname -I | awk \"{print \\$1}\"'", "r");
            #else
                return "Unknown";
            #endif
            
            if (!fp) return "Unknown";
            
            char buffer[64];
            std::string result = "";
            while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                result += buffer;
            }
            pclose(fp);
            
            // Remove newline
            if (!result.empty() && result[result.length()-1] == '\n') {
                result.erase(result.length()-1);
            }
            
            return result;
        }
    }, ttl);
}

bool PromptInfo::is_vpn_active() {
    return get_cached_value("vpn_active", []() -> std::string {
        #ifdef __APPLE__
            FILE* fp = popen("sh -c 'scutil --nc list | grep Connected | wc -l'", "r");
        #elif defined(__linux__)
            FILE* fp = popen("sh -c 'ip tuntap show | grep -q tun && echo 1 || echo 0'", "r");
        #else
            return "0";
        #endif
        
        if (!fp) return "0";
        
        char buffer[16];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            result += buffer;
        }
        pclose(fp);
        
        // Trim whitespace
        result.erase(result.find_last_not_of(" \n\r\t") + 1);
        
        return result;
    }, 60) == "1" || get_cached_value("vpn_active", []() -> std::string { return ""; }, 60) == "true";
}

std::string PromptInfo::get_active_network_interface() {
    return get_cached_value("active_network_interface", []() -> std::string {
        #ifdef __APPLE__
            FILE* fp = popen("sh -c 'route get default | grep interface | awk \"{print \\$2}\"'", "r");
        #elif defined(__linux__)
            FILE* fp = popen("sh -c 'ip route | grep default | awk \"{print \\$5}\" | head -n1'", "r");
        #else
            return "Unknown";
        #endif
        
        if (!fp) return "Unknown";
        
        char buffer[32];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            result += buffer;
        }
        pclose(fp);
        
        // Remove newline
        if (!result.empty() && result[result.length()-1] == '\n') {
            result.erase(result.length()-1);
        }
        
        return result;
    }, 120); // Network interface rarely changes, cache for 2 minutes
}
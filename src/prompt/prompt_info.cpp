#include "prompt_info.h"

#include <pwd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#include "ai.h"
#include "cjsh.h"
#include "plugin.h"
#include "pluginapi.h"
#include "theme_parser.h"

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
 * Directory placeholders:
 * {DISPLAY_DIR} - Enhanced directory display with repo/home contraction
 * {TRUNCATED_PATH} - Truncated path with symbol
 * {REPO_PATH}  - Repository-relative path
 * {DIR_TRUNCATED} - Whether directory display is truncated (true/false)
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
 * Command placeholders:
 * {CMD_DURATION} - Duration of last command (formatted)
 * {CMD_DURATION_MS} - Duration of last command in milliseconds
 * {EXIT_CODE}  - Last command exit code
 * {EXIT_SYMBOL} - Exit status symbol (✓ for success, ✗ for failure)
 * {CMD_SUCCESS} - Whether last command was successful (true/false)
 *
 * Language detection placeholders:
 * {PYTHON_VERSION} - Python version if in Python project
 * {NODEJS_VERSION} - Node.js version if in Node.js project
 * {RUST_VERSION} - Rust version if in Rust project
 * {GOLANG_VERSION} - Go version if in Go project
 * {JAVA_VERSION} - Java version if in Java project
 * {LANGUAGE_VERSIONS} - Combined language versions (only shows detected
 * projects) {PYTHON_VENV} - Python virtual environment name {NODEJS_PM} -
 * Node.js package manager (npm, yarn, pnpm) {IS_PYTHON_PROJECT} - Whether
 * current directory is a Python project {IS_NODEJS_PROJECT} - Whether current
 * directory is a Node.js project {IS_RUST_PROJECT} - Whether current directory
 * is a Rust project {IS_GOLANG_PROJECT} - Whether current directory is a Go
 * project {IS_JAVA_PROJECT} - Whether current directory is a Java project
 *
 * Container placeholders:
 * {CONTAINER_NAME} - Name of container (Docker, Podman, etc.)
 * {CONTAINER_TYPE} - Type of container technology
 * {IS_CONTAINER} - Whether running in a container (true/false)
 * {DOCKER_CONTEXT} - Docker context name
 * {DOCKER_IMAGE} - Docker image name if available
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
    std::string username = get_username();
    std::string hostname = get_hostname();
    std::string cwd = get_current_file_path();

    size_t size = username.length() + hostname.length() + cwd.length() + 10;
    std::string prompt;
    prompt.reserve(size);

    prompt = username + "@" + hostname + " : " + cwd + " $ ";
    return prompt;
}

std::string PromptInfo::get_basic_ai_prompt() {
    std::string cwd = get_current_file_path();
    std::string ai_model = g_ai->get_model();
    std::string ai_context = g_ai->get_save_directory();
    std::string ai_type = g_ai->get_assistant_type();
    std::string ai_context_comparison =
        (std::filesystem::current_path().string() + "/" == ai_context) ? "✔" : "✖";

    size_t size = ai_model.length() + ai_context.length() + cwd.length() + ai_type.length() +
                  ai_context_comparison.length() + 10;
    std::string prompt;
    prompt.reserve(size);

    prompt = ai_model + " " + ai_context + " " + ai_context_comparison + " " + cwd + " " + ai_type +
             " > ";
    return prompt;
}

std::string PromptInfo::get_basic_title() {
    return "cjsh v" + get_version() + " " + get_current_file_path();
}

bool PromptInfo::is_variable_used(const std::string& var_name,
                                  const std::vector<ThemeSegment>& segments) {
    std::string placeholder = "{" + var_name + "}";

    static std::unordered_map<std::string, bool> cache;
    static std::mutex cache_mutex;
    static constexpr size_t MAX_CACHE_SIZE = 10;

    std::string cache_key = var_name + "_" + std::to_string(segments.size());

    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        if (cache.size() > MAX_CACHE_SIZE) {
            cache.clear();
        }
        
        auto it = cache.find(cache_key);
        if (it != cache.end()) {
            return it->second;
        }
    }

    for (const auto& segment : segments) {
        if (segment.content.find(placeholder) != std::string::npos) {
            std::lock_guard<std::mutex> lock(cache_mutex);
            cache[cache_key] = true;
            return true;
        }
    }

    std::lock_guard<std::mutex> lock(cache_mutex);
    cache[cache_key] = false;
    return false;
}

std::unordered_map<std::string, std::string> PromptInfo::get_variables(
    const std::vector<ThemeSegment>& segments, bool is_git_repo,
    const std::filesystem::path& repo_root) {
    static std::unordered_map<std::string, std::pair<bool, std::chrono::steady_clock::time_point>>
        language_cache;
    static std::unordered_map<std::string,
                              std::pair<std::string, std::chrono::steady_clock::time_point>>
        version_cache;
    static std::mutex cache_mutex;
    static const std::chrono::seconds CACHE_DURATION(30);
    static constexpr size_t MAX_CACHE_SIZE = 100;

    auto now = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        if (language_cache.size() > MAX_CACHE_SIZE) {
            for (auto it = language_cache.begin(); it != language_cache.end();) {
                if ((now - it->second.second) >= CACHE_DURATION) {
                    it = language_cache.erase(it);
                } else {
                    ++it;
                }
            }
            
            if (language_cache.size() > MAX_CACHE_SIZE) {
                std::vector<std::pair<std::string, std::chrono::steady_clock::time_point>> entries;
                entries.reserve(language_cache.size());
                for (const auto& [key, value] : language_cache) {
                    entries.emplace_back(key, value.second);
                }
                std::sort(entries.begin(), entries.end(),
                         [](const auto& a, const auto& b) { return a.second < b.second; });
                
                size_t to_remove = language_cache.size() - (MAX_CACHE_SIZE * 3 / 4);
                for (size_t i = 0; i < to_remove && i < entries.size(); ++i) {
                    language_cache.erase(entries[i].first);
                }
            }
        }
        
        if (version_cache.size() > MAX_CACHE_SIZE) {
            for (auto it = version_cache.begin(); it != version_cache.end();) {
                if ((now - it->second.second) >= CACHE_DURATION) {
                    it = version_cache.erase(it);
                } else {
                    ++it;
                }
            }
            
            if (version_cache.size() > MAX_CACHE_SIZE) {
                std::vector<std::pair<std::string, std::chrono::steady_clock::time_point>> entries;
                entries.reserve(version_cache.size());
                for (const auto& [key, value] : version_cache) {
                    entries.emplace_back(key, value.second);
                }
                std::sort(entries.begin(), entries.end(),
                         [](const auto& a, const auto& b) { return a.second < b.second; });
                
                size_t to_remove = version_cache.size() - (MAX_CACHE_SIZE * 3 / 4);
                for (size_t i = 0; i < to_remove && i < entries.size(); ++i) {
                    version_cache.erase(entries[i].first);
                }
            }
        }
    }

    std::unordered_map<std::string, std::string> vars;

    std::unordered_set<std::string> needed_vars;
    for (const auto& segment : segments) {
        if (!segment.content.empty()) {
            std::string content = segment.content;

            std::regex placeholder_pattern("\\{([^}]+)\\}");
            std::smatch matches;
            std::string::const_iterator search_start(content.cbegin());

            while (std::regex_search(search_start, content.cend(), matches, placeholder_pattern)) {
                std::string var_name = matches[1];
                needed_vars.insert(var_name);

                if (var_name.substr(0, 3) == "if ") {
                    std::regex nested_var_pattern("\\{([A-Z_]+)\\}");
                    std::smatch nested_matches;
                    std::string conditional = matches[0];
                    std::string::const_iterator nested_search_start(conditional.cbegin());

                    while (std::regex_search(nested_search_start, conditional.cend(),
                                             nested_matches, nested_var_pattern)) {
                        needed_vars.insert(nested_matches[1]);
                        nested_search_start = nested_matches.suffix().first;
                    }
                }

                search_start = matches.suffix().first;
            }
        }
    }

    std::vector<std::pair<std::string, std::string>> lang_dependencies = {
        {"PYTHON_VERSION", "IS_PYTHON_PROJECT"}, {"NODEJS_VERSION", "IS_NODEJS_PROJECT"},
        {"RUST_VERSION", "IS_RUST_PROJECT"},     {"GOLANG_VERSION", "IS_GOLANG_PROJECT"},
        {"JAVA_VERSION", "IS_JAVA_PROJECT"},     {"CPP_VERSION", "IS_CPP_PROJECT"},
        {"CSHARP_VERSION", "IS_CSHARP_PROJECT"}, {"PHP_VERSION", "IS_PHP_PROJECT"},
        {"RUBY_VERSION", "IS_RUBY_PROJECT"},     {"KOTLIN_VERSION", "IS_KOTLIN_PROJECT"},
        {"SWIFT_VERSION", "IS_SWIFT_PROJECT"},   {"DART_VERSION", "IS_DART_PROJECT"},
        {"SCALA_VERSION", "IS_SCALA_PROJECT"}};

    for (const auto& [version_var, project_var] : lang_dependencies) {
        if (needed_vars.count(version_var)) {
            needed_vars.insert(project_var);
        }
        if (needed_vars.count(project_var)) {
            needed_vars.insert(version_var);
        }
    }

    auto get_cached_language_detection = [&](const std::string& lang) -> bool {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto key = std::filesystem::current_path().string() + "_" + lang;
        auto it = language_cache.find(key);

        if (it != language_cache.end() && (now - it->second.second) < CACHE_DURATION) {
            return it->second.first;
        }

        bool result = false;
        if (lang == "python")
            result = is_python_project();
        else if (lang == "nodejs")
            result = is_nodejs_project();
        else if (lang == "rust")
            result = is_rust_project();
        else if (lang == "golang")
            result = is_golang_project();
        else if (lang == "java")
            result = is_java_project();
        else if (lang == "cpp")
            result = is_cpp_project();
        else if (lang == "csharp")
            result = is_csharp_project();
        else if (lang == "php")
            result = is_php_project();
        else if (lang == "ruby")
            result = is_ruby_project();
        else if (lang == "kotlin")
            result = is_kotlin_project();
        else if (lang == "swift")
            result = is_swift_project();
        else if (lang == "dart")
            result = is_dart_project();
        else if (lang == "scala")
            result = is_scala_project();

        language_cache[key] = {result, now};
        return result;
    };

    auto get_cached_version = [&](const std::string& lang) -> std::string {
        if (!get_cached_language_detection(lang))
            return "";

        std::lock_guard<std::mutex> lock(cache_mutex);
        auto key = std::filesystem::current_path().string() + "_" + lang + "_version";
        auto it = version_cache.find(key);

        if (it != version_cache.end() && (now - it->second.second) < CACHE_DURATION) {
            return it->second.first;
        }

        std::string result;
        if (lang == "python")
            result = get_python_version();
        else if (lang == "nodejs")
            result = get_nodejs_version();
        else if (lang == "rust")
            result = get_rust_version();
        else if (lang == "golang")
            result = get_golang_version();
        else if (lang == "java")
            result = get_java_version();
        else if (lang == "cpp")
            result = get_cpp_version();
        else if (lang == "csharp")
            result = get_csharp_version();
        else if (lang == "php")
            result = get_php_version();
        else if (lang == "ruby")
            result = get_ruby_version();
        else if (lang == "kotlin")
            result = get_kotlin_version();
        else if (lang == "swift")
            result = get_swift_version();
        else if (lang == "dart")
            result = get_dart_version();
        else if (lang == "scala")
            result = get_scala_version();

        version_cache[key] = {result, now};
        return result;
    };

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

    if (needed_vars.count("PATH")) {
        vars["PATH"] = get_current_file_path();
    }

    if (needed_vars.count("DIRECTORY")) {
        vars["DIRECTORY"] = get_current_file_name();
    }

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
        vars["MEM_USAGE"] = std::to_string(static_cast<int>(get_memory_usage())) + "%";
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

    if (needed_vars.count("STATUS")) {
        char* status_env = getenv("?");
        vars["STATUS"] = status_env ? std::string(status_env) : "0";
    }

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

    if (needed_vars.count("DISPLAY_DIR")) {
        vars["DISPLAY_DIR"] = get_display_directory();
    }

    if (needed_vars.count("TRUNCATED_PATH")) {
        vars["TRUNCATED_PATH"] = get_truncated_path();
    }

    if (needed_vars.count("DIR_TRUNCATED")) {
        vars["DIR_TRUNCATED"] = is_directory_truncated() ? "true" : "false";
    }

    if (needed_vars.count("CMD_DURATION")) {
        if (should_show_duration()) {
            vars["CMD_DURATION"] = get_formatted_duration();
        } else {
            vars["CMD_DURATION"] = "";
        }
    }

    if (needed_vars.count("CMD_DURATION_MS")) {
        vars["CMD_DURATION_MS"] = std::to_string(get_last_command_duration_us());
    }

    if (needed_vars.count("EXIT_CODE")) {
        int exit_code = get_last_exit_code();
        vars["EXIT_CODE"] = exit_code != 0 ? std::to_string(exit_code) : "";
    }

    if (needed_vars.count("EXIT_SYMBOL")) {
        vars["EXIT_SYMBOL"] = get_exit_status_symbol();
    }

    if (needed_vars.count("CMD_SUCCESS")) {
        vars["CMD_SUCCESS"] = is_last_command_success() ? "true" : "false";
    }

    if (needed_vars.count("PYTHON_VERSION")) {
        vars["PYTHON_VERSION"] = get_cached_version("python");
    }

    if (needed_vars.count("NODEJS_VERSION")) {
        vars["NODEJS_VERSION"] = get_cached_version("nodejs");
    }

    if (needed_vars.count("RUST_VERSION")) {
        vars["RUST_VERSION"] = get_cached_version("rust");
    }

    if (needed_vars.count("GOLANG_VERSION")) {
        vars["GOLANG_VERSION"] = get_cached_version("golang");
    }

    if (needed_vars.count("JAVA_VERSION")) {
        vars["JAVA_VERSION"] = get_cached_version("java");
    }

    if (needed_vars.count("CPP_VERSION")) {
        vars["CPP_VERSION"] = get_cached_version("cpp");
    }

    if (needed_vars.count("CSHARP_VERSION")) {
        vars["CSHARP_VERSION"] = get_cached_version("csharp");
    }

    if (needed_vars.count("PHP_VERSION")) {
        vars["PHP_VERSION"] = get_cached_version("php");
    }

    if (needed_vars.count("RUBY_VERSION")) {
        vars["RUBY_VERSION"] = get_cached_version("ruby");
    }

    if (needed_vars.count("KOTLIN_VERSION")) {
        vars["KOTLIN_VERSION"] = get_cached_version("kotlin");
    }

    if (needed_vars.count("SWIFT_VERSION")) {
        vars["SWIFT_VERSION"] = get_cached_version("swift");
    }

    if (needed_vars.count("DART_VERSION")) {
        vars["DART_VERSION"] = get_cached_version("dart");
    }

    if (needed_vars.count("SCALA_VERSION")) {
        vars["SCALA_VERSION"] = get_cached_version("scala");
    }

    if (needed_vars.count("LANGUAGE_VERSIONS")) {
        std::string combined_versions;

        std::vector<std::string> languages = {"python", "nodejs", "rust", "golang", "java",
                                              "cpp",    "csharp", "php",  "ruby",   "kotlin",
                                              "swift",  "dart",   "scala"};

        for (const std::string& lang : languages) {
            if (get_cached_language_detection(lang)) {
                combined_versions += get_cached_version(lang);
            }
        }

        vars["LANGUAGE_VERSIONS"] = combined_versions;
    }

    if (needed_vars.count("PYTHON_VENV")) {
        vars["PYTHON_VENV"] = get_python_virtual_env();
    }

    if (needed_vars.count("NODEJS_PM")) {
        if (get_cached_language_detection("nodejs")) {
            vars["NODEJS_PM"] = get_nodejs_package_manager();
        } else {
            vars["NODEJS_PM"] = "";
        }
    }

    if (needed_vars.count("IS_PYTHON_PROJECT")) {
        vars["IS_PYTHON_PROJECT"] = get_cached_language_detection("python") ? "true" : "false";
    }

    if (needed_vars.count("IS_NODEJS_PROJECT")) {
        vars["IS_NODEJS_PROJECT"] = get_cached_language_detection("nodejs") ? "true" : "false";
    }

    if (needed_vars.count("IS_RUST_PROJECT")) {
        vars["IS_RUST_PROJECT"] = get_cached_language_detection("rust") ? "true" : "false";
    }

    if (needed_vars.count("IS_GOLANG_PROJECT")) {
        vars["IS_GOLANG_PROJECT"] = get_cached_language_detection("golang") ? "true" : "false";
    }

    if (needed_vars.count("IS_JAVA_PROJECT")) {
        vars["IS_JAVA_PROJECT"] = get_cached_language_detection("java") ? "true" : "false";
    }

    if (needed_vars.count("IS_CPP_PROJECT")) {
        vars["IS_CPP_PROJECT"] = get_cached_language_detection("cpp") ? "true" : "false";
    }

    if (needed_vars.count("IS_CSHARP_PROJECT")) {
        vars["IS_CSHARP_PROJECT"] = get_cached_language_detection("csharp") ? "true" : "false";
    }

    if (needed_vars.count("IS_PHP_PROJECT")) {
        vars["IS_PHP_PROJECT"] = get_cached_language_detection("php") ? "true" : "false";
    }

    if (needed_vars.count("IS_RUBY_PROJECT")) {
        vars["IS_RUBY_PROJECT"] = get_cached_language_detection("ruby") ? "true" : "false";
    }

    if (needed_vars.count("IS_KOTLIN_PROJECT")) {
        vars["IS_KOTLIN_PROJECT"] = get_cached_language_detection("kotlin") ? "true" : "false";
    }

    if (needed_vars.count("IS_SWIFT_PROJECT")) {
        vars["IS_SWIFT_PROJECT"] = get_cached_language_detection("swift") ? "true" : "false";
    }

    if (needed_vars.count("IS_DART_PROJECT")) {
        vars["IS_DART_PROJECT"] = get_cached_language_detection("dart") ? "true" : "false";
    }

    if (needed_vars.count("IS_SCALA_PROJECT")) {
        vars["IS_SCALA_PROJECT"] = get_cached_language_detection("scala") ? "true" : "false";
    }

    if (needed_vars.count("CONTAINER_NAME")) {
        vars["CONTAINER_NAME"] = get_container_name();
    }

    if (needed_vars.count("CONTAINER_TYPE")) {
        vars["CONTAINER_TYPE"] = get_container_type();
    }

    if (needed_vars.count("IS_CONTAINER")) {
        vars["IS_CONTAINER"] = is_in_container() ? "true" : "false";
    }

    if (needed_vars.count("DOCKER_CONTEXT")) {
        vars["DOCKER_CONTEXT"] = get_docker_context();
    }

    if (needed_vars.count("DOCKER_IMAGE")) {
        vars["DOCKER_IMAGE"] = get_docker_image();
    }

    if (needed_vars.count("REPO_PATH") && is_git_repo) {
        vars["REPO_PATH"] = get_repo_relative_path(repo_root);
    }

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
            vars["GIT_CHANGES"] = std::to_string(get_git_uncommitted_changes(repo_root));
        }
    }

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
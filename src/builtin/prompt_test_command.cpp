#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "cjsh.h"
#include "prompt.h"
#include "prompt_info.h"

int prompt_test_command(const std::vector<std::string>& args) {
    (void)args;
    Prompt p;
    PromptInfo pi;
    std::filesystem::path repo_root;
    bool is_git_repo = p.is_git_repository(repo_root);

    std::cout << "\n--- Prompt Tag Test ---\n";
    std::cout << "USERNAME: " << pi.get_username() << "\n";
    std::cout << "HOSTNAME: " << pi.get_hostname() << "\n";
    std::cout << "PATH: " << pi.get_current_file_path() << "\n";
    std::cout << "DIRECTORY: " << pi.get_current_file_name() << "\n";
    std::cout << "TIME12: " << pi.get_current_time(true) << "\n";
    std::cout << "TIME24: " << pi.get_current_time(false) << "\n";
    std::cout << "DATE: " << pi.get_current_date() << "\n";
    std::cout << "DAY: " << pi.get_current_day() << "\n";
    std::cout << "MONTH: " << pi.get_current_month() << "\n";
    std::cout << "YEAR: " << pi.get_current_year() << "\n";
    std::cout << "DAY_NAME: " << pi.get_current_day_name() << "\n";
    std::cout << "MONTH_NAME: " << pi.get_current_month_name() << "\n";
    std::cout << "SHELL: " << pi.get_shell() << "\n";
    std::cout << "SHELL_VER: " << pi.get_shell_version() << "\n";
    if (is_git_repo) {
        std::cout << "LOCAL_PATH: " << pi.get_local_path(repo_root) << "\n";
        std::cout << "GIT_BRANCH: "
                  << pi.get_git_branch(repo_root / ".git/HEAD") << "\n";
        std::cout << "GIT_STATUS: " << pi.get_git_status(repo_root) << "\n";
        int ahead = 0, behind = 0;
        pi.get_git_ahead_behind(repo_root, ahead, behind);
        std::cout << "GIT_AHEAD: " << ahead << "\n";
        std::cout << "GIT_BEHIND: " << behind << "\n";
        std::cout << "GIT_STASHES: " << pi.get_git_stash_count(repo_root)
                  << "\n";
        std::cout << "GIT_STAGED: "
                  << (pi.get_git_has_staged_changes(repo_root) ? "✓" : "")
                  << "\n";
        std::cout << "GIT_CHANGES: "
                  << pi.get_git_uncommitted_changes(repo_root) << "\n";
        std::cout << "GIT_REMOTE: " << pi.get_git_remote(repo_root) << "\n";
        std::cout << "GIT_TAG: " << pi.get_git_tag(repo_root) << "\n";
        std::cout << "GIT_LAST_COMMIT: " << pi.get_git_last_commit(repo_root)
                  << "\n";
        std::cout << "GIT_AUTHOR: " << pi.get_git_author(repo_root) << "\n";
    }
    std::cout << "OS_INFO: " << pi.get_os_info() << "\n";
    std::cout << "KERNEL_VER: " << pi.get_kernel_version() << "\n";
    std::cout << "CPU_USAGE: " << pi.get_cpu_usage() << "%\n";
    std::cout << "MEM_USAGE: " << pi.get_memory_usage() << "%\n";
    std::cout << "BATTERY: " << pi.get_battery_status() << "\n";
    std::cout << "UPTIME: " << pi.get_uptime() << "\n";
    std::cout << "TERM_TYPE: " << pi.get_terminal_type() << "\n";
    auto term_dim = pi.get_terminal_dimensions();
    std::cout << "TERM_SIZE: " << term_dim.first << "x" << term_dim.second
              << "\n";
    std::cout << "LANG_VER:python: " << pi.get_active_language_version("python")
              << "\n";
    std::cout << "LANG_VER:node: " << pi.get_active_language_version("node")
              << "\n";
    std::cout << "LANG_VER:ruby: " << pi.get_active_language_version("ruby")
              << "\n";
    std::cout << "LANG_VER:go: " << pi.get_active_language_version("go")
              << "\n";
    std::cout << "LANG_VER:rust: " << pi.get_active_language_version("rust")
              << "\n";

    // Language detection and version tests
    std::cout << "IS_PYTHON_PROJECT: "
              << (pi.is_python_project() ? "yes" : "no") << "\n";
    std::cout << "IS_NODEJS_PROJECT: "
              << (pi.is_nodejs_project() ? "yes" : "no") << "\n";
    std::cout << "IS_RUST_PROJECT: " << (pi.is_rust_project() ? "yes" : "no")
              << "\n";
    std::cout << "IS_GOLANG_PROJECT: "
              << (pi.is_golang_project() ? "yes" : "no") << "\n";
    std::cout << "IS_JAVA_PROJECT: " << (pi.is_java_project() ? "yes" : "no")
              << "\n";

    std::cout << "PYTHON_VERSION: " << pi.get_python_version() << "\n";
    std::cout << "NODEJS_VERSION: " << pi.get_nodejs_version() << "\n";
    std::cout << "RUST_VERSION: " << pi.get_rust_version() << "\n";
    std::cout << "GOLANG_VERSION: " << pi.get_golang_version() << "\n";
    std::cout << "JAVA_VERSION: " << pi.get_java_version() << "\n";

    std::cout << "PYTHON_VIRTUAL_ENV: " << pi.get_python_virtual_env() << "\n";
    std::cout << "NODEJS_PACKAGE_MANAGER: " << pi.get_nodejs_package_manager()
              << "\n";

    // Generic language tests
    std::cout << "LANG_VER_GENERIC:python: "
              << pi.get_language_version("python") << "\n";
    std::cout << "LANG_VER_GENERIC:node: " << pi.get_language_version("node")
              << "\n";
    std::cout << "LANG_VER_GENERIC:rust: " << pi.get_language_version("rust")
              << "\n";
    std::cout << "LANG_VER_GENERIC:go: " << pi.get_language_version("go")
              << "\n";
    std::cout << "LANG_VER_GENERIC:java: " << pi.get_language_version("java")
              << "\n";

    std::cout << "IS_LANG_PROJECT:python: "
              << (pi.is_language_project("python") ? "yes" : "no") << "\n";
    std::cout << "IS_LANG_PROJECT:node: "
              << (pi.is_language_project("node") ? "yes" : "no") << "\n";
    std::cout << "IS_LANG_PROJECT:rust: "
              << (pi.is_language_project("rust") ? "yes" : "no") << "\n";
    std::cout << "IS_LANG_PROJECT:go: "
              << (pi.is_language_project("go") ? "yes" : "no") << "\n";
    std::cout << "IS_LANG_PROJECT:java: "
              << (pi.is_language_project("java") ? "yes" : "no") << "\n";

    std::cout << "DISK_USAGE: "
              << pi.get_disk_usage(std::filesystem::current_path()) << "\n";
    std::cout << "SWAP_USAGE: " << pi.get_swap_usage() << "\n";
    std::cout << "LOAD_AVG: " << pi.get_load_avg() << "\n";
    std::string venv_name;
    std::cout << "VIRTUAL_ENV: "
              << (pi.is_in_virtual_environment(venv_name) ? venv_name : "")
              << "\n";
    std::cout << "BG_JOBS: " << pi.get_background_jobs_count() << "\n";
    std::cout << "STATUS: " << getenv("?") << "\n";

    // Command info tests
    std::cout << "EXIT_STATUS_SYMBOL: " << pi.get_exit_status_symbol() << "\n";
    std::cout << "LAST_COMMAND_SUCCESS: "
              << (pi.is_last_command_success() ? "yes" : "no") << "\n";
    std::cout << "LAST_COMMAND_DURATION_MS: "
              << pi.get_last_command_duration_us() << "\n";
    std::cout << "FORMATTED_DURATION: " << pi.get_formatted_duration() << "\n";
    std::cout << "SHOULD_SHOW_DURATION: "
              << (pi.should_show_duration() ? "yes" : "no") << "\n";

    std::cout << "IP_LOCAL: " << pi.get_ip_address(false) << "\n";
    std::cout << "IP_EXTERNAL: " << pi.get_ip_address(true) << "\n";
    std::cout << "VPN_STATUS: " << (pi.is_vpn_active() ? "on" : "off") << "\n";
    std::cout << "NET_IFACE: " << pi.get_active_network_interface() << "\n";
    // AI tags (if available)
    if (g_ai) {
        std::cout << "AI_MODEL: " << g_ai->get_model() << "\n";
        std::cout << "AI_AGENT_TYPE: " << g_ai->get_assistant_type() << "\n";
        std::cout << "AI_DIVIDER: >\n";
        std::cout << "AI_CONTEXT: " << g_ai->get_save_directory() << "\n";
        std::string ai_context_cmp =
            (std::filesystem::current_path().string() + "/" ==
             g_ai->get_save_directory())
                ? "✔"
                : "✖";
        std::cout << "AI_CONTEXT_COMPARISON: " << ai_context_cmp << "\n";
    }

    // Directory info tests
    std::cout << "DISPLAY_DIRECTORY: " << pi.get_display_directory() << "\n";
    std::cout << "DIRECTORY_NAME: " << pi.get_directory_name() << "\n";
    std::cout << "TRUNCATED_PATH: " << pi.get_truncated_path() << "\n";
    std::cout << "IS_TRUNCATED: "
              << (pi.is_directory_truncated() ? "yes" : "no") << "\n";
    if (is_git_repo) {
        std::cout << "REPO_RELATIVE_PATH: "
                  << pi.get_repo_relative_path(repo_root) << "\n";
    }

    // Container info tests
    std::cout << "CONTAINER_NAME: " << pi.get_container_name() << "\n";
    std::cout << "IN_CONTAINER: " << (pi.is_in_container() ? "yes" : "no")
              << "\n";
    std::cout << "CONTAINER_TYPE: " << pi.get_container_type() << "\n";
    std::cout << "IN_DOCKER: " << (pi.is_in_docker() ? "yes" : "no") << "\n";
    std::cout << "DOCKER_CONTEXT: " << pi.get_docker_context() << "\n";
    std::cout << "DOCKER_IMAGE: " << pi.get_docker_image() << "\n";

    std::cout << "--- End of Prompt Tag Test ---\n";
    return 0;
}

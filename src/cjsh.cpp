#include "cjsh.h"

#include <getopt.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <system_error>
#include <vector>

#ifdef __APPLE__
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#include "ai.h"
#include "builtin.h"
#include "cjsh_filesystem.h"
#include "colors.h"
#include "error_out.h"
#include "isocline.h"
#include "job_control.h"
#include "main_loop.h"
#include "plugin.h"
#include "shell.h"
#include "shell_env.h"
#include "theme.h"
#include "trap_command.h"
#include "usage.h"
#include "utils/command_line_parser.h"
#include "version_command.h"

bool g_exit_flag = false;
std::string g_cached_version;
std::string g_current_theme;
std::string title_line;
std::string created_line;
bool g_startup_active = true;
std::unique_ptr<Shell> g_shell = nullptr;
std::unique_ptr<Ai> g_ai = nullptr;
std::unique_ptr<Theme> g_theme = nullptr;
std::unique_ptr<Plugin> g_plugin = nullptr;
std::vector<std::string> g_startup_args;
std::vector<std::string> g_profile_startup_args;
std::chrono::steady_clock::time_point g_startup_begin_time;

namespace config {
bool login_mode = false;
bool interactive_mode = true;
bool force_interactive = false;
bool execute_command = false;
std::string cmd_to_execute = "";
bool plugins_enabled = true;
bool themes_enabled = true;
bool ai_enabled = true;
bool colors_enabled = true;
bool source_enabled = true;
bool completions_enabled = true;
bool syntax_highlighting_enabled = true;
bool smart_cd_enabled = true;
bool show_version = false;
bool show_help = false;
bool startup_test = false;
bool minimal_mode = false;
bool disable_custom_ls = false;
bool show_startup_time = false;
bool secure_mode = false;
bool show_title_line = true;
}  // namespace config

// switch to sigaction when ever possible and csignal over signal.h
// switch to cerrno when ever possible over errno.h
// switch to cstring when ever possible over string.h
// swtich to climits when ever possible over limits.h
// switch to using instead of typedef
// switch to nullptr over NULL
// cstdint instead of stdint.h
// cstdlib instead of stdlib.h
// ctime instead of time.h

static void save_startup_arguments(int argc, char* argv[]) {
    g_startup_args.clear();
    for (int i = 0; i < argc; i++) {
        g_startup_args.push_back(std::string(argv[i]));
    }
}

static int handle_non_interactive_mode(const std::string& script_file) {
    std::string script_content;

    if (!script_file.empty()) {
        auto read_result = cjsh_filesystem::FileOperations::read_file_content(script_file);
        if (!read_result.is_ok()) {
            ErrorType error_type = ErrorType::FILE_NOT_FOUND;
            if (read_result.error().find("Permission denied") != std::string::npos) {
                error_type = ErrorType::PERMISSION_DENIED;
            }

            print_error({error_type,
                         script_file.c_str(),
                         read_result.error().c_str(),
                         {"Check file path and permissions"}});
            return 127;
        }

        script_content = read_result.value();
    } else {
        std::string line;
        while (std::getline(std::cin, line)) {
            script_content += line + "\n";
        }
    }

    if (!script_content.empty()) {
        int code = g_shell ? g_shell->execute(script_content) : 1;

        const char* exit_code_str = getenv("EXIT_CODE");
        if (exit_code_str) {
            code = std::atoi(exit_code_str);
            unsetenv("EXIT_CODE");
        }

        return code;
    }

    return 0;
}

void initialize_colors() {
    colors::initialize_color_support(config::colors_enabled);

    if (!config::colors_enabled) {
        ic_enable_color(false);
        ic_style_def("ic-prompt", "");
    }
}

void initialize_plugins() {
    if (!config::plugins_enabled) {
        return;
    }
    g_plugin = std::make_unique<Plugin>(cjsh_filesystem::g_cjsh_plugin_path,
                                        config::plugins_enabled, true);
}

void initialize_themes() {
    if (!config::themes_enabled) {
        return;
    }
    g_theme = std::make_unique<Theme>(cjsh_filesystem::g_cjsh_theme_path, config::themes_enabled);
}

void initialize_ai() {
    if (!config::ai_enabled) {
        return;
    }
    std::string api_key = "";
    const char* env_key = getenv("OPENAI_API_KEY");
    if (env_key) {
        api_key = env_key;
    }
    g_ai = std::make_unique<Ai>(api_key, std::string("chat"), std::string(""),
                                std::vector<std::string>{}, cjsh_filesystem::g_cjsh_data_path,
                                config::ai_enabled);
}

static int initialize_interactive_components() {
    g_shell->set_interactive_mode(true);

    if (cjsh_filesystem::init_interactive_filesystem()) {
        g_shell->setup_interactive_handlers();

        initialize_colors();

        std::string saved_current_dir = std::filesystem::current_path().string();

        if (config::source_enabled && !config::secure_mode) {
            if (cjsh_filesystem::file_exists(cjsh_filesystem::g_cjsh_source_path)) {
                g_shell->execute_script_file(cjsh_filesystem::g_cjsh_source_path);
            }
        } else {
            if (std::filesystem::current_path() != saved_current_dir) {
                std::filesystem::current_path(saved_current_dir);
                setenv("PWD", saved_current_dir.c_str(), 1);
                g_shell->get_built_ins()->set_current_directory();
            }
        }
        return 0;
    }

    return 1;
}

static void process_profile_files() {
    if (config::secure_mode) {
        return;
    }
    std::filesystem::path user_profile = cjsh_filesystem::g_user_home_path / ".profile";
    if (std::filesystem::exists(user_profile)) {
        g_shell->execute_script_file(user_profile, true);
    }
    if (std::filesystem::exists(cjsh_filesystem::g_cjsh_profile_path)) {
        g_shell->execute_script_file(cjsh_filesystem::g_cjsh_profile_path, true);
    }
}

static int initialize_login_mode() {
    process_profile_files();
    cjsh::CommandLineParser::apply_profile_startup_flags();
    return 0;
}

static void start_interactive_process() {
    auto startup_end_time = std::chrono::steady_clock::now();
    auto startup_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        startup_end_time - g_startup_begin_time);

    if (g_shell && g_theme) {
        g_shell->set_initial_duration(startup_duration.count());
    }

    if (config::show_title_line) {
        std::cout << " CJ's Shell v" << get_version() << " - Caden J Finley (c) 2025" << std::endl;
        std::cout << " Created 2025 @ \033[1;35mAbilene Christian "
                     "University\033[0m"
                  << std::endl;
    }

    if (cjsh_filesystem::is_first_boot()) {
        // std::cout << " Thank you for installing CJ's Shell!" << std::endl;
        std::cout << std::endl;
        std::cout << " Type 'help' to see available commands and options." << std::endl;
        if (!cjsh_filesystem::file_exists(cjsh_filesystem::g_cjsh_source_path)) {
            std::cout << " To create .cjshrc run 'cjshopt generate-source" << std::endl;
        }
        if (!cjsh_filesystem::file_exists(cjsh_filesystem::g_cjsh_profile_path)) {
            std::cout << " To create .cjprofile run 'cjshopt generate-profile" << std::endl;
        }
        if (!cjsh_filesystem::file_exists(cjsh_filesystem::g_cjsh_logout_path)) {
            std::cout << " To create .cjsh_logout run 'cjshopt generate-logout" << std::endl;
        }
        std::cout << std::endl;
        std::cout << " To suppress this help message run the command: 'touch "
                  << cjsh_filesystem::g_cjsh_first_boot_path.string() << "'" << std::endl;
        std::cout << " To suppress the title line, put this command in .cjprofile: 'cjshopt "
                     "login-startup-arg --no-titleline'"
                  << std::endl;
        std::cout << " Or alternatively execute cjsh with this flag: --no-titleline" << std::endl;
        std::cout << std::endl;
        config::show_startup_time = true;
    }

    if (config::show_title_line && config::show_startup_time) {
        std::cout << std::endl;
    }

    if (config::show_startup_time) {
        std::string startup_ms = g_shell->get_initial_duration();
        std::cout << " Started in " << startup_ms << std::endl;
    }

    if (!config::startup_test) {
        main_process_loop();
    }
}

void process_logout_file() {
    if (!config::secure_mode && (config::interactive_mode || config::force_interactive)) {
        const auto& logout_path = cjsh_filesystem::g_cjsh_logout_path;
        std::error_code logout_status_ec;
        auto logout_status = std::filesystem::status(logout_path, logout_status_ec);

        if (!logout_status_ec && std::filesystem::is_regular_file(logout_status)) {
            g_shell->execute_script_file(logout_path, true);
        }
    }
}

void cleanup_resources() {
    if (g_shell) {
        TrapManager::instance().set_shell(g_shell.get());
        TrapManager::instance().execute_exit_trap();
        process_logout_file();
    }

    if (g_ai) {
        g_ai.reset();
    }

    if (g_theme) {
        g_theme.reset();
    }

    if (g_plugin) {
        g_plugin.reset();
    }

    if (g_shell) {
        g_shell.reset();
    }

    if (config::interactive_mode) {
        std::cerr << "Shutdown complete." << std::endl;
    }
}

int main(int argc, char* argv[]) {
    g_startup_begin_time = std::chrono::steady_clock::now();

    auto parse_result = cjsh::CommandLineParser::parse_arguments(argc, argv);
    if (parse_result.should_exit) {
        return parse_result.exit_code;
    }

    // Handle --version and --help early to avoid unnecessary initialization
    if (config::show_version) {
        std::vector<std::string> empty_args;
        return version_command(empty_args);
    }

    if (config::show_help) {
        print_usage();
        return 0;
    }

    std::string script_file = parse_result.script_file;
    std::vector<std::string> script_args = parse_result.script_args;

    cjsh_filesystem::initialize_cjsh_directories();
    std::atexit(cleanup_resources);

    g_shell = std::make_unique<Shell>();

    if (!script_args.empty()) {
        g_shell->set_positional_parameters(script_args);
    }

    cjsh_env::setup_environment_variables(argv[0]);
    save_startup_arguments(argc, argv);

    if (g_shell) {
        g_shell->sync_env_vars_from_system();
    }

    if (config::login_mode) {
        int login_result = initialize_login_mode();
        if (login_result != 0) {
            return login_result;
        }
    }

    if (config::execute_command) {
        int code = g_shell ? g_shell->execute(config::cmd_to_execute) : 1;

        const char* exit_code_str = getenv("EXIT_CODE");
        if (exit_code_str) {
            code = std::atoi(exit_code_str);
            unsetenv("EXIT_CODE");
        }

        if (g_shell) {
            TrapManager::instance().set_shell(g_shell.get());
            TrapManager::instance().execute_exit_trap();
        }

        return code;
    }

    if (!config::interactive_mode && !config::force_interactive) {
        return handle_non_interactive_mode(script_file);
    }

    int interactive_result = initialize_interactive_components();
    if (interactive_result != 0) {
        return interactive_result;
    }

    g_startup_active = false;
    if (!g_exit_flag && (config::interactive_mode || config::force_interactive)) {
        start_interactive_process();
    }

    std::cerr << "Cleaning up resources." << std::endl;

    const char* exit_code_str = getenv("EXIT_CODE");
    int exit_code = 0;
    if (exit_code_str) {
        exit_code = std::atoi(exit_code_str);
        unsetenv("EXIT_CODE");
    }

    return exit_code;
}

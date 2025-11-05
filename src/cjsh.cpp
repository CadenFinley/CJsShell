#include "cjsh.h"

#include <getopt.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
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

#include "builtin.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "flags.h"
#include "isocline.h"
#include "job_control.h"
#include "main_loop.h"
#include "shell.h"
#include "shell_env.h"
#include "token_constants.h"
#include "trap_command.h"
#include "usage.h"
#include "version_command.h"

const bool PRE_RELEASE = true;
const char* const c_version_base = "3.11.8";

std::string get_version() {
    static std::string cached_version =
        std::string(c_version_base) + (PRE_RELEASE ? " (pre-release)" : "");
    return cached_version;
}

std::vector<std::string>& startup_args() {
    static std::vector<std::string> args;
    return args;
}

std::vector<std::string>& profile_startup_args() {
    static std::vector<std::string> args;
    return args;
}

bool g_exit_flag = false;
bool g_startup_active = true;
std::unique_ptr<Shell> g_shell = nullptr;

namespace config {
bool login_mode = false;
bool interactive_mode = true;
bool force_interactive = false;
bool execute_command = false;
std::string cmd_to_execute;
bool colors_enabled = true;
bool source_enabled = true;
bool completions_enabled = true;
bool syntax_highlighting_enabled = true;
bool smart_cd_enabled = true;
bool show_version = false;
bool show_help = false;
bool startup_test = false;
bool minimal_mode = false;
bool show_startup_time = false;
bool secure_mode = false;
bool show_title_line = true;
bool history_expansion_enabled = true;
bool newline_after_execution = false;
bool uses_cleanup = false;
bool cleanup_newline_after_execution = false;
bool cleanup_adds_empty_line = false;
bool cleanup_truncates_multiline = false;
}  // namespace config

namespace {
std::chrono::steady_clock::time_point g_startup_begin_time;

void save_startup_arguments(int argc, char* argv[]) {
    auto& args = startup_args();
    args.clear();
    for (int i = 0; i < argc; i++) {
        args.push_back(std::string(argv[i]));
    }
}

int handle_non_interactive_mode(const std::string& script_file) {
    std::string script_content;

    if (!script_file.empty()) {
        auto read_result = cjsh_filesystem::read_file_content(script_file);
        if (!read_result.is_ok()) {
            ErrorType error_type = ErrorType::FILE_NOT_FOUND;
            if (read_result.error().find("Permission denied") != std::string::npos) {
                error_type = ErrorType::PERMISSION_DENIED;
            }

            print_error({error_type,
                         script_file,
                         read_result.error(),
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
        if (exit_code_str != nullptr) {
            char* endptr = nullptr;
            long exit_code_long = std::strtol(exit_code_str, &endptr, 10);
            if (endptr != exit_code_str && *endptr == '\0') {
                code = static_cast<int>(exit_code_long);
            }
            unsetenv("EXIT_CODE");
        }

        return code;
    }

    return 0;
}

void initialize_colors() {
    if (!config::colors_enabled) {
        ic_enable_color(false);
    } else if (config::colors_enabled && config::syntax_highlighting_enabled) {
        for (const auto& pair : token_constants::default_styles) {
            std::string style_name = pair.first;
            if (style_name.rfind("ic-", 0) != 0) {
                style_name = "cjsh-" + style_name;
            }
            ic_style_def(style_name.c_str(), pair.second.c_str());
            ic_style_def("ic-prompt", "white");
        }
    }
}

int initialize_interactive_components() {
    g_shell->set_interactive_mode(true);
    if (cjsh_filesystem::init_interactive_filesystem()) {
        g_shell->setup_interactive_handlers();
        initialize_colors();
        cjsh_env::update_terminal_dimensions();

        if (config::source_enabled && !config::secure_mode) {
            if (cjsh_filesystem::file_exists(cjsh_filesystem::g_cjsh_source_path)) {
                g_shell->execute_script_file(cjsh_filesystem::g_cjsh_source_path);
            } else if (cjsh_filesystem::file_exists(cjsh_filesystem::g_cjsh_source_alt_path)) {
                g_shell->execute_script_file(cjsh_filesystem::g_cjsh_source_alt_path);
            }
        }

        initialize_isocline();

        return 0;
    }

    return 1;
}

void process_profile_files() {
    if (config::secure_mode) {
        return;
    }
    std::filesystem::path user_profile = cjsh_filesystem::g_user_home_path / ".profile";
    if (std::filesystem::exists(user_profile)) {
        g_shell->execute_script_file(user_profile, true);
    }

    if (std::filesystem::exists(cjsh_filesystem::g_cjsh_profile_path)) {
        g_shell->execute_script_file(cjsh_filesystem::g_cjsh_profile_path, true);
    } else if (std::filesystem::exists(cjsh_filesystem::g_cjsh_profile_alt_path)) {
        g_shell->execute_script_file(cjsh_filesystem::g_cjsh_profile_alt_path, true);
    }
}

int initialize_login_mode() {
    process_profile_files();
    flags::apply_profile_startup_flags();
    return 0;
}

void start_interactive_process() {
    auto startup_end_time = std::chrono::steady_clock::now();
    auto startup_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        startup_end_time - g_startup_begin_time);

    if (config::show_title_line) {
        std::cout << " CJ's Shell v" << get_version() << " - Caden J Finley (c) 2025" << '\n';
        std::cout << " Created 2025 @ \033[1;35mAbilene Christian "
                     "University\033[0m"
                  << '\n';
    }

    if (cjsh_filesystem::is_first_boot()) {
        std::cout << " Be sure to give us a star on GitHub!" << '\n';
        std::cout << " Type 'help' to see available commands and options." << '\n';
        std::cout << " For additional help and documentation, please visit: "
                  << " https://cadenfinley.github.io/CJsShell/" << '\n';
        std::cout << '\n';
        if (!cjsh_filesystem::file_exists(cjsh_filesystem::g_cjsh_source_path)) {
            std::cout << " To create .cjshrc run 'cjshopt generate-rc'" << '\n';
        }
        if (!cjsh_filesystem::file_exists(cjsh_filesystem::g_cjsh_profile_path)) {
            std::cout << " To create .cjprofile run 'cjshopt generate-profile'" << '\n';
        }
        if (!cjsh_filesystem::file_exists(cjsh_filesystem::g_cjsh_logout_path)) {
            std::cout << " To create .cjsh_logout run 'cjshopt generate-logout'" << '\n';
        }
        std::cout << '\n';
        std::cout << " To suppress this help message run the command: 'touch "
                  << cjsh_filesystem::g_cjsh_first_boot_path.string() << "'" << '\n';
        std::cout << " To suppress the title line, put this command in .cjprofile: 'cjshopt "
                     "login-startup-arg --no-titleline'"
                  << '\n';
        std::cout << " Or alternatively execute cjsh with this flag: --no-titleline" << '\n';
        std::cout << '\n';

        std::cout << " cjsh uses a very complex, but very smart completions system.\n";
        std::cout << " During shell use it learns about the commands you use and provides better "
                     "completions as you use cjsh.\n";
        std::cout << " If you would like to skip the learning process and make all completions "
                     "faster please run: 'generate-completions'\n";
        std::cout
            << " Please note: This may take a few minutes depending on how many commands you have "
               "installed, and it can be sped up using the -j flag.\n";
        std::cout << " For example to use 8 parallel jobs run: 'generate-completions -j 8'\n";
        std::cout << "\n";
        config::show_startup_time = true;
    }

    if (config::show_title_line && config::show_startup_time) {
        std::cout << '\n';
    }

    if (config::show_startup_time) {
        long long microseconds = startup_duration.count();
        std::string startup_time_str;
        if (microseconds < 1000) {
            startup_time_str = std::to_string(microseconds) + "μs";
        } else if (microseconds < 1000000) {
            double milliseconds = static_cast<double>(microseconds) / 1000.0;
            char buffer[32];
            (void)snprintf(buffer, sizeof(buffer), "%.2fms", milliseconds);
            startup_time_str = buffer;
        } else {
            double seconds = static_cast<double>(microseconds) / 1000000.0;
            char buffer[32];
            (void)snprintf(buffer, sizeof(buffer), "%.2fs", seconds);
            startup_time_str = buffer;
        }
        std::cout << " Started in " << startup_time_str << '\n';
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
}  // namespace

void cleanup_resources() {
    if (g_shell) {
        trap_manager_set_shell(g_shell.get());
        trap_manager_execute_exit_trap();
        if (config::login_mode) {
            process_logout_file();
        }
        g_shell.reset();
    }

    if (config::interactive_mode || config::force_interactive) {
        std::cout << "Shutdown Complete." << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // main entry
    // set start time
    g_startup_begin_time = std::chrono::steady_clock::now();

    // parse passed flags
    auto parse_result = flags::parse_arguments(argc, argv);
    if (parse_result.should_exit) {
        return parse_result.exit_code;
    }

    // handle simple flags
    if (config::show_version) {
        std::vector<std::string> empty_args;
        return version_command(empty_args);
    }
    if (config::show_help) {
        print_usage();
        return 0;
    }

    // determine if the passed arg is a script file and grab following args to be used for the
    // script
    std::string script_file = parse_result.script_file;
    std::vector<std::string> script_args = parse_result.script_args;

    // verify essential files for cjsh
    cjsh_filesystem::initialize_cjsh_directories();
    if (std::atexit(cleanup_resources) != 0) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "",
                     "Failed to set exit handler",
                     {"Resource cleanup may not occur properly"}});
        return 1;
    }

    // create the shell object
    g_shell = std::make_unique<Shell>();
    if (!g_shell) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "",
                     "Failed to initialize shell",
                     {"Insufficient memory or system resources"}});
        return 1;
    }

    // set args for the script file before saving the startup args for cjsh
    if (!script_args.empty()) {
        g_shell->set_positional_parameters(script_args);
    }

    // set all envvars for cjsh
    cjsh_env::setup_environment_variables(argv[0]);
    save_startup_arguments(argc, argv);
    g_shell->sync_env_vars_from_system();

    // start login mode items
    if (config::login_mode) {
        int login_result = initialize_login_mode();
        if (login_result != 0) {
            return login_result;
        }
    }

    // execute command passed with -c
    if (config::execute_command) {
        int code = g_shell ? g_shell->execute(config::cmd_to_execute) : 1;

        const char* exit_code_str = getenv("EXIT_CODE");
        if (exit_code_str != nullptr) {
            char* endptr = nullptr;
            long exit_code_long = std::strtol(exit_code_str, &endptr, 10);
            if (endptr != exit_code_str && *endptr == '\0') {
                code = static_cast<int>(exit_code_long);
            }
            unsetenv("EXIT_CODE");
        }

        if (g_shell) {
            trap_manager_set_shell(g_shell.get());
            trap_manager_execute_exit_trap();
            g_shell.reset();
        }

        return code;
    }

    // at this point everything else with the startup args has been handled so now we handle the
    // passed script
    if (!config::interactive_mode && !config::force_interactive) {
        return handle_non_interactive_mode(script_file);
    }

    // at this point cjsh has to be in an interactive state as all non-interactive possibilites has
    // been properly handled
    int interactive_result = initialize_interactive_components();
    if (interactive_result != 0) {
        return interactive_result;
    }

    // disable the global startup flag which disables a lot of interactive features and command
    // output
    g_startup_active = false;

    // start interactive cjsh process
    if (!g_exit_flag && (config::interactive_mode || config::force_interactive)) {
        start_interactive_process();
    }

    // grab exit code from envvar which was set by the last command that executed
    const char* exit_code_str = getenv("EXIT_CODE");
    int exit_code = 0;
    if (exit_code_str != nullptr) {
        char* endptr = nullptr;
        long exit_code_long = std::strtol(exit_code_str, &endptr, 10);
        if (endptr != exit_code_str && *endptr == '\0') {
            exit_code = static_cast<int>(exit_code_long);
        }
        unsetenv("EXIT_CODE");
    }

    return exit_code;
}

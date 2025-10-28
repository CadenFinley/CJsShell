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
#include "colors.h"
#include "error_out.h"
#include "flags.h"
#include "isocline.h"
#include "job_control.h"
#include "main_loop.h"
#include "shell.h"
#include "shell_env.h"
#include "theme.h"
#include "token_constants.h"
#include "trap_command.h"
#include "usage.h"
#include "version_command.h"

bool g_exit_flag = false;
bool g_startup_active = true;
std::unique_ptr<Shell> g_shell = nullptr;

namespace {

struct PosixFeatureSnapshot {
    bool valid = false;
    bool themes_enabled = true;
    bool colors_enabled = true;
    bool completions_enabled = true;
    bool syntax_highlighting_enabled = true;
    bool smart_cd_enabled = true;
    bool show_title_line = true;
    bool show_startup_time = false;
    bool history_expansion_enabled = true;
};

PosixFeatureSnapshot g_posix_snapshot;

std::chrono::steady_clock::time_point g_startup_begin_time;

void save_startup_arguments(int argc, char* argv[]) {
    auto& args = startup_args();
    args.clear();
    for (int i = 0; i < argc; i++) {
        args.push_back(std::string(argv[i]));
    }
}

bool invoked_as_sh(const char* argv0) {
    if (argv0 == nullptr) {
        return false;
    }

    std::filesystem::path executable_path(argv0);
    std::string base_name = executable_path.filename().string();

    if (base_name.empty()) {
        return false;
    }

    size_t non_dash_pos = base_name.find_first_not_of('-');
    if (non_dash_pos == std::string::npos) {
        return false;
    }

    if (non_dash_pos > 0) {
        base_name = base_name.substr(non_dash_pos);
    }

    return base_name == "sh";
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
    colors::initialize_color_support(config::colors_enabled);

    if (!config::colors_enabled) {
        ic_enable_color(false);
        ic_style_def("ic-prompt", "");
        ic_style_def("ic-linenumbers", "");
        ic_style_def("ic-linenumber-current", "");
    } else if (config::colors_enabled && config::syntax_highlighting_enabled) {
        for (const auto& pair : token_constants::default_styles) {
            std::string style_name = pair.first;
            if (style_name.rfind("ic-", 0) != 0) {
                style_name = "cjsh-" + style_name;
            }
            ic_style_def(style_name.c_str(), pair.second.c_str());
        }
    }
}

int initialize_interactive_components() {
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

    Theme* theme = g_shell ? g_shell->get_theme() : nullptr;

    if (g_shell && theme) {
        g_shell->set_initial_duration(startup_duration.count());
    }

    if (config::show_title_line) {
        std::cout << " CJ's Shell v" << get_version() << " - Caden J Finley (c) 2025" << '\n';
        std::cout << " Created 2025 @ \033[1;35mAbilene Christian "
                     "University\033[0m"
                  << '\n';
    }

    if (cjsh_filesystem::is_first_boot()) {
        std::cout << '\n';
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
        config::show_startup_time = true;
    }

    if (config::show_title_line && config::show_startup_time) {
        std::cout << '\n';
    }

    if (config::show_startup_time) {
        std::string startup_time_str;
        if (g_shell && theme) {
            startup_time_str = g_shell->get_initial_duration();
        } else {
            long microseconds = startup_duration.count();
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

namespace config {
bool login_mode = false;
bool interactive_mode = true;
bool force_interactive = false;
bool execute_command = false;
std::string cmd_to_execute;
bool themes_enabled = true;
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
bool no_prompt = false;
bool history_expansion_enabled = true;
bool posix_mode = false;

void set_posix_mode(bool enable) {
    if (posix_mode == enable) {
        return;
    }

    if (enable) {
        g_posix_snapshot.valid = true;
        g_posix_snapshot.themes_enabled = themes_enabled;
        g_posix_snapshot.colors_enabled = colors_enabled;
        g_posix_snapshot.completions_enabled = completions_enabled;
        g_posix_snapshot.syntax_highlighting_enabled = syntax_highlighting_enabled;
        g_posix_snapshot.smart_cd_enabled = smart_cd_enabled;
        g_posix_snapshot.show_title_line = show_title_line;
        g_posix_snapshot.show_startup_time = show_startup_time;
        g_posix_snapshot.history_expansion_enabled = history_expansion_enabled;

        posix_mode = true;
        themes_enabled = false;
        colors_enabled = false;
        completions_enabled = false;
        syntax_highlighting_enabled = false;
        smart_cd_enabled = false;
        show_title_line = false;
        show_startup_time = false;
        history_expansion_enabled = false;

        if (g_shell) {
            g_shell->reset_theme();
        }
    } else {
        posix_mode = false;

        if (g_posix_snapshot.valid) {
            themes_enabled = g_posix_snapshot.themes_enabled;
            colors_enabled = g_posix_snapshot.colors_enabled;
            completions_enabled = g_posix_snapshot.completions_enabled;
            syntax_highlighting_enabled = g_posix_snapshot.syntax_highlighting_enabled;
            smart_cd_enabled = g_posix_snapshot.smart_cd_enabled;
            show_title_line = g_posix_snapshot.show_title_line;
            show_startup_time = g_posix_snapshot.show_startup_time;
            history_expansion_enabled = g_posix_snapshot.history_expansion_enabled;
            g_posix_snapshot.valid = false;
        }
    }

    if (g_shell) {
        g_shell->set_shell_option("posix", posix_mode);
    }
}

bool is_posix_mode() {
    return posix_mode;
}

}  // namespace config

void cleanup_resources() {
    if (g_shell) {
        trap_manager_set_shell(g_shell.get());
        trap_manager_execute_exit_trap();
        if (config::login_mode) {
            process_logout_file();
        }
    }

    if (g_shell) {
        g_shell.reset();
    }

    if (config::interactive_mode || config::force_interactive) {
        std::cout << "Shutdown Complete." << std::endl;
    }
}

int main(int argc, char* argv[]) {
    g_startup_begin_time = std::chrono::steady_clock::now();

    auto parse_result = flags::parse_arguments(argc, argv);
    if (parse_result.should_exit) {
        return parse_result.exit_code;
    }

    if (invoked_as_sh(argv[0])) {
        config::set_posix_mode(true);
    }

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
    if (std::atexit(cleanup_resources) != 0) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "",
                     "Failed to set exit handler",
                     {"Resource cleanup may not occur properly"}});
        return 1;
    }

    g_shell = std::make_unique<Shell>();
    if (!g_shell) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "",
                     "Failed to initialize shell",
                     {"Insufficient memory or system resources"}});
        return 1;
    }

    g_shell->set_shell_option("posix", config::posix_mode);

    if (!script_args.empty()) {
        g_shell->set_positional_parameters(script_args);
    }

    cjsh_env::setup_environment_variables(argv[0]);
    save_startup_arguments(argc, argv);

    g_shell->sync_env_vars_from_system();

    if (config::login_mode) {
        int login_result = initialize_login_mode();
        if (login_result != 0) {
            return login_result;
        }
    }

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

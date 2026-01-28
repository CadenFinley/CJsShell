#include "cjsh.h"

#include <getopt.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>
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
#include "job_control.h"
#include "main_loop.h"
#include "prompt.h"
#include "shell.h"
#include "shell_env.h"
#include "trap_command.h"
#include "usage.h"
#include "version_command.h"

bool g_exit_flag = false;
bool g_startup_active = true;
std::uint64_t g_command_sequence = 0;
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
bool completion_learning_enabled = true;
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
bool suppress_sh_warning = false;
}  // namespace config

namespace {
bool invoked_via_sh(const char* arg0) {
    if (arg0 == nullptr) {
        return false;
    }

    std::string_view shell_name(arg0);
    const std::size_t slash_pos = shell_name.find_last_of('/');
    if (slash_pos != std::string_view::npos) {
        shell_name.remove_prefix(slash_pos + 1);
    }
    if (!shell_name.empty() && shell_name.front() == '-') {
        shell_name.remove_prefix(1);
    }

    return shell_name == "sh";
}

bool cleanup_already_invoked = false;
void cleanup_resources() {
    // primary exit function that gets called on all exit paths
    if (cleanup_already_invoked) {
        return;
    }
    cleanup_already_invoked = true;

    if (g_shell) {
        trap_manager_set_shell(g_shell.get());
        trap_manager_execute_exit_trap();
        if (config::login_mode) {
            cjsh_filesystem::process_logout_file();
        }
        g_shell.reset();
    }

    if (config::interactive_mode || config::force_interactive) {
        std::cout << "Shutdown Complete." << '\n';
    }
}
}  // namespace

int run_cjsh(int argc, char* argv[]) {
    const bool launched_as_sh = invoked_via_sh((argc > 0) ? argv[0] : nullptr);
    // set start time
    startup_begin_time() = std::chrono::steady_clock::now();

    // parse passed flags
    auto parse_result = flags::parse_arguments(argc, argv);
    if (parse_result.should_exit) {
        return parse_result.exit_code;
    }

    // handle simple flags for version and help
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

    // make sure JobManager is constructed before registering cleanup so its destructor
    // runs after cleanup_resources and outlives the cleanup handler
    (void)JobManager::instance();
    trap_manager_initialize();

    // register cleanup handler
    if (std::atexit(cleanup_resources) != 0) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "",
                     "Failed to set exit handler",
                     {"Resource cleanup may not occur properly"}});
        // this is not a fatal error so we continue running cjsh as operating system should clean up
        // resources on exit
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
    flags::save_startup_arguments(argc, argv);
    g_shell->sync_env_vars_from_system();

    // start login mode items
    if (config::login_mode) {
        cjsh_filesystem::process_profile_files();
        flags::apply_profile_startup_flags();
    }

    // execute command passed with -c
    if (config::execute_command) {
        const int code = g_shell ? g_shell->execute(config::cmd_to_execute) : 1;
        return read_exit_code_or(code);
    }

    // at this point everything else with the startup args has been handled so now we handle the
    // passed script
    if (!config::interactive_mode && !config::force_interactive) {
        return handle_non_interactive_mode(script_file);
    }

    // handle the case where stdin is not a terminal
    const bool stdin_is_piped = (isatty(STDIN_FILENO) == 0);
    if (config::force_interactive && stdin_is_piped && !config::execute_command) {
        return handle_non_interactive_mode(script_file);
    }

    // at this point cjsh has to be in an interactive state as all non-interactive possibilites and
    // early exits have been properly handled
    if (launched_as_sh && (config::interactive_mode || config::force_interactive) &&
        !config::suppress_sh_warning) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     ErrorSeverity::WARNING,
                     "sh",
                     "cjsh was invoked as sh, but it is not 100% POSIX compliant",
                     {"Pass --no-sh-warning to hide this warning"}});
    }
    g_shell->set_interactive_mode(true);
    if (!cjsh_filesystem::initialize_cjsh_directories()) {
        return 1;
    }

    // init interactive signals
    g_shell->setup_interactive_handlers();

    // init interactive ui
    prompt::initialize_colors();
    cjsh_env::update_terminal_dimensions();

    // use .cjshrc
    cjsh_filesystem::process_source_files();

    // start interactive cjsh process
    if (!g_exit_flag && (config::interactive_mode || config::force_interactive)) {
        start_interactive_process();
    }

    // grab exit code from envvar which was set by the last command that executed and exit cjsh
    return read_exit_code_or(0);
}

int main(int argc, char* argv[]) {
    // main entry
    // we split off the main cjsh runner to allow atexit() to properly scope cleanup if cjsh has to
    // exit through a non normal path
    int exit_code = run_cjsh(argc, argv);
    cleanup_resources();
    return exit_code;
}

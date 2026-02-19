/*
  cjsh.cpp

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "cjsh.h"

#include <getopt.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
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

std::unique_ptr<Shell> g_shell = nullptr;

namespace {
bool invoked_via_sh(const char* arg0) {
    // check is cjsh is symlinked to sh and return true or false
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

    // reset everything
    if (!g_shell) {
        // if the shell was never created then nothing else was so just leave
        return;
    }

    // if the shell is being force exited then we skip the traps and just reset the shell to clean
    // up resources as best as possible
    if (cjsh_env::force_exit_requested()) {
        g_shell.reset();
        return;
    }

    // otherwise we do a full shutdown with traps and everything
    trap_manager_set_shell(g_shell.get());
    trap_manager_execute_exit_trap();
    if (config::login_mode) {
        cjsh_filesystem::process_logout_file();
    }

    // this might be the most important part of cjsh shutdown. this is so important as
    // std::unique_ptr doesnt always get reset in the same order when there are multiple so
    // resetting this earlier allows specific ordering of reset and release
    g_shell.reset();
}

int run_cjsh(int argc, char* argv[]) {
    cjsh_env::reset_shell_state();
    // set start time
    startup_begin_time() = std::chrono::steady_clock::now();

    // parse passed flags
    auto parse_result = flags::parse_arguments(argc, argv);
    if (parse_result.should_exit) {
        return parse_result.exit_code;
    }

    // auto-enable posix mode if invoked as sh, equivalent to --posix
    const bool launched_as_sh = invoked_via_sh((argc > 0) ? argv[0] : nullptr);
    if (launched_as_sh) {
        flags::apply_posix_mode_settings();
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

    // register cleanup handler
    if (std::atexit(cleanup_resources) != 0) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "",
                     "failed to set exit handler",
                     {"resource cleanup may not occur properly"}});
        // this is not a fatal error so we continue running cjsh as operating system should clean up
        // resources on exit there just might be some shell errors on exit because of the use of
        // std::unique_ptr
    }

    // create the shell object
    g_shell = std::make_unique<Shell>();
    if (!g_shell) {
        print_error({ErrorType::FATAL_ERROR, "", "failed to properly initialize shell", {}});
        return 1;
    }

    // explicitly apply no exec here in case it was in flags so that it applies to shell right after
    // initialization
    g_shell->apply_no_exec(config::no_exec);

    // set args for the script file before saving the startup args for cjsh
    if (!script_args.empty()) {
        flags::set_positional_parameters(script_args);
    }

    // set all envvars for cjsh
    cjsh_env::setup_environment_variables(argv[0]);
    flags::save_startup_arguments(argc, argv);
    cjsh_env::sync_env_vars_from_system(*g_shell);

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
        // is cjsh is symlinked to sh then we throw a warning saying cjsh is not 100% posix
        // compliant interactively
        print_error({ErrorType::INVALID_ARGUMENT,
                     ErrorSeverity::WARNING,
                     "sh",
                     "cjsh was invoked as sh, but it is not 100% POSIX compliant",
                     {"Pass --no-sh-warning to hide this warning"}});
    }

    // then officially turn the switch to interactive mode and read needed interactive files
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
    if (!cjsh_env::exit_requested() && (config::interactive_mode || config::force_interactive)) {
        start_interactive_process();
    }

    // grab exit code from envvar which was set by the last command that executed and exit cjsh
    return read_exit_code_or(0);
}
}  // namespace

int main(int argc, char* argv[]) {
    // main entry
    // we split off the main cjsh runner to allow atexit() to properly scope cleanup if cjsh has to
    // exit through a non normal path
    int exit_code = run_cjsh(argc, argv);
    cleanup_resources();
    return exit_code;
}

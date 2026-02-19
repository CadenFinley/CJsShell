/*
  exit_command.cpp

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

#include "exit_command.h"

#include "builtin_help.h"
#include "error_out.h"
#include "job_control.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "cjsh.h"
#include "exec.h"
#include "flags.h"
#include "shell.h"
#include "shell_env.h"

namespace {
enum class JobWarningState : std::uint8_t {
    NONE,
    RUNNING_OR_STOPPED
};

JobWarningState g_last_job_warning = JobWarningState::NONE;
std::uint64_t g_last_exit_warning_command = 0;
}  // namespace

int exit_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args, {"Usage: exit [-f|--force] [N]",
                                   "Exit the shell with status N (default last command).",
                                   "Use --force to skip running exit traps."})) {
        return 0;
    }
    int exit_code = 0;
    bool force_exit = false;

    int non_flag_args = 0;

    force_exit = std::find(args.begin(), args.end(), "-f") != args.end() ||
                 std::find(args.begin(), args.end(), "--force") != args.end();

    for (size_t i = 1; i < args.size(); i++) {
        const std::string& val = args[i];
        if (val != "-f" && val != "--force") {
            non_flag_args++;
            char* endptr = nullptr;
            long code = std::strtol(val.c_str(), &endptr, 10);
            if (endptr && *endptr == '\0') {
                exit_code = static_cast<int>(code) & 0xFF;

                break;
            } else {
                print_error({ErrorType::INVALID_ARGUMENT,
                             "exit",
                             "invalid numeric argument: " + val,
                             {"Use a number between 0 and 255."}});
                cjsh_env::request_exit();
                cjsh_env::set_shell_variable_value("EXIT_CODE", "128");
                return 0;
            }
        }
    }

    if (non_flag_args > 1) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "exit",
                     "too many arguments",
                     {"Use at most one exit status argument."}});
        cjsh_env::request_exit();
        cjsh_env::set_shell_variable_value("EXIT_CODE", "128");
        return 0;
    }

    const auto& initial_args = flags::startup_args();
    const bool invoked_with_dash_c =
        std::find(initial_args.begin(), initial_args.end(), "-c") != initial_args.end();
    const bool running_dash_c =
        config::execute_command || !config::cmd_to_execute.empty() || invoked_with_dash_c;
    const bool should_check_jobs = !force_exit && !running_dash_c;
    bool forced_by_repeated_exit = false;
    const std::uint64_t current_command_sequence = cjsh_env::command_sequence();

    if (should_check_jobs) {
        auto& job_manager = JobManager::instance();
        job_manager.update_job_statuses();

        const auto jobs = job_manager.get_all_jobs();
        bool has_stopped_jobs = false;
        bool has_running_jobs = false;

        for (const auto& job : jobs) {
            if (!job) {
                continue;
            }
            const JobState state = job->state.load(std::memory_order_relaxed);
            if (state == JobState::STOPPED) {
                has_stopped_jobs = true;
            } else if (state == JobState::RUNNING) {
                has_running_jobs = true;
            }
        }

        const bool has_blocking_jobs = has_stopped_jobs || has_running_jobs;
        if (!has_blocking_jobs) {
            g_last_job_warning = JobWarningState::NONE;
            g_last_exit_warning_command = 0;
        } else {
            const bool had_previous_warning =
                g_last_job_warning == JobWarningState::RUNNING_OR_STOPPED;
            const bool consecutive_exit_attempt =
                had_previous_warning && g_last_exit_warning_command != 0 &&
                current_command_sequence == g_last_exit_warning_command + 1;

            if (consecutive_exit_attempt) {
                force_exit = true;
                forced_by_repeated_exit = true;
            } else {
                std::string warning;
                if (has_stopped_jobs && has_running_jobs) {
                    warning = "There are stopped and running jobs.";
                } else if (has_stopped_jobs) {
                    warning = "There are stopped jobs.";
                } else {
                    warning = "There are running jobs.";
                }

                g_last_job_warning = JobWarningState::RUNNING_OR_STOPPED;
                g_last_exit_warning_command = current_command_sequence;

                print_error({ErrorType::RUNTIME_ERROR,
                             ErrorSeverity::WARNING,
                             "exit",
                             warning,
                             {"Use `jobs` to inspect them.",
                              "Resume, disown, or run `exit --force` to exit."}});
                return 1;
            }
        }
    }

    if (force_exit) {
        cjsh_env::request_force_exit();
        if (forced_by_repeated_exit) {
            print_error({ErrorType::RUNTIME_ERROR,
                         ErrorSeverity::WARNING,
                         "exit",
                         "Second exit attempt detected. Forcing exit despite active jobs.",
                         {"Use `exit --force` to skip the warning immediately."}});
        }
        if (g_shell && g_shell->shell_exec) {
            g_shell->shell_exec->terminate_all_child_process();
        }
        JobManager::instance().clear_all_jobs();
        std::exit(exit_code);
    }

    cjsh_env::request_exit();
    cjsh_env::set_shell_variable_value("EXIT_CODE", std::to_string(exit_code));
    return 0;
}

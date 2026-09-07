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
#include <string>
#include <utility>
#include <vector>

#include "flags.h"
#include "numeric_utils.h"
#include "shell_env.h"
#include "signal_handler.h"

namespace {
enum class ExitWarningState : std::uint8_t {
    NONE,
    CONFIRMATION_REQUIRED
};

ExitWarningState g_last_exit_warning = ExitWarningState::NONE;
std::uint64_t g_last_exit_warning_command = 0;
int g_pending_exit_status = 0;

int get_last_command_status() {
    const std::string last_status = cjsh_env::get_shell_variable_value("?");
    if (last_status.empty()) {
        return 0;
    }

    return numeric_utils::parse_exit_status_or(last_status, 0, false);
}
}  // namespace

int exit_command(const std::vector<std::string>& args) {
    const std::string command_name = args.empty() ? "exit" : args[0];
    if (builtin_handle_help(args,
                            {"Usage: " + command_name + " [-f|--force] [N]",
                             "Exit the shell with status N (default last command).",
                             "Use --force to bypass confirmation and run normal exit cleanup."})) {
        return 0;
    }
    int exit_code = get_last_command_status();
    bool force_exit = false;
    std::vector<std::string> operands;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-f" || args[i] == "--force") {
            force_exit = true;
        } else {
            operands.push_back(args[i]);
        }
    }
    if (operands.size() > 1) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     command_name,
                     "too many arguments",
                     {"Use at most one exit status argument."}});
        g_last_exit_warning = ExitWarningState::NONE;
        return 1;
    }
    if (!operands.empty()) {
        const auto& value = operands.front();
        const size_t digits = !value.empty() && (value[0] == '+' || value[0] == '-') ? 1 : 0;
        long status = 0;
        if (digits == value.size() ||
            value.find_first_not_of("0123456789", digits) != std::string::npos ||
            !numeric_utils::parse_long_strict(value, status)) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         command_name,
                         "invalid numeric argument: " + value,
                         {"Use a signed integer exit status."}});
            cjsh_env::request_exit();
            (void)cjsh_env::set_shell_variable_value("EXIT_CODE", "2");
            return 2;
        }
        exit_code = static_cast<int>(static_cast<unsigned long>(status) & 0xffUL);
    }

    const auto& initial_args = flags::startup_args();
    const bool invoked_with_dash_c =
        std::find(initial_args.begin(), initial_args.end(), "-c") != initial_args.end();
    const bool running_dash_c =
        config::execute_command || !config::cmd_to_execute.empty() || invoked_with_dash_c;
    const bool should_check_confirmation =
        !force_exit && !running_dash_c && !cjsh_env::startup_active() &&
        !SignalHandler::shutting_down() && !SignalHandler::executing_trap() &&
        config::exit_confirmation_mode != config::ExitConfirmationMode::Never;
    const std::uint64_t current_command_sequence = cjsh_env::command_sequence();

    const bool consecutive_exit_attempt =
        g_last_exit_warning == ExitWarningState::CONFIRMATION_REQUIRED &&
        current_command_sequence == g_last_exit_warning_command + 1;
    if (consecutive_exit_attempt && operands.empty()) {
        exit_code = g_pending_exit_status;
    }

    if (should_check_confirmation) {
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
        const bool confirmation_required =
            has_blocking_jobs ||
            config::exit_confirmation_mode == config::ExitConfirmationMode::Always;
        if (!confirmation_required) {
            g_last_exit_warning = ExitWarningState::NONE;
            g_last_exit_warning_command = 0;
        } else {
            if (consecutive_exit_attempt) {
                g_last_exit_warning = ExitWarningState::NONE;
                g_last_exit_warning_command = 0;
            } else {
                std::string warning;
                if (has_stopped_jobs && has_running_jobs) {
                    warning = "There are stopped and running jobs.";
                } else if (has_stopped_jobs) {
                    warning = "There are stopped jobs.";
                } else if (has_running_jobs) {
                    warning = "There are running jobs.";
                } else {
                    warning = "Exit confirmation is required.";
                }

                g_last_exit_warning = ExitWarningState::CONFIRMATION_REQUIRED;
                g_last_exit_warning_command = current_command_sequence;
                g_pending_exit_status = exit_code;

                std::vector<std::string> suggestions;
                if (has_blocking_jobs) {
                    suggestions.push_back("Use `jobs` to inspect them.");
                    suggestions.push_back(
                        "Resume, disown, repeat `exit`, or run `exit --force` to exit.");
                } else {
                    suggestions.push_back("Repeat `exit` to confirm.");
                    suggestions.push_back("Run `exit --force` to exit immediately.");
                }
                print_error({ErrorType::RUNTIME_ERROR, ErrorSeverity::WARNING, "exit", warning,
                             std::move(suggestions)});
                return 1;
            }
        }
    } else {
        g_last_exit_warning = ExitWarningState::NONE;
        g_last_exit_warning_command = 0;
    }

    cjsh_env::request_exit();

    // set the exit code that the shell will actually exit with
    (void)cjsh_env::set_shell_variable_value("EXIT_CODE", std::to_string(exit_code));

    // the exit command itself will return 0 on a successful exit request and exit code set. this is
    // not the exit code that the shell returns
    return 0;
}

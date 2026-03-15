/*
  job_control_commands.cpp

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

#include "job_control_commands.h"

#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#include "builtin_help.h"
#include "cjsh.h"
#include "error_out.h"
#include "exec.h"
#include "job_control.h"
#include "shell.h"

int bg_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args,
                            {"Usage: bg [%JOB]", "Resume a stopped job in the background."})) {
        return 0;
    }

    auto& job_manager = JobManager::instance();
    job_manager.update_job_statuses();

    auto resolved_job = job_control_helpers::resolve_control_job_target(args, job_manager);
    if (!resolved_job) {
        return 1;
    }

    auto job = resolved_job->job;
    int job_id = resolved_job->job_id;

    const JobState current_state = job->state.load(std::memory_order_relaxed);
    if (current_state != JobState::STOPPED) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     std::to_string(job_id),
                     "not stopped",
                     {"Use 'jobs' to list job states"}});
        return 1;
    }

    if (g_shell && g_shell->shell_exec) {
        g_shell->shell_exec->set_job_output_forwarding(job->pgid, false);
    }

    if (killpg(job->pgid, SIGCONT) < 0) {
        print_error_errno({ErrorType::RUNTIME_ERROR, "bg", "killpg", {}});
        return 1;
    }

    job->state.store(JobState::RUNNING, std::memory_order_relaxed);
    job->stop_notified.store(false, std::memory_order_relaxed);
    std::cout << "[" << job_id << "]+ " << job->display_command() << " &" << '\n';

    return 0;
}

int fg_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args, {"Usage: fg [%JOB]", "Bring a job to the foreground."})) {
        return 0;
    }

    auto& job_manager = JobManager::instance();
    job_manager.update_job_statuses();

    auto resolved_job = job_control_helpers::resolve_control_job_target(args, job_manager);
    if (!resolved_job) {
        return 1;
    }

    auto job = resolved_job->job;
    int job_id = resolved_job->job_id;

    if (isatty(STDIN_FILENO) != 0) {
        if (tcsetpgrp(STDIN_FILENO, job->pgid) < 0) {
            print_error_errno({ErrorType::RUNTIME_ERROR, "fg", "tcsetpgrp", {}});
            return 1;
        }
    }

    const JobState current_state = job->state.load(std::memory_order_relaxed);
    if (current_state == JobState::DONE || current_state == JobState::TERMINATED) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     std::to_string(job_id),
                     "job has already completed",
                     {"Use 'jobs' to list available jobs"}});
        return 1;
    }

    if (g_shell && g_shell->shell_exec) {
        g_shell->shell_exec->set_job_output_forwarding(job->pgid, true);
    }

    if (killpg(job->pgid, SIGCONT) < 0) {
        print_error_errno({ErrorType::RUNTIME_ERROR, "fg", "killpg", {}});
        return 1;
    }

    job->state.store(JobState::RUNNING, std::memory_order_relaxed);
    job->stop_notified.store(false, std::memory_order_relaxed);
    job_manager.set_current_job(job_id);

    std::cout << job->display_command() << '\n';

    int status = 0;
    for (pid_t pid : job->pids) {
        waitpid(pid, &status, WUNTRACED);
    }

    if (isatty(STDIN_FILENO) != 0) {
        tcsetpgrp(STDIN_FILENO, getpgrp());
    }

    if (WIFEXITED(status)) {
        job_manager.remove_job(job_id);
        return WEXITSTATUS(status);
    }
    if (WIFSTOPPED(status)) {
        job->state.store(JobState::STOPPED, std::memory_order_relaxed);
        job_manager.notify_job_stopped(job);
        return 128 + WSTOPSIG(status);
    }
    if (WIFSIGNALED(status)) {
        job_manager.remove_job(job_id);
        return 128 + WTERMSIG(status);
    }

    return 0;
}

int jobs_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(
            args, {"Usage: jobs [-lp]", "List active jobs. -l shows PIDs, -p prints PIDs only."})) {
        return 0;
    }

    auto& job_manager = JobManager::instance();
    job_manager.update_job_statuses();

    bool long_format = false;
    bool pid_only = false;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-l") {
            long_format = true;
        } else if (args[i] == "-p") {
            pid_only = true;
        } else if (args[i].substr(0, 1) == "-") {
            print_error({ErrorType::INVALID_ARGUMENT,
                         args[i],
                         "Invalid option",
                         {"Use -l for long format, -p for PIDs only"}});
            return 1;
        } else {
            print_error({ErrorType::INVALID_ARGUMENT,
                         args[i],
                         "jobs does not take positional arguments",
                         {"Usage: jobs [-lp]"}});
            return 1;
        }
    }

    auto jobs = job_manager.get_all_jobs();
    if (jobs.empty()) {
        if (!pid_only) {
            std::cout << "No jobs" << '\n';
        }
        return 0;
    }

    int current = job_manager.get_current_job();
    int previous = job_manager.get_previous_job();

    for (const auto& job : jobs) {
        if (pid_only) {
            for (pid_t pid : job->pids) {
                std::cout << pid << '\n';
            }
            continue;
        }

        std::string status_char = " ";
        if (job->job_id == current) {
            status_char = "+";
        } else if (job->job_id == previous) {
            status_char = "-";
        }

        std::string state_str;
        const JobState state = job->state.load(std::memory_order_relaxed);
        switch (state) {
            case JobState::RUNNING:
                state_str = "Running";
                break;
            case JobState::STOPPED:
                state_str = "Stopped";
                break;
            case JobState::DONE:
                state_str = "Done";
                break;
            case JobState::TERMINATED:
                state_str = "Terminated";
                break;
        }

        std::cout << "[" << job->job_id << "]" << status_char << " ";

        if (long_format) {
            std::cout << std::setw(8) << job->pids[0] << " ";
        }

        std::cout << std::setw(12) << std::left << state_str << " " << job->display_command()
                  << '\n';

        job->notified = true;
    }

    return 0;
}

int wait_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(
            args, {"Usage: wait [ID ...]",
                   "Wait for specified jobs or processes. Without IDs, waits for all."})) {
        return 0;
    }

    auto& job_manager = JobManager::instance();

    if (args.size() == 1) {
        auto jobs = job_manager.get_all_jobs();
        int last_exit_status = 0;

        for (const auto& job : jobs) {
            if (job->state.load(std::memory_order_relaxed) != JobState::RUNNING) {
                continue;
            }

            auto job_exit = job_control_helpers::wait_for_job_and_remove(job, job_manager);
            if (job_exit.has_value()) {
                last_exit_status = job_exit.value();
            }
        }

        return last_exit_status;
    }

    int last_exit_status = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& target = args[i];

        if (!target.empty() && target[0] == '%') {
            auto parsed_job_id = job_control_helpers::parse_job_specifier(target);
            if (!parsed_job_id.has_value()) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             target,
                             "Arguments must be process or job IDs",
                             {"Use 'jobs' to list available jobs"}});
                return 1;
            }

            int job_id = parsed_job_id.value();
            auto job = job_manager.get_job(job_id);
            if (!job) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             target,
                             "no such job",
                             {"Use 'jobs' to list available jobs"}});
                return 1;
            }

            auto job_exit = job_control_helpers::wait_for_job_and_remove(job, job_manager);
            if (job_exit.has_value()) {
                last_exit_status = job_exit.value();
            }
        } else {
            try {
                pid_t pid = std::stoi(target);
                int status = 0;
                if (waitpid(pid, &status, 0) < 0) {
                    print_error_errno({ErrorType::RUNTIME_ERROR, "wait", "waitpid", {}});
                    return 1;
                }

                auto interpreted = job_control_helpers::interpret_wait_status(status);
                if (interpreted.has_value()) {
                    last_exit_status = interpreted.value();
                }

                job_manager.mark_pid_completed(pid, status);
            } catch (...) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             target,
                             "Arguments must be process or job IDs",
                             {"Use 'jobs' to list available jobs"}});
                return 1;
            }
        }
    }

    return last_exit_status;
}

int disown_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args, {"Usage: disown [jobspec ...]",
                                   "Remove jobs from the shell's job table so they are not"
                                   " sent hangup signals."})) {
        return 0;
    }

    auto& job_manager = JobManager::instance();
    job_manager.update_job_statuses();

    bool disown_all = false;
    std::vector<int> targets;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-a" || args[i] == "--all") {
            disown_all = true;
            continue;
        }

        std::string job_spec = args[i];
        if (!job_spec.empty() && job_spec[0] == '%') {
            job_spec = job_spec.substr(1);
        }

        try {
            targets.push_back(std::stoi(job_spec));
        } catch (...) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         args[i],
                         "no such job",
                         {"Use 'jobs' to list available jobs"}});
            return 1;
        }
    }

    if (disown_all) {
        auto jobs = job_manager.get_all_jobs();
        targets.clear();
        targets.reserve(jobs.size());
        for (const auto& job : jobs) {
            targets.push_back(job->job_id);
        }
    }

    if (targets.empty()) {
        int current = job_manager.get_current_job();
        if (current == -1) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "",
                         "no current job",
                         {"Use 'jobs' to identify targets"}});
            return 1;
        }
        targets.push_back(current);
    }

    bool had_error = false;
    for (int job_id : targets) {
        auto job = job_manager.get_job(job_id);
        if (!job) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         std::to_string(job_id),
                         "no such job",
                         {"Use 'jobs' to list available jobs"}});
            had_error = true;
            continue;
        }

        job_manager.remove_job(job_id);
        if (g_shell && g_shell->shell_exec) {
            g_shell->shell_exec->remove_job(job_id);
        }
    }

    return had_error ? 1 : 0;
}

namespace {

bool is_blank(const std::string& value) {
    return std::all_of(value.begin(), value.end(),
                       [](unsigned char ch) { return std::isspace(ch); });
}

std::string normalize_name(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        return {};
    }
    std::string name;
    for (size_t i = 2; i < args.size(); ++i) {
        if (i > 2) {
            name.push_back(' ');
        }
        name.append(args[i]);
    }
    return name;
}

}  // namespace

int jobname_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args,
                            {"Usage: jobname JOB_SPEC NEW_NAME", "       jobname JOB_SPEC --clear",
                             "Assign a temporary display name to a job, or clear it."})) {
        return 0;
    }

    if (args.size() < 3) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "jobname",
                     "missing job spec or new name",
                     {"Usage: jobname JOB_SPEC NEW_NAME", "       jobname JOB_SPEC --clear"}});
        return 1;
    }

    auto& job_manager = JobManager::instance();
    job_manager.update_job_statuses();

    std::vector<std::string> resolve_args = {"jobname", args[1]};
    auto resolved = job_control_helpers::resolve_control_job_target(resolve_args, job_manager);
    if (!resolved) {
        return 1;
    }

    auto job = resolved->job;
    if (!job) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     args[1],
                     "no such job",
                     {"Use 'jobs' to list available jobs"}});
        return 1;
    }

    const bool clear_name = args.size() == 3 && (args[2] == "--clear" || args[2] == "-c");

    if (clear_name) {
        job->set_custom_name({});
    } else {
        std::string new_name = normalize_name(args);
        if (new_name.empty() || is_blank(new_name)) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "jobname",
                         "new name cannot be empty",
                         {"Provide the desired display name after the job spec"}});
            return 1;
        }

        job->set_custom_name(new_name);
    }
    std::cout << "[" << job->job_id << "] => " << job->display_command() << '\n';
    return 0;
}

int kill_command(const std::vector<std::string>& args) {
    auto run = [&]() -> int {
        if (builtin_handle_help(args,
                                {"Usage: kill [-s SIGNAL| -SIGNAL] ID ...",
                                 "Send a signal to processes or jobs. Use -l to list signals."})) {
            return 0;
        }
        if (args.size() < 2) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "",
                         "No targets specified",
                         {"Provide at least one PID or job ID"}});
            return 2;
        }

        int signal = SIGTERM;
        size_t start_index = 1;

        if (args[1].substr(0, 1) == "-") {
            if (args[1] == "-l") {
                std::cout << "HUP INT QUIT ILL TRAP ABRT BUS FPE KILL USR1 SEGV USR2 "
                             "PIPE ALRM TERM CHLD CONT STOP TSTP TTIN TTOU URG XCPU XFSZ "
                             "VTALRM PROF WINCH IO SYS"
                          << '\n';
                return 0;
            }

            if (args.size() < 3) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             "",
                             "No targets specified",
                             {"kill: usage: kill [-s sigspec | -n signum | -sigspec] pid "
                              "| jobspec ..."}});
                return 2;
            }

            std::string signal_str = args[1].substr(1);
            signal = job_control_helpers::parse_signal(signal_str);
            if (signal == -1) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             "kill",
                             "invalid option: " + args[1],
                             {"Use -l to list valid signals"}});
                return 1;
            }

            start_index = 2;
        }

        auto& job_manager = JobManager::instance();
        job_manager.update_job_statuses();

        auto is_stop_signal = [](int sig) {
            switch (sig) {
#ifdef SIGSTOP
                case SIGSTOP:
                    return true;
#endif
#ifdef SIGTSTP
                case SIGTSTP:
                    return true;
#endif
#ifdef SIGTTIN
                case SIGTTIN:
                    return true;
#endif
#ifdef SIGTTOU
                case SIGTTOU:
                    return true;
#endif
                default:
                    return false;
            }
        };

        auto is_continue_signal = [](int sig) {
#ifdef SIGCONT
            return sig == SIGCONT;
#else
            (void)sig;
            return false;
#endif
        };

        auto update_job_state_after_signal = [&](const std::shared_ptr<JobControlJob>& job) {
            if (!job) {
                return;
            }
            if (is_stop_signal(signal)) {
                job->state.store(JobState::STOPPED, std::memory_order_relaxed);
            } else if (is_continue_signal(signal)) {
                job->state.store(JobState::RUNNING, std::memory_order_relaxed);
                job->stop_notified.store(false, std::memory_order_relaxed);
            }
        };

        bool had_error = false;

        auto send_signal_to_job = [&](const std::shared_ptr<JobControlJob>& job) {
            if (!job) {
                return;
            }
            if (killpg(job->pgid, signal) < 0) {
                print_error_errno({ErrorType::RUNTIME_ERROR, "kill", "killpg", {}});
                had_error = true;
                return;
            }
            update_job_state_after_signal(job);
        };

        auto handle_job_target = [&](const std::string& spec, const std::string& original) -> bool {
            std::string job_spec = spec;
            job_control_helpers::trim_in_place(job_spec);
            if (job_spec.empty()) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             original,
                             "No such job",
                             {"Use 'jobs' to list available jobs"}});
                had_error = true;
                return false;
            }

            if (job_spec == "+" || job_spec == "-") {
                const bool is_current = job_spec[0] == '+';
                int target_id =
                    is_current ? job_manager.get_current_job() : job_manager.get_previous_job();
                if (target_id < 0) {
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 original,
                                 is_current ? "current job not set" : "no previous job",
                                 {"Use 'jobs' to list available jobs"}});
                    had_error = true;
                    return false;
                }

                auto job = job_manager.get_job(target_id);
                if (!job) {
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 original,
                                 "No such job",
                                 {"Use 'jobs' to list available jobs"}});
                    had_error = true;
                    return false;
                }

                send_signal_to_job(job);
                return true;
            }

            size_t consumed = 0;
            try {
                int parsed_value = std::stoi(job_spec, &consumed);
                if (consumed == job_spec.size()) {
                    auto job = job_manager.get_job(parsed_value);
                    if (!job) {
                        job = job_manager.get_job_by_pid(static_cast<pid_t>(parsed_value));
                    }

                    if (job) {
                        send_signal_to_job(job);
                        return true;
                    }

                    print_error({ErrorType::INVALID_ARGUMENT,
                                 original,
                                 "No such job",
                                 {"Use 'jobs' to list available jobs"}});
                    had_error = true;
                    return false;
                }
            } catch (...) {
                // Not a numeric job spec; fall back to command lookup.
            }

            bool ambiguous = false;
            auto job = job_control_helpers::find_job_by_command(job_spec, job_manager, ambiguous);
            if (job) {
                send_signal_to_job(job);
                return true;
            }

            had_error = true;
            if (ambiguous) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             original,
                             "multiple jobs match command",
                             {"Use job id or PID to disambiguate"}});
            } else {
                print_error({ErrorType::INVALID_ARGUMENT,
                             original,
                             "No such job",
                             {"Use 'jobs' to list available jobs"}});
            }
            return false;
        };

        for (size_t i = start_index; i < args.size(); ++i) {
            const std::string& target = args[i];

            if (!target.empty() && target[0] == '%') {
                handle_job_target(target.substr(1), target);
                continue;
            }

            size_t consumed = 0;
            bool treated_as_pid = false;
            try {
                pid_t pid = std::stoi(target, &consumed);
                if (consumed == target.size()) {
                    if (kill(pid, signal) < 0) {
                        print_error_errno({ErrorType::RUNTIME_ERROR, "kill", "kill", {}});
                        had_error = true;
                    } else {
                        auto job = job_manager.get_job_by_pid_or_pgid(pid);
                        update_job_state_after_signal(job);
                    }
                    treated_as_pid = true;
                }
            } catch (...) {
                // Not a numeric PID; fall back to job lookup.
            }

            if (!treated_as_pid) {
                handle_job_target(target, target);
            }
        }

        return had_error ? 1 : 0;
    };

    try {
        return run();
    } catch (...) {
        print_error({ErrorType::INVALID_ARGUMENT, "kill", "invalid argument", {}});
        return 1;
    }
}

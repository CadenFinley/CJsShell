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
#include <csignal>
#include <iomanip>
#include <iostream>

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
        perror("cjsh: bg: killpg");
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
            perror("fg: tcsetpgrp");
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
        perror("fg: killpg");
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
                    perror("wait");
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

/*
  kill_command.cpp

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

#include "kill_command.h"

#include <csignal>
#include <iostream>

#include "builtin_help.h"
#include "error_out.h"
#include "job_control.h"

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
                perror("kill");
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
                        perror("kill");
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

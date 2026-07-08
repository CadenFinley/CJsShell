/*
  exec_jobs.cpp

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

#include "exec.h"

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "job_control.h"
#include "shell.h"
#include "signal_handler.h"
#include "wait_status_utils.h"

namespace {

int extract_exit_code(int status) {
    return wait_status_utils::to_exit_code(status, 1);
}

}  // namespace

Job* Exec::find_job_locked(int job_id) {
    auto it = jobs.find(job_id);
    if (it == jobs.end()) {
        report_missing_job(job_id);
        return nullptr;
    }
    return &it->second;
}

Job* Exec::find_job_and_set_output_forwarding_locked(int job_id, bool forward) {
    Job* job = find_job_locked(job_id);
    if (job == nullptr) {
        return nullptr;
    }

    if (job->output_relay) {
        job->output_relay->forward.store(forward);
    }

    return job;
}

void Exec::report_missing_job(int job_id) {
    set_error(ErrorType::RUNTIME_ERROR, "job", "job [" + std::to_string(job_id) + "] not found");
    print_last_error();
}

void Exec::resume_job(Job& job, bool cont, std::string_view context) {
    if (!cont || !job.stopped) {
        return;
    }

    if (kill(-job.pgid, SIGCONT) < 0) {
        set_error(ErrorType::RUNTIME_ERROR, "kill",
                  "failed to send SIGCONT to " + std::string(context) + ": " +
                      std::string(strerror(errno)));
    }

    job.stopped = false;
}

int Exec::add_job(const Job& job) {
    std::lock_guard<std::mutex> lock(jobs_mutex);

    int job_id = next_job_id++;
    jobs[job_id] = job;

    return job_id;
}

void Exec::remove_job(int job_id) {
    std::lock_guard<std::mutex> lock(jobs_mutex);

    auto it = jobs.find(job_id);
    if (it != jobs.end()) {
        jobs.erase(it);
    }
}

void Exec::put_job_in_foreground(int job_id, bool cont) {
    std::lock_guard<std::mutex> lock(jobs_mutex);

    Job* job = find_job_and_set_output_forwarding_locked(job_id, true);
    if (job == nullptr) {
        return;
    }

    const bool main_shell_controls_terminal = shell_is_interactive &&
                                              (isatty(shell_terminal) != 0) && shell_pgid > 0 &&
                                              getpid() == shell_pgid && getpgrp() == shell_pgid;

    bool terminal_control_acquired = false;
    if (main_shell_controls_terminal) {
        if (tcsetpgrp(shell_terminal, job->pgid) == 0) {
            terminal_control_acquired = true;
        } else {
            if (errno != ENOTTY && errno != EINVAL && errno != EPERM) {
                set_error(ErrorType::RUNTIME_ERROR, "tcsetpgrp",
                          "warning: failed to set terminal control to job: " +
                              std::string(strerror(errno)));
            }
        }
    }

    resume_job(*job, cont, "job");

    jobs_mutex.unlock();

    wait_for_job(job_id);

    jobs_mutex.lock();

    if (terminal_control_acquired && main_shell_controls_terminal) {
        if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
            if (errno != ENOTTY && errno != EINVAL) {
                set_error(
                    ErrorType::RUNTIME_ERROR, "tcsetpgrp",
                    "warning: failed to restore terminal control: " + std::string(strerror(errno)));
            }
        }

        if (tcgetattr(shell_terminal, &shell_tmodes) == 0) {
            if (tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes) < 0) {
                set_error(ErrorType::RUNTIME_ERROR, "tcsetattr",
                          "failed to restore terminal attributes: " + std::string(strerror(errno)));
            }
        }
    }
}

void Exec::put_job_in_background(int job_id, bool cont) {
    std::lock_guard<std::mutex> lock(jobs_mutex);

    Job* job = find_job_and_set_output_forwarding_locked(job_id, false);
    if (job == nullptr) {
        return;
    }

    resume_job(*job, cont, "background job");
}

void Exec::set_job_output_forwarding(pid_t pgid, bool forward) {
    std::lock_guard<std::mutex> lock(jobs_mutex);
    for (auto& pair : jobs) {
        Job& job = pair.second;
        if (job.pgid == pgid) {
            if (job.output_relay) {
                job.output_relay->forward.store(forward);
            }
            break;
        }
    }
}

void Exec::wait_for_job(int job_id) {
    std::unique_lock<std::mutex> lock(jobs_mutex);

    auto it = jobs.find(job_id);
    if (it == jobs.end()) {
        return;
    }

    pid_t job_pgid = it->second.pgid;
    std::vector<pid_t> remaining_pids = it->second.pids;
    pid_t last_pid = it->second.last_pid;
    std::vector<pid_t> pid_order = it->second.pid_order;
    std::vector<int> pipeline_statuses = it->second.pipeline_statuses;

    lock.unlock();

    int status = 0;
    pid_t pid = 0;

    bool job_stopped = false;
    bool saw_last = false;
    int last_status = 0;
    int stop_signal = 0;

    while (!remaining_pids.empty()) {
        pid = waitpid(-job_pgid, &status, WUNTRACED);

        if (pid == -1) {
            if (errno == EINTR) {
                if (g_shell) {
                    g_shell->process_pending_signals();
                } else if (auto* signal_handler = SignalHandler::instance()) {
                    signal_handler->process_pending_signals(this);
                }
                continue;
            }
            if (errno == ECHILD) {
                remaining_pids.clear();
                break;
            }
            set_error(ErrorType::RUNTIME_ERROR, "waitpid",
                      "failed to wait for child process: " + std::string(strerror(errno)));
            break;
        }

        auto pid_it = std::find(remaining_pids.begin(), remaining_pids.end(), pid);
        if (pid_it != remaining_pids.end()) {
            remaining_pids.erase(pid_it);
        }

        auto order_it = std::find(pid_order.begin(), pid_order.end(), pid);
        if (order_it != pid_order.end()) {
            size_t index = static_cast<size_t>(std::distance(pid_order.begin(), order_it));
            if (index < pipeline_statuses.size() && (WIFEXITED(status) || WIFSIGNALED(status))) {
                pipeline_statuses[index] = extract_exit_code(status);
            }
        }

        if (pid == last_pid) {
        }

        if (pid == last_pid) {
            saw_last = true;
            last_status = status;
        }

        if (WIFSTOPPED(status)) {
            job_stopped = true;
            stop_signal = WSTOPSIG(status);
            break;
        }
    }

    lock.lock();

    it = jobs.find(job_id);
    if (it != jobs.end()) {
        Job& job = it->second;
        if (!pipeline_statuses.empty()) {
            job.pipeline_statuses = pipeline_statuses;
        }

        if (job_stopped) {
            job.stopped = true;
            job.status = status;

            auto job_control = JobManager::instance().get_job_by_pgid(job_pgid);
            if (!job_control) {
                auto jobs_snapshot = JobManager::instance().get_all_jobs();
                for (const auto& candidate : jobs_snapshot) {
                    if (!candidate) {
                        continue;
                    }
                    if (candidate->pgid == job_pgid ||
                        std::find(candidate->pids.begin(), candidate->pids.end(), job_pgid) !=
                            candidate->pids.end()) {
                        job_control = candidate;
                        break;
                    }
                }
            }

            const bool should_auto_background =
                job.auto_background_on_stop && stop_signal == SIGTSTP && job.pgid > 0;

            if (should_auto_background) {
                if (job.auto_background_on_stop_silent && job.output_relay) {
                    job.output_relay->forward.store(false);
                }
                resume_job(job, true, "background job");
                job.background = true;
                job.completed = false;
                last_exit_code = 0;

                if (job_control) {
                    job_control->state.store(JobState::RUNNING, std::memory_order_relaxed);
                    job_control->background.store(true, std::memory_order_relaxed);
                    job_control->stop_notified.store(false, std::memory_order_relaxed);
                }

                JobManager::instance().set_last_background_pid(job.last_pid);

                const std::string& display_command =
                    job_control ? job_control->display_command() : job.command;
                std::cerr << "\n[" << job_id << "]+ " << display_command << " &" << '\n';
            } else {
                last_exit_code = 128 + SIGTSTP;

                if (job_control) {
                    JobManager::instance().notify_job_stopped(job_control);
                }
            }
        } else {
            job.completed = true;
            job.stopped = false;
            job.status = status;

            int final_status = saw_last ? last_status : status;
            const bool exited_or_signaled = WIFEXITED(final_status) || WIFSIGNALED(final_status);
            if (!exited_or_signaled) {
                final_status = (job.last_status != 0) ? job.last_status : job.status;
            }
            job.last_status = final_status;

            if (WIFEXITED(final_status)) {
                int exit_status = extract_exit_code(final_status);
                last_exit_code = exit_status;
                job.completed = true;
                auto exit_result = job_utils::make_exit_error_result(
                    job.command, exit_status, "command completed successfully",
                    "command failed with exit code ");
                set_error(exit_result.type, job.command, exit_result.message,
                          exit_result.suggestions);
            } else if (WIFSIGNALED(final_status)) {
                last_exit_code = 128 + WTERMSIG(final_status);
                set_error(ErrorType::RUNTIME_ERROR, job.command,
                          "command terminated by signal " + std::to_string(WTERMSIG(final_status)));
            }
        }
    }
}

void Exec::handle_child_signal(pid_t pid, int status) {
    static bool use_signal_masking = false;
    static int signal_count = 0;

    if (++signal_count > 10) {
        use_signal_masking = true;
    }

    std::unique_ptr<SignalMask> mask;
    if (use_signal_masking) {
        mask = std::make_unique<SignalMask>(SIGCHLD);
    }

    std::lock_guard<std::mutex> lock(jobs_mutex);

    for (auto& job_pair : jobs) {
        int job_id = job_pair.first;
        Job& job = job_pair.second;

        auto it = std::find(job.pids.begin(), job.pids.end(), pid);
        if (it != job.pids.end()) {
            auto order_it = std::find(job.pid_order.begin(), job.pid_order.end(), pid);
            if (order_it != job.pid_order.end()) {
                size_t index = static_cast<size_t>(std::distance(job.pid_order.begin(), order_it));
                int recorded = -1;
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    recorded = extract_exit_code(status);
                }
                if (recorded != -1 && index < job.pipeline_statuses.size()) {
                    job.pipeline_statuses[index] = recorded;
                }
            }
            if (pid == job.last_pid) {
                job.last_status = status;
            }
            if (WIFSTOPPED(status)) {
                job.stopped = true;
                job.status = status;
            } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                job.pids.erase(it);

                if (job.pids.empty()) {
                    job.completed = true;
                    job.stopped = false;
                    job.status = status;

                    if (job.background && !job.suppress_notifications) {
                        if (WIFSIGNALED(status)) {
                            std::cerr << "\n[" << job_id << "] Terminated\t" << job.command << '\n';
                        } else {
                            const int exit_code = extract_exit_code(status);
                            if (exit_code == 0) {
                                std::cerr << "\n[" << job_id << "] Done\t" << job.command << '\n';
                            } else {
                                std::cerr << "\n[" << job_id << "] Exit " << exit_code << '\t'
                                          << job.command << '\n';
                            }
                        }
                    }
                }
            }
            break;
        }
    }
}

std::map<int, Job> Exec::get_jobs() {
    std::lock_guard<std::mutex> lock(jobs_mutex);
    return jobs;
}

void Exec::terminate_all_child_process(int signal) {
    struct JobRecord {
        int id;
        Job job;
    };

    std::vector<JobRecord> job_snapshot;
    {
        std::lock_guard<std::mutex> lock(jobs_mutex);
        job_snapshot.reserve(jobs.size());
        for (const auto& pair : jobs) {
            job_snapshot.push_back({pair.first, pair.second});
        }
    }

    const auto send_signal_to_job = [](const Job& job, int signal) -> bool {
        bool pid_signaled = false;

        for (pid_t pid : job.pids) {
            if (pid <= 0) {
                continue;
            }
            if (kill(pid, signal) == 0) {
                pid_signaled = true;
            }
        }

        if (job.pgid <= 0) {
            return pid_signaled;
        }

        if (killpg(job.pgid, signal) == 0) {
            return true;
        }

        const int group_errno = errno;
        if (group_errno == ESRCH) {
            return pid_signaled;
        }

        if ((group_errno == EPERM || group_errno == EACCES) && pid_signaled) {
            return true;
        }

        print_error({ErrorType::RUNTIME_ERROR,
                     ErrorSeverity::WARNING,
                     "killpg",
                     "failed to send signal " + std::to_string(signal) + " to pgid " +
                         std::to_string(job.pgid) + ": " + std::string(strerror(group_errno)),
                     {}});
        return pid_signaled;
    };

    bool signaled_any = false;
    for (const auto& entry : job_snapshot) {
        const Job& job = entry.job;
        if (job.completed) {
            continue;
        }

#ifdef SIGCONT
        if (job.stopped && job.pgid > 0) {
            (void)send_signal_to_job(job, SIGCONT);
        }
#endif

        if (send_signal_to_job(job, signal)) {
            signaled_any = true;
        }

        if (job.pgid > 0 || !job.pids.empty()) {
            std::cerr << "[" << entry.id << "] Terminated\t" << job.command << '\n';
        }
    }

    for (const auto& entry : job_snapshot) {
        const Job& job = entry.job;
        if (job.completed) {
            continue;
        }

        (void)send_signal_to_job(job, SIGKILL);
    }

    int status = 0;
    int zombie_count = 0;
    const int max_terminate_iterations = 100;
    while (zombie_count < max_terminate_iterations) {
        pid_t reap_pid = waitpid(-1, &status, WNOHANG);
        if (reap_pid <= 0) {
            break;
        }
        ++zombie_count;
    }

    if (zombie_count >= max_terminate_iterations) {
        print_error({ErrorType::RUNTIME_ERROR,
                     ErrorSeverity::WARNING,
                     "terminate_all_child_process",
                     "hit maximum cleanup iterations",
                     {}});
    }

    {
        std::lock_guard<std::mutex> lock(jobs_mutex);
        for (auto& pair : jobs) {
            pair.second.completed = true;
            pair.second.stopped = false;
            pair.second.pids.clear();
            pair.second.status = 0;
        }
    }

    if (!signaled_any) {
        set_error(ErrorType::RUNTIME_ERROR, "", "No child processes to terminate");
    } else {
        set_error(ErrorType::RUNTIME_ERROR, "", "All child processes terminated");
    }
}

void Exec::abandon_all_child_processes() {
    if (!jobs_mutex.try_lock()) {
        return;
    }
    jobs.clear();
    jobs_mutex.unlock();
}

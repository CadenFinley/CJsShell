#include "job_control.h"

#include "builtin_help.h"
#include "cjsh_filesystem.h"
#include "suggestion_utils.h"

#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>

#include "error_out.h"

namespace {

int parse_signal(const std::string& signal_str) {
    if (signal_str.empty())
        return SIGTERM;

    if (std::isdigit(signal_str[0]) != 0) {
        try {
            return std::stoi(signal_str);
        } catch (...) {
            return -1;
        }
    }

    std::string name = signal_str;
    if (name.substr(0, 3) == "SIG") {
        name = name.substr(3);
    }

    std::transform(name.begin(), name.end(), name.begin(), ::toupper);

    if (name == "HUP")
        return SIGHUP;
    if (name == "INT")
        return SIGINT;
    if (name == "QUIT")
        return SIGQUIT;
    if (name == "KILL")
        return SIGKILL;
    if (name == "TERM")
        return SIGTERM;
    if (name == "USR1")
        return SIGUSR1;
    if (name == "USR2")
        return SIGUSR2;
    if (name == "STOP")
        return SIGSTOP;
    if (name == "CONT")
        return SIGCONT;
    if (name == "TSTP")
        return SIGTSTP;

    return -1;
}

}  // namespace

namespace job_utils {

ExitErrorResult make_exit_error_result(const std::string& command, int exit_code,
                                       const std::string& success_message,
                                       const std::string& failure_prefix) {
    ExitErrorResult result{ErrorType::RUNTIME_ERROR, success_message, {}};
    if (exit_code == 0) {
        return result;
    }
    result.message = failure_prefix + std::to_string(exit_code);
    if (exit_code == 127) {
        if (!cjsh_filesystem::command_exists(command)) {
            result.type = ErrorType::COMMAND_NOT_FOUND;
            result.message.clear();
            result.suggestions = suggestion_utils::generate_command_suggestions(command);
            return result;
        }
    } else if (exit_code == 126) {
        result.type = ErrorType::PERMISSION_DENIED;
    }
    return result;
}

bool command_consumes_terminal_stdin(const Command& cmd) {
    if (!cmd.input_file.empty() || !cmd.here_doc.empty() || !cmd.here_string.empty()) {
        return false;
    }

    if (cmd.fd_redirections.count(0) > 0 || cmd.fd_duplications.count(0) > 0) {
        return false;
    }

    return true;
}

bool pipeline_consumes_terminal_stdin(const std::vector<Command>& commands) {
    if (commands.empty()) {
        return false;
    }

    if (commands.back().background) {
        return false;
    }

    return command_consumes_terminal_stdin(commands.front());
}

}  // namespace job_utils

JobManager& JobManager::instance() {
    static JobManager instance;
    return instance;
}

int JobManager::add_job(pid_t pgid, const std::vector<pid_t>& pids, const std::string& command,
                        bool background, bool reads_stdin) {
    int job_id = next_job_id++;
    auto job =
        std::make_shared<JobControlJob>(job_id, pgid, pids, command, background, reads_stdin);
    jobs[job_id] = job;

    update_current_previous(job_id);

    return job_id;
}

void JobManager::remove_job(int job_id) {
    auto it = jobs.find(job_id);
    if (it != jobs.end()) {
        if (current_job == job_id) {
            current_job = previous_job;
            previous_job = -1;
        } else if (previous_job == job_id) {
            previous_job = -1;
        }

        jobs.erase(it);
    }
}

std::shared_ptr<JobControlJob> JobManager::get_job(int job_id) {
    auto it = jobs.find(job_id);
    return it != jobs.end() ? it->second : nullptr;
}

std::shared_ptr<JobControlJob> JobManager::get_job_by_pgid(pid_t pgid) {
    for (const auto& pair : jobs) {
        if (pair.second->pgid == pgid) {
            return pair.second;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<JobControlJob>> JobManager::get_all_jobs() {
    std::vector<std::shared_ptr<JobControlJob>> result;
    result.reserve(jobs.size());
    for (const auto& pair : jobs) {
        result.push_back(pair.second);
    }

    std::sort(result.begin(), result.end(),
              [](const std::shared_ptr<JobControlJob>& a, const std::shared_ptr<JobControlJob>& b) {
                  return a->job_id < b->job_id;
              });

    return result;
}

void JobManager::update_job_status() {
    for (auto& pair : jobs) {
        auto job = pair.second;

        int status = 0;
        for (pid_t pid : job->pids) {
            pid_t result = waitpid(pid, &status, WNOHANG | WUNTRACED | WCONTINUED);

            if (result > 0) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    job->state = WIFEXITED(status) ? JobState::DONE : JobState::TERMINATED;
                    job->exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status);
                } else if (WIFSTOPPED(status)) {
                    job->state = JobState::STOPPED;
                } else if (WIFCONTINUED(status)) {
                    job->state = JobState::RUNNING;
                }
            } else if (result == -1) {
            }
        }
    }
}

void JobManager::set_current_job(int job_id) {
    update_current_previous(job_id);
}

void JobManager::update_current_previous(int new_current) {
    if (current_job != new_current) {
        previous_job = current_job;
        current_job = new_current;
    }
}

void JobManager::cleanup_finished_jobs() {
    std::vector<int> to_remove;

    for (const auto& pair : jobs) {
        auto job = pair.second;
        if (job->state == JobState::DONE || job->state == JobState::TERMINATED) {
            if (!job->notified) {
                if (job->state == JobState::DONE) {
                    std::cerr << "\n[" << job->job_id << "] Done\t" << job->command << '\n';
                } else {
                    std::cerr << "\n[" << job->job_id << "] Terminated\t" << job->command << '\n';
                }
                job->notified = true;
            }

            if (job->notified) {
                to_remove.push_back(job->job_id);
            }
        }
    }

    for (int job_id : to_remove) {
        remove_job(job_id);
    }
}

bool JobManager::foreground_job_reads_stdin() {
    if (jobs.empty()) {
        return false;
    }

    int foreground_id = current_job;
    if (foreground_id == -1) {
        return false;
    }

    auto it = jobs.find(foreground_id);
    if (it == jobs.end()) {
        return false;
    }

    const auto& job = it->second;
    if (job->background || !job->reads_stdin) {
        return false;
    }

    if (job->awaiting_stdin_signal) {
        return true;
    }

    if (job->stdin_signal_count > 0) {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = now - job->last_stdin_signal_time;
        if (elapsed <= std::chrono::milliseconds(250)) {
            return true;
        }
    }
    return false;
}

void JobManager::mark_job_reads_stdin(pid_t pid, bool reads_stdin) {
    for (const auto& pair : jobs) {
        const auto& job = pair.second;
        if (job->pgid == pid ||
            std::find(job->pids.begin(), job->pids.end(), pid) != job->pids.end()) {
            if (job->reads_stdin != reads_stdin) {
                job->reads_stdin = reads_stdin;
            }
            return;
        }
    }
}

void JobManager::record_stdin_signal(pid_t pid, int signal_number) {
    auto now = std::chrono::steady_clock::now();
    for (const auto& pair : jobs) {
        const auto& job = pair.second;
        if (job->pgid == pid ||
            std::find(job->pids.begin(), job->pids.end(), pid) != job->pids.end()) {
            job->reads_stdin = true;
            job->awaiting_stdin_signal = true;
            const auto clamped_signal = std::clamp(signal_number, 0, 255);
            job->last_stdin_signal = static_cast<std::uint8_t>(clamped_signal);
            if (job->stdin_signal_count < std::numeric_limits<std::uint16_t>::max()) {
                job->stdin_signal_count = static_cast<std::uint16_t>(job->stdin_signal_count + 1);
            }
            job->last_stdin_signal_time = now;
            return;
        }
    }
}

void JobManager::clear_stdin_signal(pid_t pid) {
    for (const auto& pair : jobs) {
        const auto& job = pair.second;
        if (job->pgid == pid ||
            std::find(job->pids.begin(), job->pids.end(), pid) != job->pids.end()) {
            if (job->awaiting_stdin_signal || job->stdin_signal_count > 0) {
                job->awaiting_stdin_signal = false;
                job->last_stdin_signal = 0;
                job->stdin_signal_count = 0;
                job->last_stdin_signal_time = std::chrono::steady_clock::time_point::min();
            }
            return;
        }
    }
}

int jobs_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(
            args, {"Usage: jobs [-lp]", "List active jobs. -l shows PIDs, -p prints PIDs only."})) {
        return 0;
    }
    auto& job_manager = JobManager::instance();
    job_manager.update_job_status();

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
        }
    }

    auto jobs = job_manager.get_all_jobs();
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
        switch (job->state) {
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

        std::cout << std::setw(12) << std::left << state_str << " " << job->command << '\n';

        job->notified = true;
    }

    return 0;
}

int fg_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args, {"Usage: fg [%JOB]", "Bring a job to the foreground."})) {
        return 0;
    }
    auto& job_manager = JobManager::instance();
    job_manager.update_job_status();

    int job_id = job_manager.get_current_job();

    if (args.size() > 1) {
        std::string job_spec = args[1];
        if (job_spec.substr(0, 1) == "%") {
            job_spec = job_spec.substr(1);
        }

        try {
            job_id = std::stoi(job_spec);
        } catch (...) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         args[1],
                         "no such job",
                         {"Use 'jobs' to list available jobs"}});
            return 1;
        }
    }

    auto job = job_manager.get_job(job_id);
    if (!job) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     std::to_string(job_id),
                     "no such job",
                     {"Use 'jobs' to list available jobs"}});
        return 1;
    }

    if (isatty(STDIN_FILENO) != 0) {
        if (tcsetpgrp(STDIN_FILENO, job->pgid) < 0) {
            perror("fg: tcsetpgrp");
            return 1;
        }
    }

    if (job->state == JobState::STOPPED) {
        if (killpg(job->pgid, SIGCONT) < 0) {
            perror("fg: killpg");
            return 1;
        }
    }

    job->state = JobState::RUNNING;
    job_manager.set_current_job(job_id);

    std::cout << job->command << '\n';

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
        job->state = JobState::STOPPED;
        return 128 + WSTOPSIG(status);
    }
    if (WIFSIGNALED(status)) {
        job_manager.remove_job(job_id);
        return 128 + WTERMSIG(status);
    }

    return 0;
}

int bg_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args,
                            {"Usage: bg [%JOB]", "Resume a stopped job in the background."})) {
        return 0;
    }
    auto& job_manager = JobManager::instance();
    job_manager.update_job_status();

    int job_id = job_manager.get_current_job();

    if (args.size() > 1) {
        std::string job_spec = args[1];
        if (job_spec.substr(0, 1) == "%") {
            job_spec = job_spec.substr(1);
        }

        try {
            job_id = std::stoi(job_spec);
        } catch (...) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         args[1],
                         "no such job",
                         {"Use 'jobs' to list available jobs"}});
            return 1;
        }
    }

    auto job = job_manager.get_job(job_id);
    if (!job) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     std::to_string(job_id),
                     "no such job",
                     {"Use 'jobs' to list available jobs"}});
        return 1;
    }

    if (job->state != JobState::STOPPED) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     std::to_string(job_id),
                     "not stopped",
                     {"Use 'jobs' to list job states"}});
        return 1;
    }

    if (killpg(job->pgid, SIGCONT) < 0) {
        perror("cjsh: bg: killpg");
        return 1;
    }

    job->state = JobState::RUNNING;
    std::cout << "[" << job_id << "]+ " << job->command << " &" << '\n';

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
            if (job->state == JobState::RUNNING) {
                int status = 0;
                for (pid_t pid : job->pids) {
                    if (waitpid(pid, &status, 0) > 0) {
                        if (WIFEXITED(status)) {
                            last_exit_status = WEXITSTATUS(status);
                        } else if (WIFSIGNALED(status)) {
                            last_exit_status = 128 + WTERMSIG(status);
                        }
                    }
                }
                job_manager.remove_job(job->job_id);
            }
        }

        return last_exit_status;
    }

    int last_exit_status = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        std::string target = args[i];

        if (target.substr(0, 1) == "%") {
            try {
                int job_id = std::stoi(target.substr(1));
                auto job = job_manager.get_job(job_id);
                if (!job) {
                    std::cerr << "wait: %" << job_id << ": no such job" << '\n';
                    return 1;
                }

                int status = 0;
                for (pid_t pid : job->pids) {
                    if (waitpid(pid, &status, 0) > 0) {
                        if (WIFEXITED(status)) {
                            last_exit_status = WEXITSTATUS(status);
                        } else if (WIFSIGNALED(status)) {
                            last_exit_status = 128 + WTERMSIG(status);
                        }
                    }
                }

                job_manager.remove_job(job_id);
            } catch (...) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             target,
                             "Arguments must be process or job IDs",
                             {"Use 'jobs' to list available jobs"}});
                return 1;
            }
        } else {
            try {
                pid_t pid = std::stoi(target);
                int status = 0;
                if (waitpid(pid, &status, 0) < 0) {
                    perror("wait");
                    return 1;
                }

                if (WIFEXITED(status)) {
                    last_exit_status = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    last_exit_status = 128 + WTERMSIG(status);
                }
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

int kill_command(const std::vector<std::string>& args) {
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
        signal = parse_signal(signal_str);
        if (signal == -1) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         args[1],
                         "Invalid signal specification",
                         {"Use -l to list valid signals"}});
            return 1;
        }

        start_index = 2;
    }

    auto& job_manager = JobManager::instance();

    for (size_t i = start_index; i < args.size(); ++i) {
        std::string target = args[i];

        if (target.substr(0, 1) == "%") {
            try {
                int job_id = std::stoi(target.substr(1));
                auto job = job_manager.get_job(job_id);
                if (!job) {
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 target,
                                 "No such job",
                                 {"Use 'jobs' to list available jobs"}});
                    continue;
                }

                if (killpg(job->pgid, signal) < 0) {
                    perror("kill");
                }
            } catch (...) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             target,
                             "Arguments must be process or job IDs",
                             {"Use 'jobs' to list available jobs"}});
            }
        } else {
            try {
                pid_t pid = std::stoi(target);
                if (kill(pid, signal) < 0) {
                    perror("kill");
                }
            } catch (...) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             target,
                             "Arguments must be process or job IDs",
                             {"Use 'jobs' to list available jobs"}});
            }
        }
    }

    return 0;
}

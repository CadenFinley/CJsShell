#include "job_control.h"

#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "shell.h"
#include "signal_handler.h"
#include "suggestion_utils.h"

#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>

#include "error_out.h"

namespace {

std::atomic<pid_t> g_atomic_last_background_pid{-1};

std::string_view trim_view(const std::string& value) {
    const auto start = value.find_first_not_of(" \t");
    if (start == std::string::npos) {
        return std::string_view{};
    }
    const auto end = value.find_last_not_of(" \t");
    return std::string_view(value).substr(start, end - start + 1);
}

bool job_command_matches(const std::shared_ptr<JobControlJob>& job, const std::string& spec) {
    if (spec.empty()) {
        return false;
    }

    const auto& comparison_source = job->has_custom_name() ? job->custom_name : job->command;
    const auto trimmed_command = trim_view(comparison_source);
    if (trimmed_command.empty()) {
        return false;
    }

    const std::string_view spec_view(spec);
    if (trimmed_command == spec_view) {
        return true;
    }

    const auto first_space = trimmed_command.find_first_of(" \t");
    if (first_space == std::string::npos) {
        return false;
    }

    return trimmed_command.substr(0, first_space) == spec_view;
}

std::shared_ptr<JobControlJob> resolve_job_argument(const std::vector<std::string>& args,
                                                    JobManager& job_manager, int& job_id_out) {
    job_id_out = job_manager.get_current_job();

    if (args.size() <= 1) {
        auto job = job_manager.get_job(job_id_out);
        if (!job) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         std::to_string(job_id_out),
                         "no such job",
                         {"Use 'jobs' to list available jobs"}});
        }
        return job;
    }

    std::string job_spec = args[1];
    if (!job_spec.empty() && job_spec[0] == '%') {
        job_spec.erase(0, 1);
    }

    job_control_helpers::trim_in_place(job_spec);

    if (job_spec.empty()) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     args[1],
                     "no such job",
                     {"Use 'jobs' to list available jobs"}});
        return nullptr;
    }

    size_t consumed = 0;
    try {
        int parsed_value = std::stoi(job_spec, &consumed);
        if (consumed == job_spec.size()) {
            auto job = job_manager.get_job(parsed_value);
            if (job) {
                job_id_out = parsed_value;
                return job;
            }

            auto job_by_pid = job_manager.get_job_by_pid(static_cast<pid_t>(parsed_value));
            if (job_by_pid) {
                job_id_out = job_by_pid->job_id;
                return job_by_pid;
            }
        }
    } catch (...) {
    }

    bool ambiguous = false;
    auto job = job_control_helpers::find_job_by_command(job_spec, job_manager, ambiguous);
    if (job) {
        job_id_out = job->job_id;
        return job;
    }

    if (ambiguous) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     args[1],
                     "multiple jobs match command",
                     {"Use job id or PID to disambiguate"}});
    } else {
        print_error({ErrorType::INVALID_ARGUMENT,
                     args[1],
                     "no such job",
                     {"Use 'jobs' to list available jobs"}});
    }

    return nullptr;
}

}  // namespace

namespace job_control_helpers {

int parse_signal(const std::string& signal_str) {
    if (signal_str.empty()) {
        return SIGTERM;
    }

    int resolved = SignalHandler::name_to_signal(signal_str);
    if (resolved == 0) {
        // POSIX allows signal 0 as a special case for error checking.
        return 0;
    }

    if (resolved > 0 && SignalHandler::is_valid_signal(resolved)) {
        return resolved;
    }

    return -1;
}

void trim_in_place(std::string& value) {
    const auto start = value.find_first_not_of(" \t");
    if (start == std::string::npos) {
        value.clear();
        return;
    }
    const auto end = value.find_last_not_of(" \t");
    value = value.substr(start, end - start + 1);
}

std::shared_ptr<JobControlJob> find_job_by_command(const std::string& spec, JobManager& job_manager,
                                                   bool& ambiguous) {
    ambiguous = false;
    std::shared_ptr<JobControlJob> match;

    const auto jobs = job_manager.get_all_jobs();
    for (const auto& job : jobs) {
        if (job_command_matches(job, spec)) {
            if (match) {
                ambiguous = true;
                return nullptr;
            }
            match = job;
        }
    }

    return match;
}

std::optional<ResolvedJob> resolve_control_job_target(const std::vector<std::string>& args,
                                                      JobManager& job_manager) {
    int job_id = job_manager.get_current_job();
    auto job = resolve_job_argument(args, job_manager, job_id);
    if (!job) {
        return std::nullopt;
    }
    return ResolvedJob{job->job_id, job};
}

std::optional<int> interpret_wait_status(int status) {
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return std::nullopt;
}

std::optional<int> wait_for_job_and_remove(const std::shared_ptr<JobControlJob>& job,
                                           JobManager& job_manager) {
    int status = 0;
    std::optional<int> last_exit_status;
    for (pid_t pid : job->pids) {
        if (waitpid(pid, &status, 0) > 0) {
            auto interpreted = interpret_wait_status(status);
            if (interpreted.has_value()) {
                last_exit_status = interpreted;
            }
        }
    }
    job_manager.remove_job(job->job_id);
    return last_exit_status;
}

std::optional<int> parse_job_specifier(const std::string& target) {
    if (target.empty() || target[0] != '%') {
        return std::nullopt;
    }

    try {
        return std::stoi(target.substr(1));
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace job_control_helpers

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

    if (cmd.has_fd_redirection(0) || cmd.has_fd_duplication(0)) {
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

JobControlJob::JobControlJob(int id, pid_t group_id, const std::vector<pid_t>& process_ids,
                             const std::string& cmd, bool is_background, bool consumes_stdin)
    : job_id(id),
      pgid(group_id),
      pids(process_ids),
      command(cmd),
      background(is_background),
      reads_stdin(consumes_stdin) {
}

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

std::shared_ptr<JobControlJob> JobManager::get_job_by_pid(pid_t pid) {
    for (const auto& pair : jobs) {
        const auto& job = pair.second;
        if (std::find(job->pids.begin(), job->pids.end(), pid) != job->pids.end()) {
            return job;
        }
    }
    return nullptr;
}

std::shared_ptr<JobControlJob> JobManager::get_job_by_pid_or_pgid(pid_t id) {
    for (const auto& pair : jobs) {
        const auto& job = pair.second;
        const bool matches_process =
            job->pgid == id || std::find(job->pids.begin(), job->pids.end(), id) != job->pids.end();
        if (matches_process) {
            return job;
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
                    job->stop_notified = false;
                }
            } else if (result == -1) {
            }
        }
    }
}

void JobManager::set_current_job(int job_id) {
    update_current_previous(job_id);
}

int JobManager::get_current_job() const {
    return current_job;
}

int JobManager::get_previous_job() const {
    return previous_job;
}

void JobManager::set_last_background_pid(pid_t pid) {
    last_background_pid = pid;
    g_atomic_last_background_pid.store(pid, std::memory_order_relaxed);
}

pid_t JobManager::get_last_background_pid() const {
    return last_background_pid;
}

pid_t JobManager::get_last_background_pid_atomic() {
    return g_atomic_last_background_pid.load(std::memory_order_relaxed);
}

void JobManager::set_shell(Shell* shell) {
    shell_ref = shell;
}

void JobManager::notify_job_stopped(const std::shared_ptr<JobControlJob>& job) const {
    if (!job || job->stop_notified) {
        return;
    }

    if (!config::interactive_mode && !config::force_interactive) {
        return;
    }

    job->state = JobState::STOPPED;

    char status_char = ' ';
    if (job->job_id == current_job) {
        status_char = '+';
    } else if (job->job_id == previous_job) {
        status_char = '-';
    }

    std::cerr << "\n[" << job->job_id << "]" << status_char << "  Stopped\t"
              << job->display_command() << '\n';

    job->stop_notified = true;
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
                    const char* label = job->exit_status == 0 ? "Done" : "Exit";
                    std::cerr << "\n[" << job->job_id << "] " << label;
                    if (job->exit_status != 0) {
                        std::cerr << ' ' << job->exit_status;
                    }
                    std::cerr << "\t" << job->display_command() << '\n';
                } else {
                    std::cerr << "\n[" << job->job_id << "] Terminated\t" << job->display_command()
                              << '\n';
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

void JobManager::clear_stdin_signal(pid_t pid) {
    auto job = get_job_by_pid_or_pgid(pid);
    if (!job) {
        return;
    }

    if (job->awaiting_stdin_signal || job->stdin_signal_count > 0) {
        job->awaiting_stdin_signal = false;
        job->last_stdin_signal = 0;
        job->stdin_signal_count = 0;
        job->last_stdin_signal_time = std::chrono::steady_clock::time_point::min();
    }
}

void JobManager::clear_all_jobs() {
    jobs.clear();
    current_job = -1;
    previous_job = -1;
    last_background_pid = -1;
}

void JobManager::mark_pid_completed(pid_t pid, int status) {
    for (auto& pair : jobs) {
        auto& job = pair.second;
        if (!job) {
            continue;
        }
        auto pid_it = std::find(job->pids.begin(), job->pids.end(), pid);
        if (pid_it == job->pids.end()) {
            continue;
        }

        job->pids.erase(pid_it);

        if (WIFEXITED(status)) {
            job->state = JobState::DONE;
            job->exit_status = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            job->state = JobState::TERMINATED;
            job->exit_status = WTERMSIG(status);
        }

        if (job->pids.empty()) {
            remove_job(job->job_id);
        }
        return;
    }
}

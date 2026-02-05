/*
  job_control.h

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

#pragma once

#include <sys/types.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "error_out.h"
#include "parser.h"

class Shell;

enum class JobState : std::uint8_t {
    RUNNING,
    STOPPED,
    DONE,
    TERMINATED
};

struct JobControlJob {
    int job_id;
    pid_t pgid;
    std::vector<pid_t> pids;
    std::string command;
    JobState state{};
    int exit_status{};
    bool notified{false};
    bool stop_notified{false};
    bool background{false};
    bool reads_stdin{false};
    bool awaiting_stdin_signal{false};
    std::uint8_t last_stdin_signal{0};
    std::uint16_t stdin_signal_count{0};
    std::chrono::steady_clock::time_point last_stdin_signal_time{
        std::chrono::steady_clock::time_point::min()};
    std::string custom_name;

    JobControlJob(int id, pid_t group_id, const std::vector<pid_t>& process_ids,
                  const std::string& cmd, bool is_background, bool consumes_stdin);

    bool has_custom_name() const {
        return !custom_name.empty();
    }

    void set_custom_name(std::string name) {
        custom_name = std::move(name);
    }

    void clear_custom_name() {
        custom_name.clear();
    }

    const std::string& display_command() const {
        return custom_name.empty() ? command : custom_name;
    }
};

class JobManager {
   public:
    static JobManager& instance();

    int add_job(pid_t pgid, const std::vector<pid_t>& pids, const std::string& command,
                bool background = false, bool reads_stdin = true);

    void remove_job(int job_id);

    std::shared_ptr<JobControlJob> get_job(int job_id);

    std::shared_ptr<JobControlJob> get_job_by_pgid(pid_t pgid);

    std::shared_ptr<JobControlJob> get_job_by_pid(pid_t pid);

    std::shared_ptr<JobControlJob> get_job_by_pid_or_pgid(pid_t id);

    std::vector<std::shared_ptr<JobControlJob>> get_all_jobs();

    void update_job_status();

    void set_current_job(int job_id);

    int get_current_job() const;

    int get_previous_job() const;

    void set_last_background_pid(pid_t pid);

    pid_t get_last_background_pid() const;

    static pid_t get_last_background_pid_atomic();

    void cleanup_finished_jobs();

    void set_shell(Shell* shell);

    void notify_job_stopped(const std::shared_ptr<JobControlJob>& job) const;

    bool foreground_job_reads_stdin();

    void clear_stdin_signal(pid_t pid);

    void clear_all_jobs();
    void mark_pid_completed(pid_t pid, int status);

   private:
    JobManager() = default;
    std::unordered_map<int, std::shared_ptr<JobControlJob>> jobs;
    int next_job_id = 1;
    int current_job = -1;
    int previous_job = -1;
    pid_t last_background_pid = -1;
    Shell* shell_ref = nullptr;

    void update_current_previous(int new_current);
};

namespace job_control_helpers {

struct ResolvedJob {
    int job_id;
    std::shared_ptr<JobControlJob> job;
};

std::shared_ptr<JobControlJob> find_job_by_command(const std::string& spec, JobManager& job_manager,
                                                   bool& ambiguous);

void trim_in_place(std::string& value);

std::optional<ResolvedJob> resolve_control_job_target(const std::vector<std::string>& args,
                                                      JobManager& job_manager);

std::optional<int> interpret_wait_status(int status);

std::optional<int> wait_for_job_and_remove(const std::shared_ptr<JobControlJob>& job,
                                           JobManager& job_manager);

std::optional<int> parse_job_specifier(const std::string& target);

int parse_signal(const std::string& signal_str);

}  // namespace job_control_helpers

namespace job_utils {

struct ExitErrorResult {
    ErrorType type;
    std::string message;
    std::vector<std::string> suggestions;
};

ExitErrorResult make_exit_error_result(const std::string& command, int exit_code,
                                       const std::string& success_message,
                                       const std::string& failure_prefix);

bool command_consumes_terminal_stdin(const Command& cmd);
bool pipeline_consumes_terminal_stdin(const std::vector<Command>& commands);

}  // namespace job_utils

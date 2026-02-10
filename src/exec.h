/*
  exec.h

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

#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <sys/types.h>
#include <termios.h>

#include "error_out.h"

struct Command;

struct Job {
    pid_t pgid{0};
    std::string command;
    bool background{false};
    bool auto_background_on_stop{false};
    bool completed{false};
    bool stopped{false};
    int status{0};
    std::vector<pid_t> pids;
    pid_t last_pid{-1};
    int last_status{0};
    std::vector<pid_t> pid_order;
    std::vector<int> pipeline_statuses;
};

class Exec {
   private:
    std::mutex error_mutex;
    std::mutex jobs_mutex;
    std::map<int, Job> jobs;
    int next_job_id = 1;
    pid_t shell_pgid;
    struct termios shell_tmodes;
    int shell_terminal;
    bool owns_shell_terminal = false;
    bool shell_is_interactive;
    int last_exit_code = 0;
    std::vector<int> last_pipeline_statuses;
    ErrorInfo last_error;

    bool handle_empty_args(const std::vector<std::string>& args);
    bool initialize_env_assignments(const std::vector<std::string>& args,
                                    std::vector<std::pair<std::string, std::string>>& assignments,
                                    size_t& cmd_start_idx);
    std::optional<int> handle_assignments_prefix(
        const std::vector<std::string>& args,
        std::vector<std::pair<std::string, std::string>>& assignments, size_t& cmd_start_idx,
        const std::function<void()>& on_assignments_only);
    bool requires_fork(const Command& cmd) const;
    bool can_execute_in_process(const Command& cmd) const;
    int execute_builtin_with_redirections(Command cmd);
    void warn_parent_setpgid_failure();

    Job* find_job_locked(int job_id);
    void resume_job(Job& job, bool cont, std::string_view context);
    void report_missing_job(int job_id);

    void set_last_pipeline_statuses(std::vector<int> statuses);

   public:
    Exec();
    ~Exec();

    int execute_command_sync(const std::vector<std::string>& args,
                             bool auto_background_on_stop = false);
    int execute_command_async(const std::vector<std::string>& args);
    int execute_pipeline(const std::vector<Command>& commands);
    int run_with_command_redirections(Command cmd, const std::function<int()>& action,
                                      const std::string& command_name, bool persist_fd_changes,
                                      bool* action_invoked = nullptr);

    int add_job(const Job& job);
    void remove_job(int job_id);
    void put_job_in_foreground(int job_id, bool cont);
    void put_job_in_background(int job_id, bool cont);
    void wait_for_job(int job_id);
    void handle_child_signal(pid_t pid, int status);
    std::map<int, Job> get_jobs();
    void terminate_all_child_process();
    void abandon_all_child_processes();

    void set_error(const ErrorInfo& error);
    void set_error(ErrorType type, const std::string& command = "", const std::string& message = "",
                   const std::vector<std::string>& suggestions = {});
    ErrorInfo get_error();
    void print_last_error();
    void print_error_if_needed(int exit_code);
    int get_exit_code() const;
    const std::vector<int>& get_last_pipeline_statuses() const;
};

namespace exec_utils {

struct CommandOutput {
    std::string output;
    int exit_code;
    bool success;
};

CommandOutput execute_command_for_output(const std::string& command);

CommandOutput execute_command_vector_for_output(const std::vector<std::string>& args);

}  // namespace exec_utils

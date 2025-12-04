#pragma once

#include <sys/types.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "error_out.h"
#include "parser/parser.h"

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

    JobControlJob(int id, pid_t group_id, const std::vector<pid_t>& process_ids,
                  const std::string& cmd, bool is_background, bool consumes_stdin);
};

class JobManager {
   public:
    static JobManager& instance();

    int add_job(pid_t pgid, const std::vector<pid_t>& pids, const std::string& command,
                bool background = false, bool reads_stdin = true);

    void remove_job(int job_id);

    std::shared_ptr<JobControlJob> get_job(int job_id);

    std::shared_ptr<JobControlJob> get_job_by_pgid(pid_t pgid);

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

    void notify_job_stopped(const std::shared_ptr<JobControlJob>& job);

    bool foreground_job_reads_stdin();

    void mark_job_reads_stdin(pid_t pid, bool reads_stdin = true);

    void record_stdin_signal(pid_t pid, int signal_number);

    void clear_stdin_signal(pid_t pid);

    void handle_shell_continued();
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

int jobs_command(const std::vector<std::string>& args);
int fg_command(const std::vector<std::string>& args);
int bg_command(const std::vector<std::string>& args);
int wait_command(const std::vector<std::string>& args);
int kill_command(const std::vector<std::string>& args);
int disown_command(const std::vector<std::string>& args);

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

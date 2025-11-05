#pragma once

#include <map>
#include <mutex>
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
    bool shell_is_interactive;
    int last_exit_code = 0;
    std::vector<int> last_pipeline_statuses;
    ErrorInfo last_error;

    bool requires_fork(const Command& cmd) const;
    bool can_execute_in_process(const Command& cmd) const;
    int execute_builtin_with_redirections(Command cmd);
    bool handle_empty_args(const std::vector<std::string>& args);
    void report_missing_job(int job_id);
    bool initialize_env_assignments(const std::vector<std::string>& args,
                                    std::vector<std::pair<std::string, std::string>>& assignments,
                                    size_t& cmd_start_idx);
    void resume_job(Job& job, bool cont, std::string_view context);
    void set_last_pipeline_statuses(std::vector<int> statuses);

   public:
    Exec();
    ~Exec();

    int execute_command_sync(const std::vector<std::string>& args);
    int execute_command_async(const std::vector<std::string>& args);
    int execute_pipeline(const std::vector<Command>& commands);
    int add_job(const Job& job);
    void remove_job(int job_id);
    void update_job_status(int job_id, bool completed, bool stopped, int status);
    void put_job_in_foreground(int job_id, bool cont);
    void put_job_in_background(int job_id, bool cont);
    void wait_for_job(int job_id);
    std::map<int, Job> get_jobs();
    void init_shell();
    void handle_child_signal(pid_t pid, int status);
    void set_error(const ErrorInfo& error);
    void set_error(ErrorType type, const std::string& command = "", const std::string& message = "",
                   const std::vector<std::string>& suggestions = {});
    ErrorInfo get_error();
    std::string get_error_string();
    void print_last_error();
    int get_exit_code() const;
    void set_exit_code(int code);
    const std::vector<int>& get_last_pipeline_statuses() const;
    void terminate_all_child_process();

    std::string last_terminal_output_error;
};

namespace exec_utils {

struct CommandOutput {
    std::string output;
    int exit_code;
    bool success;
};

CommandOutput execute_command_for_output(const std::string& command);

CommandOutput execute_command_vector_for_output(const std::vector<std::string>& args);

std::string execute_command_for_output_trimmed(const std::string& command);

}  // namespace exec_utils
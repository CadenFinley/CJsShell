#pragma once

#include <sys/types.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Shell;

// Job states
enum class JobState {
  RUNNING,
  STOPPED,
  DONE,
  TERMINATED
};

// Job information for job control
struct JobControlJob {
  int job_id;
  pid_t pgid;
  std::vector<pid_t> pids;
  std::string command;
  JobState state;
  int exit_status;
  bool notified;

  JobControlJob(int id, pid_t group_id, const std::vector<pid_t>& process_ids,
                const std::string& cmd)
      : job_id(id),
        pgid(group_id),
        pids(process_ids),
        command(cmd),
        state(JobState::RUNNING),
        exit_status(0),
        notified(false) {
  }
};

// Job control manager
class JobManager {
 public:
  static JobManager& instance();

  // Add a new job
  int add_job(pid_t pgid, const std::vector<pid_t>& pids,
              const std::string& command);

  // Remove a job
  void remove_job(int job_id);

  // Get job by ID
  std::shared_ptr<JobControlJob> get_job(int job_id);

  // Get job by process group ID
  std::shared_ptr<JobControlJob> get_job_by_pgid(pid_t pgid);

  // Get all jobs
  std::vector<std::shared_ptr<JobControlJob>> get_all_jobs();

  // Update job status
  void update_job_status();

  // Mark job as current
  void set_current_job(int job_id);

  // Get current job
  int get_current_job() const {
    return current_job;
  }

  // Get previous job
  int get_previous_job() const {
    return previous_job;
  }

  // Set last background PID
  void set_last_background_pid(pid_t pid) {
    last_background_pid = pid;
  }

  // Get last background PID
  pid_t get_last_background_pid() const {
    return last_background_pid;
  }

  // Cleanup finished jobs
  void cleanup_finished_jobs();

  // Set shell reference
  void set_shell(Shell* shell) {
    shell_ref = shell;
  }

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

// Builtin commands
int jobs_command(const std::vector<std::string>& args);
int fg_command(const std::vector<std::string>& args);
int bg_command(const std::vector<std::string>& args);
int wait_command(const std::vector<std::string>& args);
int kill_command(const std::vector<std::string>& args);

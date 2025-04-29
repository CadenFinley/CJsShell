#pragma once
#include "parser.h"
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <vector>
#include <signal.h>
#include <map>
#include <termios.h>
#include "cjsh_filesystem.h"
#include "script_interpreter.h"
#include <memory>

struct Job {
  pid_t pgid;
  std::string command;
  bool background;
  bool completed;
  bool stopped;
  int status;
  std::vector<pid_t> pids;
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
  std::unique_ptr<ScriptInterpreter> interpreter;

public:
  Exec();
  ~Exec();

  void execute_command_sync(const std::vector<std::string>& args);
  void execute_command_async(const std::vector<std::string>& args);
  void execute_pipeline(const std::vector<Command>& commands);
  int add_job(const Job& job);
  void remove_job(int job_id);
  void update_job_status(int job_id, bool completed, bool stopped, int status);
  void put_job_in_foreground(int job_id, bool cont);
  void put_job_in_background(int job_id, bool cont);
  void wait_for_job(int job_id);
  std::map<int, Job> get_jobs();
  void init_shell();
  void handle_child_signal(pid_t pid, int status);
  void set_error(const std::string& error);
  std::string get_error();
  void terminate_all_child_process();
  
  // Helper method to check if a file is a shell script
  bool is_shell_script(const std::string& filepath);
  
  std::string last_terminal_output_error;
};
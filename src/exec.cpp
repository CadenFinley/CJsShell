#include "exec.h"
#include <vector>
#include <iostream>
#include <memory>
#include <csignal>
#include <algorithm>

Exec::Exec(){
  last_terminal_output_error = "";
  shell_terminal = STDIN_FILENO;
  shell_is_interactive = isatty(shell_terminal);
  
  // Initialize shell_pgid but don't try to take control of the terminal here
  shell_pgid = getpid();
  
  // Save default terminal modes
  if (shell_is_interactive) {
    if (tcgetattr(shell_terminal, &shell_tmodes) < 0) {
      perror("tcgetattr failed in Exec constructor");
    }
  }
}

Exec::~Exec() {
  // Make sure all child processes are terminated
  for (auto& job_pair : jobs) {
    Job& job = job_pair.second;
    if (!job.completed) {
      for (pid_t pid : job.pids) {
        kill(pid, SIGTERM);
      }
    }
  }
}

void Exec::init_shell() {
  // Initialize shell terminal settings
  if (shell_is_interactive) {
    // Put ourselves in our own process group
    shell_pgid = getpid();
    if (setpgid(shell_pgid, shell_pgid) < 0) {
      // Only fail if it's not an "Operation not permitted" error
      // (which happens when the process is already a process group leader)
      if (errno != EPERM) {
        perror("setpgid failed");
        return;
      }
    }

    // Grab control of the terminal - but handle errors gracefully
    if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
      // This could fail if the shell is running in the background
      perror("tcsetpgrp failed");
      // Continue anyway - the shell can still run without job control
    }

    // Save default terminal attributes
    if (tcgetattr(shell_terminal, &shell_tmodes) < 0) {
      perror("tcgetattr failed");
      // Continue anyway - the shell can still run without saved terminal attributes
    }
  }
}

// New method to handle child process signals
void Exec::handle_child_signal(pid_t pid, int status) {
  std::lock_guard<std::mutex> lock(jobs_mutex);
  
  // Find the job that this process belongs to
  for (auto& job_pair : jobs) {
    int job_id = job_pair.first;
    Job& job = job_pair.second;
    
    // If this process is part of the job
    auto it = std::find(job.pids.begin(), job.pids.end(), pid);
    if (it != job.pids.end()) {
      // Update the job status if the process was stopped or terminated
      if (WIFSTOPPED(status)) {
        job.stopped = true;
        job.status = status;
      } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
        // Remove the process from the job's process list
        job.pids.erase(it);
        
        // If all processes in the job have completed
        if (job.pids.empty()) {
          job.completed = true;
          job.stopped = false;
          job.status = status;
          
          // Notify the user of job completion
          if (job.background) {
            std::cout << "\n[" << job_id << "] Done\t" << job.command << std::endl;
          }
        }
      }
      break;
    }
  }
}

// Thread-safe method to set error message
void Exec::set_error(const std::string& error) {
  std::lock_guard<std::mutex> lock(error_mutex);
  last_terminal_output_error = error;
}

// Thread-safe method to get error message
std::string Exec::get_error() {
  std::lock_guard<std::mutex> lock(error_mutex);
  return last_terminal_output_error;
}

void Exec::execute_command_sync(const std::vector<std::string>& args) {
  if (args.empty()) {
    set_error("cjsh: Failed to parse command");
    std::cerr << last_terminal_output_error << std::endl;
    return;
  }
  
  pid_t pid = fork();
  
  if (pid == -1) {
    set_error("cjsh: Failed to fork process: " + std::string(strerror(errno)));
    std::cerr << last_terminal_output_error << std::endl;
    return;
  }
  
  if (pid == 0) { // Child process
    // Put the child in its own process group
    pid_t child_pid = getpid();
    if (setpgid(child_pid, child_pid) < 0) {
      perror("setpgid failed in child");
      _exit(EXIT_FAILURE);
    }
    
    // If shell is interactive, give terminal control to the child
    if (shell_is_interactive) {
      if (tcsetpgrp(shell_terminal, child_pid) < 0) {
        perror("tcsetpgrp failed in child");
      }
    }
    
    // Reset signal handlers to default
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    
    // Unblock signals that might be blocked
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGTSTP);
    sigaddset(&set, SIGCHLD);
    sigprocmask(SIG_UNBLOCK, &set, nullptr);
    
    std::vector<char*> c_args;
    for (auto& arg : args) {
      c_args.push_back(const_cast<char*>(arg.data()));
    }
    c_args.push_back(nullptr);
    
    execvp(args[0].c_str(), c_args.data());
    
    std::string error_msg = "cjsh: Failed to execute command( " + args[0] + " ) : " + std::string(strerror(errno));
    std::cerr << error_msg << std::endl;
    _exit(EXIT_FAILURE);
  }
  
  // Parent process
  
  // Put the child in its own process group
  if (setpgid(pid, pid) < 0) {
    // This can happen if the child has already exited
    if (errno != EACCES && errno != ESRCH) {
      perror("setpgid failed in parent");
    }
  }
  
  // Create a job for this process
  Job job;
  job.pgid = pid;
  job.command = args[0];
  job.background = false;
  job.completed = false;
  job.stopped = false;
  job.pids.push_back(pid);
  
  int job_id = add_job(job);
  
  // Put job in foreground
  put_job_in_foreground(job_id, false);
  
  // For sync commands, we can set a success message
  set_error("command completed successfully");
}

void Exec::execute_command_async(const std::vector<std::string>& args) {
  if (args.empty()) {
    set_error("cjsh: Failed to parse command");
    std::cerr << last_terminal_output_error << std::endl;
    return;
  }
  
  pid_t pid = fork();
  
  if (pid == -1) {
    set_error("cjsh: Failed to fork process: " + std::string(strerror(errno)));
    std::cerr << last_terminal_output_error << std::endl;
    return;
  }
  
  if (pid == 0) { // Child process
    // Create a new session and process group
    if (setsid() == -1) {
      perror("cjsh (async): setsid");
      _exit(EXIT_FAILURE);
    }

    // Redirect standard file descriptors to /dev/null
    int dev_null = open("/dev/null", O_RDWR);
    if (dev_null != -1) {
      if (dup2(dev_null, STDIN_FILENO) == -1 ||
          dup2(dev_null, STDOUT_FILENO) == -1 ||
          dup2(dev_null, STDERR_FILENO) == -1) {
        perror("cjsh (async): dup2");
        _exit(EXIT_FAILURE);
      }
      if (dev_null > 2) {
        close(dev_null);
      }
    }
    
    // Reset signal handlers to default
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    
    std::vector<char*> c_args;
    for (auto& arg : args) {
      c_args.push_back(const_cast<char*>(arg.data()));
    }
    c_args.push_back(nullptr);
    
    execvp(args[0].c_str(), c_args.data());
    
    _exit(EXIT_FAILURE);
  }
  else { // Parent process
    // Create a job for this process
    Job job;
    job.pgid = pid;
    job.command = args[0];
    job.background = true;
    job.completed = false;
    job.stopped = false;
    job.pids.push_back(pid);
    
    int job_id = add_job(job);
    
    // For async commands, indicate they've been launched successfully
    set_error("async command launched");
    
    std::cout << "[" << job_id << "] " << pid << std::endl;
  }
}

void Exec::execute_pipeline(const std::vector<Command>& commands) {
  if (commands.empty()) {
    set_error("cjsh: Empty pipeline");
    return;
  }
  
  // If there's only one command, no need for pipes
  if (commands.size() == 1) {
    const Command& cmd = commands[0];
    
    // Check if the command should run in the background
    if (cmd.background) {
      execute_command_async(cmd.args);
    } else {
      // Handle I/O redirections
      pid_t pid = fork();
      
      if (pid == -1) {
        set_error("cjsh: Failed to fork process: " + std::string(strerror(errno)));
        std::cerr << last_terminal_output_error << std::endl;
        return;
      }
      
      if (pid == 0) { // Child process
        // Put the child in its own process group
        pid_t child_pid = getpid();
        if (setpgid(child_pid, child_pid) < 0) {
          perror("setpgid failed in child");
          _exit(EXIT_FAILURE);
        }
        
        // Set up input redirection
        if (!cmd.input_file.empty()) {
          int fd = open(cmd.input_file.c_str(), O_RDONLY);
          if (fd == -1) {
            std::cerr << "cjsh: " << cmd.input_file << ": " << strerror(errno) << std::endl;
            _exit(EXIT_FAILURE);
          }
          dup2(fd, STDIN_FILENO);
          close(fd);
        }
        
        // Set up output redirection
        if (!cmd.output_file.empty()) {
          int fd = open(cmd.output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
          if (fd == -1) {
            std::cerr << "cjsh: " << cmd.output_file << ": " << strerror(errno) << std::endl;
            _exit(EXIT_FAILURE);
          }
          dup2(fd, STDOUT_FILENO);
          close(fd);
        }
        
        // Set up append redirection
        if (!cmd.append_file.empty()) {
          int fd = open(cmd.append_file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
          if (fd == -1) {
            std::cerr << "cjsh: " << cmd.append_file << ": " << strerror(errno) << std::endl;
            _exit(EXIT_FAILURE);
          }
          dup2(fd, STDOUT_FILENO);
          close(fd);
        }
        
        // Execute the command
        std::vector<char*> c_args;
        for (auto& arg : cmd.args) {
          c_args.push_back(const_cast<char*>(arg.data()));
        }
        c_args.push_back(nullptr);
        
        execvp(cmd.args[0].c_str(), c_args.data());
        
        std::cerr << "cjsh: " << cmd.args[0] << ": " << strerror(errno) << std::endl;
        _exit(EXIT_FAILURE);
      }
      
      // Parent process
      
      // Create a job for this process
      Job job;
      job.pgid = pid;
      job.command = cmd.args[0];
      job.background = false;
      job.completed = false;
      job.stopped = false;
      job.pids.push_back(pid);
      
      int job_id = add_job(job);
      
      // Put job in foreground
      put_job_in_foreground(job_id, false);
    }
    
    return;
  }
  
  // For multiple commands, set up pipes
  std::vector<pid_t> pids;
  pid_t pgid = 0;
  
  int pipes[2];
  int in_fd = STDIN_FILENO;
  
  for (size_t i = 0; i < commands.size(); i++) {
    const Command& cmd = commands[i];
    
    // Create a pipe for all but the last command
    if (i < commands.size() - 1) {
      if (pipe(pipes) == -1) {
        set_error("cjsh: Failed to create pipe: " + std::string(strerror(errno)));
        std::cerr << last_terminal_output_error << std::endl;
        
        // Kill any already created child processes
        for (pid_t pid : pids) {
          kill(pid, SIGTERM);
        }
        
        return;
      }
    }
    
    pid_t pid = fork();
    
    if (pid == -1) {
      set_error("cjsh: Failed to fork process: " + std::string(strerror(errno)));
      std::cerr << last_terminal_output_error << std::endl;
      
      // Close pipes
      if (i < commands.size() - 1) {
        close(pipes[0]);
        close(pipes[1]);
      }
      
      // Kill any already created child processes
      for (pid_t pid : pids) {
        kill(pid, SIGTERM);
      }
      
      return;
    }
    
    if (pid == 0) { // Child process
      // Put all processes in the pipeline in the same process group
      if (pgid == 0) {
        pgid = getpid();
      }
      
      if (setpgid(0, pgid) < 0) {
        perror("setpgid failed in child");
        _exit(EXIT_FAILURE);
      }
      
      // If the first command, set input redirection
      if (i == 0 && !cmd.input_file.empty()) {
        int fd = open(cmd.input_file.c_str(), O_RDONLY);
        if (fd == -1) {
          std::cerr << "cjsh: " << cmd.input_file << ": " << strerror(errno) << std::endl;
          _exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
      } else if (in_fd != STDIN_FILENO) {
        // Connect input to the previous command's output
        dup2(in_fd, STDIN_FILENO);
        close(in_fd);
      }
      
      // If the last command, set output redirection
      if (i == commands.size() - 1) {
        if (!cmd.output_file.empty()) {
          int fd = open(cmd.output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
          if (fd == -1) {
            std::cerr << "cjsh: " << cmd.output_file << ": " << strerror(errno) << std::endl;
            _exit(EXIT_FAILURE);
          }
          dup2(fd, STDOUT_FILENO);
          close(fd);
        } else if (!cmd.append_file.empty()) {
          int fd = open(cmd.append_file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
          if (fd == -1) {
            std::cerr << "cjsh: " << cmd.append_file << ": " << strerror(errno) << std::endl;
            _exit(EXIT_FAILURE);
          }
          dup2(fd, STDOUT_FILENO);
          close(fd);
        }
      } else {
        // Connect output to the next command's input
        dup2(pipes[1], STDOUT_FILENO);
        close(pipes[0]);
        close(pipes[1]);
      }
      
      // Execute the command
      std::vector<char*> c_args;
      for (auto& arg : cmd.args) {
        c_args.push_back(const_cast<char*>(arg.data()));
      }
      c_args.push_back(nullptr);
      
      execvp(cmd.args[0].c_str(), c_args.data());
      
      std::cerr << "cjsh: " << cmd.args[0] << ": " << strerror(errno) << std::endl;
      _exit(EXIT_FAILURE);
    }
    
    // Parent process
    
    // Set process group for the first process
    if (pgid == 0) {
      pgid = pid;
    }
    
    if (setpgid(pid, pgid) < 0) {
      // This can happen if the child has already exited
      if (errno != EACCES) {
        perror("setpgid failed in parent");
      }
    }
    
    pids.push_back(pid);
    
    // Close previously used input fd if it was a pipe
    if (in_fd != STDIN_FILENO) {
      close(in_fd);
    }
    
    // Close write end of the current pipe
    if (i < commands.size() - 1) {
      close(pipes[1]);
      in_fd = pipes[0];
    }
  }
  
  // Create a job for this pipeline
  Job job;
  job.pgid = pgid;
  job.command = commands[0].args[0] + " | ...";
  job.background = commands.back().background;
  job.completed = false;
  job.stopped = false;
  job.pids = pids;
  
  int job_id = add_job(job);
  
  // Put job in foreground or background
  if (job.background) {
    put_job_in_background(job_id, false);
    std::cout << "[" << job_id << "] " << pgid << std::endl;
  } else {
    put_job_in_foreground(job_id, false);
  }
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

void Exec::update_job_status(int job_id, bool completed, bool stopped, int status) {
  std::lock_guard<std::mutex> lock(jobs_mutex);
  
  auto it = jobs.find(job_id);
  if (it != jobs.end()) {
    it->second.completed = completed;
    it->second.stopped = stopped;
    it->second.status = status;
  }
}

void Exec::put_job_in_foreground(int job_id, bool cont) {
  std::lock_guard<std::mutex> lock(jobs_mutex);
  
  auto it = jobs.find(job_id);
  if (it == jobs.end()) {
    std::cerr << "cjsh: No such job " << job_id << std::endl;
    return;
  }
  
  Job& job = it->second;
  
  // Put the job in the foreground only if shell is interactive
  if (shell_is_interactive) {
    // Check if shell_terminal is actually a terminal before using tcsetpgrp
    if (isatty(shell_terminal)) {
      // This is crucial for Ctrl+C to work - give terminal control to the job
      if (tcsetpgrp(shell_terminal, job.pgid) < 0) {
        // Only print error if it's not an expected terminal-related error
        if (errno != ENOTTY && errno != EINVAL) {
          perror("tcsetpgrp (job to fg)");
        }
      }
    }
  }
  
  // Send the job a continue signal if necessary
  if (cont && job.stopped) {
    if (kill(-job.pgid, SIGCONT) < 0) {
      perror("kill (SIGCONT)");
    }
    job.stopped = false;
  }
  
  // Temporarily release the mutex while waiting for the job
  jobs_mutex.unlock();
  
  // Wait for the job to complete or stop
  wait_for_job(job_id);
  
  // Re-acquire the mutex
  jobs_mutex.lock();
  
  // Put the shell back in the foreground
  if (shell_is_interactive) {
    // Only attempt to manipulate terminal if it's actually a terminal
    if (isatty(shell_terminal)) {
      // Take back terminal control - critical for proper shell operation after job completes
      if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
        // Only report error if it's not the expected "not a terminal" errors
        if (errno != ENOTTY && errno != EINVAL) {
          perror("tcsetpgrp (shell to fg)");
        }
      }
      
      // Restore the shell's terminal modes - only if we have a valid terminal
      if (tcgetattr(shell_terminal, &shell_tmodes) == 0) {
        if (tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes) < 0) {
          perror("tcsetattr");
        }
      }
    }
  }
}

void Exec::put_job_in_background(int job_id, bool cont) {
  std::lock_guard<std::mutex> lock(jobs_mutex);
  
  auto it = jobs.find(job_id);
  if (it == jobs.end()) {
    std::cerr << "cjsh: No such job " << job_id << std::endl;
    return;
  }
  
  Job& job = it->second;
  
  // Send the job a continue signal if necessary
  if (cont && job.stopped) {
    if (kill(-job.pgid, SIGCONT) < 0) {
      perror("kill (SIGCONT)");
    }
    
    job.stopped = false;
  }
}

void Exec::wait_for_job(int job_id) {
  std::unique_lock<std::mutex> lock(jobs_mutex);
  
  auto it = jobs.find(job_id);
  if (it == jobs.end()) {
    return;
  }
  
  // Create safe copies of needed job data
  pid_t job_pgid = it->second.pgid;
  std::vector<pid_t> remaining_pids = it->second.pids;
  
  // Release the mutex while waiting
  lock.unlock();
  
  int status = 0;
  pid_t pid;
  
  // Wait until all processes in the job have completed or the job is stopped
  bool job_stopped = false;
  
  while (!remaining_pids.empty()) {
    pid = waitpid(-job_pgid, &status, WUNTRACED);
    
    if (pid == -1) {
      // Error in waitpid
      if (errno == EINTR) {
        // Interrupted by signal, try again
        continue;
      } else if (errno == ECHILD) {
        // No child processes - they may have been reaped by SIGCHLD handler
        // Just mark all remaining processes as completed
        remaining_pids.clear();
        break;
      } else {
        // Other error, break out of the loop
        perror("waitpid");
        break;
      }
    }
    
    // Process exited, remove from our tracking list
    auto pid_it = std::find(remaining_pids.begin(), remaining_pids.end(), pid);
    if (pid_it != remaining_pids.end()) {
      remaining_pids.erase(pid_it);
    }
    
    // If the process was stopped, the job is stopped
    if (WIFSTOPPED(status)) {
      job_stopped = true;
      break;
    }
  }
  
  // Re-acquire the mutex before updating job status
  lock.lock();
  
  // Check if the job still exists in our map
  it = jobs.find(job_id);
  if (it != jobs.end()) {
    Job& job = it->second;
    
    // Update job status
    if (job_stopped) {
      job.stopped = true;
      job.status = status;
    } else {
      job.completed = true;
      job.stopped = false;
      job.status = status;
      
      // Update the error message based on exit status
      if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        if (exit_status == 0) {
          set_error("command completed successfully");
        } else {
          set_error("command failed with exit code " + std::to_string(exit_status));
        }
      } else if (WIFSIGNALED(status)) {
        set_error("command terminated by signal " + std::to_string(WTERMSIG(status)));
      }
    }
  }
}

std::map<int, Job> Exec::get_jobs() {
  std::lock_guard<std::mutex> lock(jobs_mutex);
  return jobs;
}
#include "exec.h"
#include <vector>
#include <iostream>
#include <memory>
#include <csignal>
#include <algorithm>

// Global pointer to the Exec instance for signal handlers
static Exec* g_exec_instance = nullptr;

Exec::Exec(){
  last_terminal_output_error = "";
  shell_terminal = STDIN_FILENO;
  shell_is_interactive = isatty(shell_terminal);
  
  if (shell_is_interactive) {
    // Put ourselves in our own process group
    shell_pgid = getpid();
    if (setpgid(shell_pgid, shell_pgid) < 0) {
      perror("setpgid failed");
      exit(1);
    }

    // Grab control of the terminal
    tcsetpgrp(shell_terminal, shell_pgid);

    // Save default terminal attributes
    tcgetattr(shell_terminal, &shell_tmodes);
  }
  
  g_exec_instance = this;
  setup_signal_handlers();
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
      perror("setpgid failed");
      exit(1);
    }

    // Grab control of the terminal
    tcsetpgrp(shell_terminal, shell_pgid);

    // Save default terminal attributes
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

void Exec::setup_signal_handlers() {
  // Setup signal handlers
  struct sigaction sa;
  
  // SIGCHLD handler
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sigaction(SIGCHLD, &sa, nullptr);
  
  // SIGINT handler
  sa.sa_handler = sigint_handler;
  sigaction(SIGINT, &sa, nullptr);
  
  // SIGTSTP handler
  sa.sa_handler = sigtstp_handler;
  sigaction(SIGTSTP, &sa, nullptr);
  
  // SIGCONT handler
  sa.sa_handler = sigcont_handler;
  sigaction(SIGCONT, &sa, nullptr);
  
  // Ignore SIGTTIN and SIGTTOU
  sa.sa_handler = SIG_IGN;
  sigaction(SIGTTIN, &sa, nullptr);
  sigaction(SIGTTOU, &sa, nullptr);
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
      tcsetpgrp(shell_terminal, child_pid);
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
    
    std::string error_msg = "cjsh: Failed to execute command: " + std::string(strerror(errno));
    std::cerr << error_msg << std::endl;
    _exit(EXIT_FAILURE);
  }
  
  // Parent process
  
  // Put the child in its own process group
  if (setpgid(pid, pid) < 0) {
    // This can happen if the child has already exited
    if (errno != EACCES) {
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
  
  // Put the job in the foreground
  tcsetpgrp(shell_terminal, job.pgid);
  
  // Send the job a continue signal if necessary
  if (cont && job.stopped) {
    if (kill(-job.pgid, SIGCONT) < 0) {
      perror("kill (SIGCONT)");
    }
  }
  
  // Wait for the job to complete or stop
  wait_for_job(job_id);
  
  // Put the shell back in the foreground
  tcsetpgrp(shell_terminal, shell_pgid);
  
  // Restore the shell's terminal modes
  tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes);
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
  int status;
  pid_t pid;
  
  do {
    pid = waitpid(-1, &status, WUNTRACED);
    
    // Find the job that this process belongs to
    for (auto& job_pair : jobs) {
      Job& job = job_pair.second;
      
      // If this process is part of the job
      auto it = std::find(job.pids.begin(), job.pids.end(), pid);
      if (it != job.pids.end()) {
        // Remove the process from the job's process list
        job.pids.erase(it);
        
        // If all processes in the job have completed
        if (job.pids.empty()) {
          if (WIFSTOPPED(status)) {
            job.stopped = true;
            job.completed = false;
          } else {
            job.completed = true;
            job.stopped = false;
          }
          
          job.status = status;
          
          // If this is the job we're waiting for, and it's completed or stopped
          if (job_pair.first == job_id && (job.completed || job.stopped)) {
            return;
          }
        }
        
        break;
      }
    }
  } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));
}

std::map<int, Job> Exec::get_jobs() {
  std::lock_guard<std::mutex> lock(jobs_mutex);
  return jobs;
}

// Signal handlers
void sigchld_handler(int sig) {
  (void)sig; // Mark parameter as deliberately unused
  
  // Check for terminated children
  pid_t pid;
  int status;
  
  while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
    if (g_exec_instance) {
      // Find the job that this process belongs to
      auto jobs = g_exec_instance->get_jobs();
      
      for (auto& job_pair : jobs) {
        int job_id = job_pair.first;
        Job& job = job_pair.second;
        
        // If this process is part of the job
        auto it = std::find(job.pids.begin(), job.pids.end(), pid);
        if (it != job.pids.end()) {
          // Update the job status if the process was stopped or terminated
          if (WIFSTOPPED(status)) {
            g_exec_instance->update_job_status(job_id, false, true, status);
          } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            // Only mark the job as completed if this was the last process
            job.pids.erase(it);
            if (job.pids.empty()) {
              g_exec_instance->update_job_status(job_id, true, false, status);
              
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
  }
}

void sigint_handler(int sig) {
  (void)sig; // Mark parameter as deliberately unused
  
  // Ignore SIGINT in the shell process
  // Child processes will inherit the default handler when they are exec'd
}

void sigtstp_handler(int sig) {
  (void)sig; // Mark parameter as deliberately unused
  
  // Ignore SIGTSTP in the shell process
}

void sigcont_handler(int sig) {
  (void)sig; // Mark parameter as deliberately unused
  
  // Handle SIGCONT if necessary
}
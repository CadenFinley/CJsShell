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
  
  shell_pgid = getpid();
  
  if (shell_is_interactive) {
    if (tcgetattr(shell_terminal, &shell_tmodes) < 0) {
      perror("tcgetattr failed in Exec constructor");
    }
  }
}

Exec::~Exec() {
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
  if (shell_is_interactive) {
    shell_pgid = getpid();
    if (setpgid(shell_pgid, shell_pgid) < 0) {
      if (errno != EPERM) {
        perror("setpgid failed");
        return;
      }
    }

    if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
      perror("tcsetpgrp failed");
    }

    if (tcgetattr(shell_terminal, &shell_tmodes) < 0) {
      perror("tcgetattr failed");
    }
  }
}

void Exec::handle_child_signal(pid_t pid, int status) {
  std::lock_guard<std::mutex> lock(jobs_mutex);
  
  for (auto& job_pair : jobs) {
    int job_id = job_pair.first;
    Job& job = job_pair.second;
    
    auto it = std::find(job.pids.begin(), job.pids.end(), pid);
    if (it != job.pids.end()) {
      if (WIFSTOPPED(status)) {
        job.stopped = true;
        job.status = status;
      } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
        job.pids.erase(it);
        
        if (job.pids.empty()) {
          job.completed = true;
          job.stopped = false;
          job.status = status;
          
          if (job.background) {
            std::cout << "\n[" << job_id << "] Done\t" << job.command << std::endl;
          }
        }
      }
      break;
    }
  }
}

void Exec::set_error(const std::string& error) {
  std::lock_guard<std::mutex> lock(error_mutex);
  last_terminal_output_error = error;
}

std::string Exec::get_error() {
  std::lock_guard<std::mutex> lock(error_mutex);
  return last_terminal_output_error;
}

int Exec::execute_command_sync(const std::vector<std::string>& args) {
  if (args.empty()) {
    set_error("cjsh: Failed to parse command");
    std::cerr << last_terminal_output_error << std::endl;
    last_exit_code = 1;
    return 1;
  }
  std::vector<std::pair<std::string, std::string>> env_assignments;
  size_t cmd_start_idx = 0;
  
  for (size_t i = 0; i < args.size(); i++) {
    std::string var_name, var_value;
    size_t pos = args[i].find('=');
    
    if (pos != std::string::npos && pos > 0) {
      std::string name = args[i].substr(0, pos);
      bool valid_name = true;

      if (!isalpha(name[0]) && name[0] != '_') {
        valid_name = false;
      } else {
        for (char c : name) {
          if (!isalnum(c) && c != '_') {
            valid_name = false;
            break;
          }
        }
      }
      
      if (valid_name) {
        var_name = name;
        var_value = args[i].substr(pos + 1);
        env_assignments.push_back({var_name, var_value});
        cmd_start_idx = i + 1;
      } else {
        break;
      }
    } else {
      break;
    }
  }

  if (cmd_start_idx >= args.size()) {
    for (const auto& env : env_assignments) {
      setenv(env.first.c_str(), env.second.c_str(), 1);
    }
    set_error("Environment variables set");
    last_exit_code = 0;
    return 0;
  }
  
  std::vector<std::string> cmd_args(args.begin() + cmd_start_idx, args.end());
  
  pid_t pid = fork();
  
  if (pid == -1) {
    set_error("cjsh: Failed to fork process: " + std::string(strerror(errno)));
    std::cerr << last_terminal_output_error << std::endl;
    last_exit_code = 1;
    return 1;
  }
  
  if (pid == 0) {
    for (const auto& env : env_assignments) {
      setenv(env.first.c_str(), env.second.c_str(), 1);
    }
    
    pid_t child_pid = getpid();
    if (setpgid(child_pid, child_pid) < 0) {
      perror("setpgid failed in child");
      _exit(EXIT_FAILURE);
    }
    
    if (shell_is_interactive) {
      if (tcsetpgrp(shell_terminal, child_pid) < 0) {
        perror("tcsetpgrp failed in child");
      }
    }
    
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGTSTP);
    sigaddset(&set, SIGCHLD);
    sigprocmask(SIG_UNBLOCK, &set, nullptr);
    
    std::vector<char*> c_args;
    for (auto& arg : cmd_args) {
      c_args.push_back(const_cast<char*>(arg.data()));
    }
    c_args.push_back(nullptr);
    
    execvp(cmd_args[0].c_str(), c_args.data());
    
    std::string error_msg = "cjsh: Failed to execute command ( " + cmd_args[0] + " ) : no command found";
    std::cerr << error_msg << std::endl;
    _exit(EXIT_FAILURE);
  }
  
  if (setpgid(pid, pid) < 0) {
    if (errno != EACCES && errno != ESRCH) {
      perror("setpgid failed in parent");
    }
  }
  
  Job job;
  job.pgid = pid;
  job.command = args[0];
  job.background = false;
  job.completed = false;
  job.stopped = false;
  job.pids.push_back(pid);
  
  int job_id = add_job(job);
  
  put_job_in_foreground(job_id, false);
  
  std::lock_guard<std::mutex> lock(jobs_mutex);
  auto it = jobs.find(job_id);
  int exit_code = 0;
  
  if (it != jobs.end() && it->second.completed) {
    int status = it->second.status;
    if (WIFEXITED(status)) {
      exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      exit_code = 128 + WTERMSIG(status);
    }
  }
  
  set_error(exit_code == 0 ? "command completed successfully" : "command failed with exit code " + std::to_string(exit_code));
  last_exit_code = exit_code;
  return exit_code;
}

int Exec::execute_command_async(const std::vector<std::string>& args) {
  if (args.empty()) {
    set_error("cjsh: Failed to parse command");
    std::cerr << last_terminal_output_error << std::endl;
    last_exit_code = 1;
    return 1;
  }
  
  pid_t pid = fork();
  
  if (pid == -1) {
    set_error("cjsh: Failed to fork process: " + std::string(strerror(errno)));
    std::cerr << last_terminal_output_error << std::endl;
    last_exit_code = 1;
    return 1;
  }
  
  if (pid == 0) {
    if (setsid() == -1) {
      perror("cjsh (async): setsid");
      _exit(EXIT_FAILURE);
    }

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
  else {
    Job job;
    job.pgid = pid;
    job.command = args[0];
    job.background = true;
    job.completed = false;
    job.stopped = false;
    job.pids.push_back(pid);
    
    int job_id = add_job(job);
    
    set_error("async command launched");
    
    std::cout << "[" << job_id << "] " << pid << std::endl;
    last_exit_code = 0;
    return 0;
  }
}

int Exec::execute_pipeline(const std::vector<Command>& commands) {
  if (commands.empty()) {
    set_error("cjsh: Empty pipeline");
    last_exit_code = 1;
    return 1;
  }
  
  if (commands.size() == 1) {
    const Command& cmd = commands[0];
    
    if (cmd.background) {
      execute_command_async(cmd.args);
    } else {
      pid_t pid = fork();
      
      if (pid == -1) {
        set_error("cjsh: Failed to fork process: " + std::string(strerror(errno)));
        std::cerr << last_terminal_output_error << std::endl;
        return 1;
      }
      
      if (pid == 0) {
        pid_t child_pid = getpid();
        if (setpgid(child_pid, child_pid) < 0) {
          perror("setpgid failed in child");
          _exit(EXIT_FAILURE);
        }
        
        if (!cmd.input_file.empty()) {
          int fd = open(cmd.input_file.c_str(), O_RDONLY);
          if (fd == -1) {
            std::cerr << "cjsh: " << cmd.input_file << ": " << strerror(errno) << std::endl;
            _exit(EXIT_FAILURE);
          }
          dup2(fd, STDIN_FILENO);
          close(fd);
        }
        
        if (!cmd.output_file.empty()) {
          int fd = open(cmd.output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
          if (fd == -1) {
            std::cerr << "cjsh: " << cmd.output_file << ": " << strerror(errno) << std::endl;
            _exit(EXIT_FAILURE);
          }
          dup2(fd, STDOUT_FILENO);
          close(fd);
        }
        
        if (!cmd.append_file.empty()) {
          int fd = open(cmd.append_file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
          if (fd == -1) {
            std::cerr << "cjsh: " << cmd.append_file << ": " << strerror(errno) << std::endl;
            _exit(EXIT_FAILURE);
          }
          dup2(fd, STDOUT_FILENO);
          close(fd);
        }
        
        std::vector<char*> c_args;
        for (auto& arg : cmd.args) {
          c_args.push_back(const_cast<char*>(arg.data()));
        }
        c_args.push_back(nullptr);
        
        execvp(cmd.args[0].c_str(), c_args.data());
        
        std::cerr << "cjsh: " << cmd.args[0] << ": " << strerror(errno) << std::endl;
        _exit(EXIT_FAILURE);
      }
      
      Job job;
      job.pgid = pid;
      job.command = cmd.args[0];
      job.background = false;
      job.completed = false;
      job.stopped = false;
      job.pids.push_back(pid);
      
      int job_id = add_job(job);
      
      put_job_in_foreground(job_id, false);
    }
    
    return 0;
  }
  
  std::vector<pid_t> pids;
  pid_t pgid = 0;
  
  int pipes[2];
  int in_fd = STDIN_FILENO;
  
  for (size_t i = 0; i < commands.size(); i++) {
    const Command& cmd = commands[i];
    
    if (i < commands.size() - 1) {
      if (pipe(pipes) == -1) {
        set_error("cjsh: Failed to create pipe: " + std::string(strerror(errno)));
        std::cerr << last_terminal_output_error << std::endl;
        
        for (pid_t pid : pids) {
          kill(pid, SIGTERM);
        }
        
        return 1;
      }
    }
    
    pid_t pid = fork();
    
    if (pid == -1) {
      set_error("cjsh: Failed to fork process: " + std::string(strerror(errno)));
      std::cerr << last_terminal_output_error << std::endl;
      
      if (i < commands.size() - 1) {
        close(pipes[0]);
        close(pipes[1]);
      }
      
      for (pid_t pid : pids) {
        kill(pid, SIGTERM);
      }
      
      return 1;
    }
    
    if (pid == 0) {
      if (pgid == 0) {
        pgid = getpid();
      }
      
      if (setpgid(0, pgid) < 0) {
        perror("setpgid failed in child");
        _exit(EXIT_FAILURE);
      }
      
      if (i == 0 && !cmd.input_file.empty()) {
        int fd = open(cmd.input_file.c_str(), O_RDONLY);
        if (fd == -1) {
          std::cerr << "cjsh: " << cmd.input_file << ": " << strerror(errno) << std::endl;
          _exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
      } else if (in_fd != STDIN_FILENO) {
        dup2(in_fd, STDIN_FILENO);
        close(in_fd);
      }
      
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
        dup2(pipes[1], STDOUT_FILENO);
        close(pipes[0]);
        close(pipes[1]);
      }
      
      std::vector<char*> c_args;
      for (auto& arg : cmd.args) {
        c_args.push_back(const_cast<char*>(arg.data()));
      }
      c_args.push_back(nullptr);
      
      execvp(cmd.args[0].c_str(), c_args.data());
      
      std::cerr << "cjsh: " << cmd.args[0] << ": " << strerror(errno) << std::endl;
      _exit(EXIT_FAILURE);
    }
    
    if (pgid == 0) {
      pgid = pid;
    }
    
    if (setpgid(pid, pgid) < 0) {
      if (errno != EACCES) {
        perror("setpgid failed in parent");
      }
    }
    
    pids.push_back(pid);
    
    if (in_fd != STDIN_FILENO) {
      close(in_fd);
    }
    
    if (i < commands.size() - 1) {
      close(pipes[1]);
      in_fd = pipes[0];
    }
  }
  
  Job job;
  job.pgid = pgid;
  job.command = commands[0].args[0] + " | ...";
  job.background = commands.back().background;
  job.completed = false;
  job.stopped = false;
  job.pids = pids;
  
  int job_id = add_job(job);
  
  if (job.background) {
    put_job_in_background(job_id, false);
    std::cout << "[" << job_id << "] " << pgid << std::endl;
  } else {
    put_job_in_foreground(job_id, false);
  }
  
  return 0;
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
  
  if (shell_is_interactive) {
    if (isatty(shell_terminal)) {
      if (tcsetpgrp(shell_terminal, job.pgid) < 0) {
        if (errno != ENOTTY && errno != EINVAL) {
          perror("tcsetpgrp (job to fg)");
        }
      }
    }
  }
  
  if (cont && job.stopped) {
    if (kill(-job.pgid, SIGCONT) < 0) {
      perror("kill (SIGCONT)");
    }
    job.stopped = false;
  }
  
  jobs_mutex.unlock();
  
  wait_for_job(job_id);
  
  jobs_mutex.lock();
  
  if (shell_is_interactive) {
    if (isatty(shell_terminal)) {
      if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
        if (errno != ENOTTY && errno != EINVAL) {
          perror("tcsetpgrp (shell to fg)");
        }
      }
      
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
  
  pid_t job_pgid = it->second.pgid;
  std::vector<pid_t> remaining_pids = it->second.pids;
  
  lock.unlock();
  
  int status = 0;
  pid_t pid;
  
  bool job_stopped = false;
  
  while (!remaining_pids.empty()) {
    pid = waitpid(-job_pgid, &status, WUNTRACED);
    
    if (pid == -1) {
      if (errno == EINTR) {
        continue;
      } else if (errno == ECHILD) {
        remaining_pids.clear();
        break;
      } else {
        perror("waitpid");
        break;
      }
    }
    
    auto pid_it = std::find(remaining_pids.begin(), remaining_pids.end(), pid);
    if (pid_it != remaining_pids.end()) {
      remaining_pids.erase(pid_it);
    }
    
    if (WIFSTOPPED(status)) {
      job_stopped = true;
      break;
    }
  }
  
  lock.lock();
  
  it = jobs.find(job_id);
  if (it != jobs.end()) {
    Job& job = it->second;
    
    if (job_stopped) {
      job.stopped = true;
      job.status = status;
      last_exit_code = 128 + SIGTSTP;
    } else {
      job.completed = true;
      job.stopped = false;
      job.status = status;
      
      if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        last_exit_code = exit_status;
        if (exit_status == 0) {
          set_error("command completed successfully");
        } else {
          set_error("command failed with exit code " + std::to_string(exit_status));
        }
      } else if (WIFSIGNALED(status)) {
        last_exit_code = 128 + WTERMSIG(status);
        set_error("command terminated by signal " + std::to_string(WTERMSIG(status)));
      }
    }
  }
}

void Exec::terminate_all_child_process() {
  std::lock_guard<std::mutex> lock(jobs_mutex);
  
  for (auto& job_pair : jobs) {
    Job& job = job_pair.second;
    if (!job.completed) {
      if (kill(-job.pgid, SIGTERM) < 0) {
        if (errno != ESRCH) {
          perror("kill (SIGTERM) in terminate_all_child_process");
        }
      }
      
      job.completed = true;
      job.stopped = false;
      job.status = 0;
      
      std::cout << "[" << job_pair.first << "] Terminated\t" << job.command << std::endl;
    }
  }
  
  set_error("All child processes terminated");
}

std::map<int, Job> Exec::get_jobs() {
  std::lock_guard<std::mutex> lock(jobs_mutex);
  return jobs;
}
#include "exec.h"

#include <sys/resource.h>
#include <sysexits.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <csignal>
#include <iostream>
#include <memory>
#include <vector>

#include "cjsh.h"

Exec::Exec() {
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
        if (kill(pid, 0) == 0) {
          kill(pid, SIGTERM);
        }
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
      // Track the status of the last process in the job as soon as we see it
      if (pid == job.last_pid) {
        job.last_status = status;
      }
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
            std::cout << "\n[" << job_id << "] Done\t" << job.command
                      << std::endl;
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

void Exec::set_process_priority(pid_t pid, bool is_foreground) {
  if (is_foreground) {
    if (setpriority(PRIO_PGRP, pid, -10) == 0) {
      return;
    }
    setpriority(PRIO_PGRP, pid, 0);
    setpriority(PRIO_PROCESS, pid, 0);

#ifdef _GNU_SOURCE
    struct sched_param param;
    param.sched_priority = 0;
    sched_setscheduler(pid, SCHED_BATCH, &param);
#endif
  } else {
    setpriority(PRIO_PGRP, pid, 4);
  }
}

int Exec::execute_command_sync(const std::vector<std::string>& args) {
  if (args.empty()) {
    set_error("cjsh: Failed to parse command");
    last_exit_code = EX_DATAERR;
    return EX_DATAERR;
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
    last_exit_code = EX_OSERR;
    return EX_OSERR;
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

    if (setpriority(PRIO_PROCESS, 0, 0) < 0) {
      perror("setpriority failed in child");
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
      c_args.push_back(const_cast<char*>(arg.c_str()));
    }
    c_args.push_back(nullptr);

    execvp(cmd_args[0].c_str(), c_args.data());

    std::string err = "cjsh: command failed to execute: ( " + cmd_args[0] +
                      " ) -> " + std::string(strerror(errno));
    std::cerr << err << std::endl;
    set_error(err);
    _exit(127);
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

  set_error(exit_code == 0
                ? "command completed successfully"
                : "command failed with exit code " + std::to_string(exit_code));
  last_exit_code = exit_code;
  return exit_code;
}

int Exec::execute_command_async(const std::vector<std::string>& args) {
  if (args.empty()) {
    set_error("cjsh: Failed to parse command");
    last_exit_code = EX_DATAERR;
    return EX_DATAERR;
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
    last_exit_code = EX_OSERR;
    return EX_OSERR;
  }

  if (pid == 0) {
    for (const auto& env : env_assignments) {
      setenv(env.first.c_str(), env.second.c_str(), 1);
    }

    if (setpgid(0, 0) < 0) {
      perror("setpgid failed in child");
      _exit(EXIT_FAILURE);
    }

    if (setpriority(PRIO_PROCESS, 0, 0) < 0) {
      perror("setpriority failed in child");
    }

    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    std::vector<char*> c_args;
    for (auto& arg : cmd_args) {
      c_args.push_back(const_cast<char*>(arg.c_str()));
    }
    c_args.push_back(nullptr);

    execvp(cmd_args[0].c_str(), c_args.data());
    _exit(127);
  } else {
    if (setpgid(pid, pid) < 0 && errno != EACCES && errno != EPERM) {
      perror("setpgid failed in parent");
    }

    Job job;
    job.pgid = pid;
    job.command = args[0];
    job.background = true;
    job.completed = false;
    job.stopped = false;
    job.pids.push_back(pid);

    int job_id = add_job(job);

    set_error("Background job started");

    std::cout << "[" << job_id << "] " << pid << std::endl;
    last_exit_code = 0;
    return 0;
  }
}

int Exec::execute_pipeline(const std::vector<Command>& commands) {
  if (commands.empty()) {
    set_error("cjsh: Empty pipeline");
    last_exit_code = EX_USAGE;
    return EX_USAGE;
  }

  if (commands.size() == 1) {
    const Command& cmd = commands[0];

    if (cmd.background) {
      execute_command_async(cmd.args);
    } else {
      pid_t pid = fork();

      if (pid == -1) {
        set_error("cjsh: Failed to fork process: " +
                  std::string(strerror(errno)));
        last_exit_code = EX_OSERR;
        return EX_OSERR;
      }

      if (pid == 0) {
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

        if (setpriority(PRIO_PROCESS, 0, 0) < 0) {
          perror("setpriority failed in child");
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

        // Handle here document (create temp pipe)
        if (!cmd.here_doc.empty()) {
          int here_pipe[2];
          if (pipe(here_pipe) == -1) {
            perror("pipe for here document");
            _exit(EXIT_FAILURE);
          }
          
          // Write here document content to pipe
          write(here_pipe[1], cmd.here_doc.c_str(), cmd.here_doc.length());
          write(here_pipe[1], "\n", 1);  // Add final newline
          close(here_pipe[1]);
          
          if (dup2(here_pipe[0], STDIN_FILENO) == -1) {
            perror("dup2 here document");
            close(here_pipe[0]);
            _exit(EXIT_FAILURE);
          }
          close(here_pipe[0]);
        } else if (!cmd.input_file.empty()) {
          int fd = open(cmd.input_file.c_str(), O_RDONLY);
          if (fd == -1) {
            std::cerr << "cjsh: " << cmd.input_file << ": " << strerror(errno)
                      << std::endl;
            _exit(EXIT_FAILURE);
          }
          if (dup2(fd, STDIN_FILENO) == -1) {
            std::cerr << "cjsh: dup2 failed for stdin redirection: "
                      << strerror(errno) << std::endl;
            close(fd);
            _exit(EXIT_FAILURE);
          }
          close(fd);
        }

        if (!cmd.output_file.empty()) {
          int fd =
              open(cmd.output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
          if (fd == -1) {
            std::cerr << "cjsh: " << cmd.output_file << ": " << strerror(errno)
                      << std::endl;
            _exit(EXIT_FAILURE);
          }
          if (dup2(fd, STDOUT_FILENO) == -1) {
            std::cerr << "cjsh: dup2 failed for stdout redirection: "
                      << strerror(errno) << std::endl;
            close(fd);
            _exit(EXIT_FAILURE);
          }
          close(fd);
        }

        if (!cmd.append_file.empty()) {
          int fd = open(cmd.append_file.c_str(), O_WRONLY | O_CREAT | O_APPEND,
                        0644);
          if (fd == -1) {
            std::cerr << "cjsh: " << cmd.append_file << ": " << strerror(errno)
                      << std::endl;
            _exit(EXIT_FAILURE);
          }
          if (dup2(fd, STDOUT_FILENO) == -1) {
            std::cerr << "cjsh: dup2 failed for append redirection: "
                      << strerror(errno) << std::endl;
            close(fd);
            _exit(EXIT_FAILURE);
          }
          close(fd);
        }

        // stderr redirections
        if (!cmd.stderr_file.empty()) {
          int flags =
              O_WRONLY | O_CREAT | (cmd.stderr_append ? O_APPEND : O_TRUNC);
          int fd = open(cmd.stderr_file.c_str(), flags, 0644);
          if (fd == -1) {
            std::cerr << "cjsh: " << cmd.stderr_file << ": " << strerror(errno)
                      << std::endl;
            _exit(EXIT_FAILURE);
          }
          if (dup2(fd, STDERR_FILENO) == -1) {
            perror("dup2 stderr");
            close(fd);
            _exit(EXIT_FAILURE);
          }
          close(fd);
        } else if (cmd.stderr_to_stdout) {
          if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1) {
            perror("dup2 2>&1");
            _exit(EXIT_FAILURE);
          }
        }
        
        // Handle stdout to stderr redirection (>&2)
        if (cmd.stdout_to_stderr) {
          if (dup2(STDERR_FILENO, STDOUT_FILENO) == -1) {
            perror("dup2 >&2");
            _exit(EXIT_FAILURE);
          }
        }

        std::vector<char*> c_args;
        for (auto& arg : cmd.args) {
          c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);

        execvp(cmd.args[0].c_str(), c_args.data());
        _exit(127);
      }

      // Parent branch
      if (g_shell && !g_shell->get_interactive_mode()) {
        // In non-interactive mode (e.g., -c), wait synchronously for the child
        // and propagate its real exit status to avoid races with job control.
        int status = 0;
        pid_t wpid;
        do {
          wpid = waitpid(pid, &status, 0);
        } while (wpid == -1 && errno == EINTR);

        int exit_code = 0;
        if (wpid == -1) {
          // If wait failed unexpectedly, report generic failure
          exit_code = EX_OSERR;
        } else if (WIFEXITED(status)) {
          exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
          exit_code = 128 + WTERMSIG(status);
        }

        set_error(exit_code == 0 ? "command completed successfully"
                                 : "command failed with exit code " +
                                       std::to_string(exit_code));
        last_exit_code = exit_code;
        if (g_debug_mode) {
          std::cerr << "DEBUG: execute_pipeline single-command "
                       "(non-interactive) exit="
                    << last_exit_code << std::endl;
        }
        return last_exit_code;
      } else {
        // Interactive mode: use job control and foreground waiting
        Job job;
        job.pgid = pid;
        job.command = cmd.args[0];
        job.background = false;
        job.completed = false;
        job.stopped = false;
        job.pids.push_back(pid);
        job.last_pid = pid;

        int job_id = add_job(job);
        put_job_in_foreground(job_id, false);

        // If there were redirections, ensure file buffers are flushed
        if (!cmd.output_file.empty() || !cmd.append_file.empty() ||
            !cmd.stderr_file.empty()) {
          sync();
        }

        if (g_debug_mode) {
          std::cerr << "DEBUG: execute_pipeline single-command returning "
                       "last_exit_code="
                    << last_exit_code << std::endl;
        }
        return last_exit_code;
      }
    }

    return 0;
  }
  std::vector<pid_t> pids;
  pid_t pgid = 0;

  std::vector<std::array<int, 2>> pipes(commands.size() - 1);

  try {
    for (size_t i = 0; i < commands.size() - 1; i++) {
      if (pipe(pipes[i].data()) == -1) {
        set_error("cjsh: Failed to create pipe: " +
                  std::string(strerror(errno)));
        last_exit_code = EX_OSERR;
        return EX_OSERR;
      }
    }

    for (size_t i = 0; i < commands.size(); i++) {
      const Command& cmd = commands[i];

      if (cmd.args.empty()) {
        set_error("cjsh: Empty command in pipeline");
        std::cerr << last_terminal_output_error << std::endl;

        for (size_t j = 0; j < commands.size() - 1; j++) {
          close(pipes[j][0]);
          close(pipes[j][1]);
        }

        return 1;
      }

      pid_t pid = fork();

      if (pid == -1) {
        set_error("cjsh: Failed to fork process: " +
                  std::string(strerror(errno)));
        last_exit_code = EX_OSERR;
        return EX_OSERR;
      }

      if (pid == 0) {
        if (i == 0) {
          pgid = getpid();
        }

        if (setpgid(0, pgid) < 0) {
          perror("setpgid failed in child");
          _exit(EXIT_FAILURE);
        }

        if (shell_is_interactive && i == 0) {
          if (tcsetpgrp(shell_terminal, pgid) < 0) {
            perror("tcsetpgrp failed in child");
          }
        }

        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
        if (i == 0) {
          // Handle here document for first command in pipeline
          if (!cmd.here_doc.empty()) {
            int here_pipe[2];
            if (pipe(here_pipe) == -1) {
              perror("pipe for here document");
              _exit(EXIT_FAILURE);
            }
            
            // Write here document content to pipe
            write(here_pipe[1], cmd.here_doc.c_str(), cmd.here_doc.length());
            write(here_pipe[1], "\n", 1);  // Add final newline
            close(here_pipe[1]);
            
            if (dup2(here_pipe[0], STDIN_FILENO) == -1) {
              perror("dup2 here document");
              close(here_pipe[0]);
              _exit(EXIT_FAILURE);
            }
            close(here_pipe[0]);
          } else if (!cmd.input_file.empty()) {
            int fd = open(cmd.input_file.c_str(), O_RDONLY);
            if (fd == -1) {
              std::cerr << "cjsh: " << cmd.input_file << ": " << strerror(errno)
                        << std::endl;
              _exit(EXIT_FAILURE);
            }
            if (dup2(fd, STDIN_FILENO) == -1) {
              perror("dup2 input");
              close(fd);
              _exit(EXIT_FAILURE);
            }
            close(fd);
          }
        } else {
          if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
            perror("dup2 pipe input");
            _exit(EXIT_FAILURE);
          }
        }

        if (i == commands.size() - 1) {
          if (!cmd.output_file.empty()) {
            int fd = open(cmd.output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                          0644);
            if (fd == -1) {
              std::cerr << "cjsh: " << cmd.output_file << ": "
                        << strerror(errno) << std::endl;
              _exit(EXIT_FAILURE);
            }
            if (dup2(fd, STDOUT_FILENO) == -1) {
              perror("dup2 output");
              close(fd);
              _exit(EXIT_FAILURE);
            }
            close(fd);
          } else if (!cmd.append_file.empty()) {
            int fd = open(cmd.append_file.c_str(),
                          O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd == -1) {
              std::cerr << "cjsh: " << cmd.append_file << ": "
                        << strerror(errno) << std::endl;
              _exit(EXIT_FAILURE);
            }
            if (dup2(fd, STDOUT_FILENO) == -1) {
              perror("dup2 append");
              close(fd);
              _exit(EXIT_FAILURE);
            }
            close(fd);
          }
        } else {
          if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
            perror("dup2 pipe output");
            _exit(EXIT_FAILURE);
          }
        }

        // Apply stderr redirections for every command in pipeline
        if (!cmd.stderr_file.empty()) {
          int flags =
              O_WRONLY | O_CREAT | (cmd.stderr_append ? O_APPEND : O_TRUNC);
          int fd = open(cmd.stderr_file.c_str(), flags, 0644);
          if (fd == -1) {
            std::cerr << "cjsh: " << cmd.stderr_file << ": " << strerror(errno)
                      << std::endl;
            _exit(EXIT_FAILURE);
          }
          if (dup2(fd, STDERR_FILENO) == -1) {
            perror("dup2 stderr");
            close(fd);
            _exit(EXIT_FAILURE);
          }
          close(fd);
        } else if (cmd.stderr_to_stdout) {
          if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1) {
            perror("dup2 2>&1");
            _exit(EXIT_FAILURE);
          }
        }
        
        // Handle stdout to stderr redirection (>&2)
        if (cmd.stdout_to_stderr) {
          if (dup2(STDERR_FILENO, STDOUT_FILENO) == -1) {
            perror("dup2 >&2");
            _exit(EXIT_FAILURE);
          }
        }

        for (size_t j = 0; j < commands.size() - 1; j++) {
          close(pipes[j][0]);
          close(pipes[j][1]);
        }

        std::vector<char*> c_args;
        for (auto& arg : cmd.args) {
          c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);

        execvp(cmd.args[0].c_str(), c_args.data());
        _exit(127);
      }

      if (i == 0) {
        pgid = pid;
      }

      if (setpgid(pid, pgid) < 0) {
        if (errno != EACCES && errno != EPERM) {
          perror("setpgid failed in parent");
        }
      }

      pids.push_back(pid);
    }

    for (size_t i = 0; i < commands.size() - 1; i++) {
      close(pipes[i][0]);
      close(pipes[i][1]);
    }

  } catch (const std::exception& e) {
    std::cerr << "Error executing pipeline: " << e.what() << std::endl;
    for (pid_t pid : pids) {
      kill(pid, SIGTERM);
    }
    return 1;
  }

  Job job;
  job.pgid = pgid;
  job.command = commands[0].args[0] + " | ...";
  job.background = commands.back().background;
  job.completed = false;
  job.stopped = false;
  job.pids = pids;
  job.last_pid = pids.empty() ? -1 : pids.back();

  int job_id = add_job(job);

  if (job.background) {
    put_job_in_background(job_id, false);
    std::cout << "[" << job_id << "] " << pgid << std::endl;
    last_exit_code = 0;
  } else {
    put_job_in_foreground(job_id, false);
    // After foreground wait, compute exit code based on last process
    std::lock_guard<std::mutex> lock(jobs_mutex);
    auto it = jobs.find(job_id);
    if (it != jobs.end()) {
      int st = it->second.last_status;
      if (WIFEXITED(st))
        last_exit_code = WEXITSTATUS(st);
      else if (WIFSIGNALED(st))
        last_exit_code = 128 + WTERMSIG(st);
    }
  }

  return last_exit_code;
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

void Exec::update_job_status(int job_id, bool completed, bool stopped,
                             int status) {
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

  set_process_priority(job.pgid, true);

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

  set_process_priority(job.pgid, false);

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
  pid_t last_pid = it->second.last_pid;

  lock.unlock();

  int status = 0;
  pid_t pid;

  bool job_stopped = false;
  bool saw_last = false;
  int last_status = 0;

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

    // Track last process status separately for correct pipeline exit code
    if (pid == last_pid) {
      // status captured in variable 'status'
    }

    if (pid == last_pid) {
      saw_last = true;
      last_status = status;
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

      // Prefer exit status of the last command in pipeline if available.
      // If the SIGCHLD handler already reaped the child, our local 'status'
      // may be uninitialized (e.g., 0). In that case, fall back to the
      // recorded job.last_status or job.status captured by the signal handler.
      int final_status = saw_last ? last_status : status;
      if (!(WIFEXITED(final_status) || WIFSIGNALED(final_status))) {
        final_status = (job.last_status != 0) ? job.last_status : job.status;
      }
      job.last_status = final_status;

      if (WIFEXITED(final_status)) {
        int exit_status = WEXITSTATUS(final_status);
        last_exit_code = exit_status;
        job.completed = true;
        if (g_debug_mode) {
          std::cerr << "DEBUG: wait_for_job setting last_exit_code="
                    << exit_status << " from final_status=" << final_status
                    << std::endl;
        }
        if (exit_status == 0) {
          set_error("command completed successfully");
        } else {
          set_error("command failed with exit code " +
                    std::to_string(exit_status));
        }
      } else if (WIFSIGNALED(final_status)) {
        last_exit_code = 128 + WTERMSIG(final_status);
        set_error("command terminated by signal " +
                  std::to_string(WTERMSIG(final_status)));
      }
    }
  }
}

void Exec::terminate_all_child_process() {
  std::lock_guard<std::mutex> lock(jobs_mutex);

  for (auto& job_pair : jobs) {
    Job& job = job_pair.second;
    if (!job.completed) {
      if (killpg(job.pgid, 0) == 0) {
        if (kill(-job.pgid, SIGTERM) < 0) {
          if (errno != ESRCH) {
            perror("kill (SIGTERM) in terminate_all_child_process");
          }
        }
        std::cout << "[" << job_pair.first << "] Terminated\t" << job.command
                  << std::endl;
      }

      job.completed = true;
      job.stopped = false;
      job.status = 0;
    }
  }

  set_error("All child processes terminated");
}

std::map<int, Job> Exec::get_jobs() {
  std::lock_guard<std::mutex> lock(jobs_mutex);
  return jobs;
}
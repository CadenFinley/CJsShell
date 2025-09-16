#include "exec.h"

#include <sysexits.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <csignal>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <sys/stat.h>

#include "builtin.h"
#include "cjsh.h"
#include "job_control.h"
#include "signal_handler.h"

// Helper function to check if noclobber should prevent file creation
static bool should_noclobber_prevent_overwrite(const std::string& filename, bool force_overwrite = false) {
  if (force_overwrite) {
    return false;  // Force overwrite (>|) bypasses noclobber
  }
  
  if (!g_shell || !g_shell->get_shell_option("noclobber")) {
    return false;  // Noclobber not enabled
  }
  
  struct stat file_stat;
  if (stat(filename.c_str(), &file_stat) == 0) {
    // File exists and noclobber is enabled
    return true;
  }
  
  return false;  // File doesn't exist, safe to create
}

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
  if (g_debug_mode) {
    std::cerr << "DEBUG: Exec destructor called" << std::endl;
  }

  int status;
  pid_t pid;
  int zombie_count = 0;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    zombie_count++;
    if (g_debug_mode && zombie_count <= 3) {
      std::cerr << "DEBUG: Exec destructor reaped zombie " << pid << std::endl;
    }
  }

  if (g_debug_mode && zombie_count > 0) {
    std::cerr << "DEBUG: Exec destructor reaped " << zombie_count << " zombies"
              << std::endl;
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
  static bool use_signal_masking = false;
  static int signal_count = 0;

  if (++signal_count > 10) {
    use_signal_masking = true;
  }

  std::unique_ptr<SignalMask> mask;
  if (use_signal_masking) {
    mask = std::make_unique<SignalMask>(SIGCHLD);
  }

  std::lock_guard<std::mutex> lock(jobs_mutex);

  for (auto& job_pair : jobs) {
    int job_id = job_pair.first;
    Job& job = job_pair.second;

    auto it = std::find(job.pids.begin(), job.pids.end(), pid);
    if (it != job.pids.end()) {
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
            std::cerr << "\n[" << job_id << "] Done\t" << job.command
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

int Exec::execute_command_sync(const std::vector<std::string>& args) {
  if (args.empty()) {
    set_error("cjsh: cannot execute empty command - no arguments provided");
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
    set_error("cjsh: failed to fork process: " + std::string(strerror(errno)));
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

    std::string err = "cjsh: '" + cmd_args[0] + "': command not found";
    if (errno == ENOENT) {
      err = "cjsh: '" + cmd_args[0] + "': command not found";
    } else if (errno == EACCES) {
      err = "cjsh: '" + cmd_args[0] + "': permission denied";
    } else if (errno == ENOEXEC) {
      err = "cjsh: '" + cmd_args[0] + "': invalid executable format";
    } else {
      err = "cjsh: '" + cmd_args[0] + "': " + std::string(strerror(errno));
    }
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

  std::string full_command;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0)
      full_command += " ";
    full_command += args[i];
  }
  int new_job_id = JobManager::instance().add_job(pid, {pid}, full_command);

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

    JobManager::instance().remove_job(new_job_id);
  }

  set_error(exit_code == 0
                ? "command completed successfully"
                : "command failed with exit code " + std::to_string(exit_code));
  last_exit_code = exit_code;
  return exit_code;
}

int Exec::execute_command_async(const std::vector<std::string>& args) {
  if (args.empty()) {
    set_error("cjsh: cannot execute empty command - no arguments provided");
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
    std::string cmd_name = cmd_args.empty() ? "unknown" : cmd_args[0];
    set_error("cjsh: failed to create background process for '" + cmd_name +
              "': " + std::string(strerror(errno)));
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

    std::string full_command;
    for (size_t i = 0; i < args.size(); ++i) {
      if (i > 0)
        full_command += " ";
      full_command += args[i];
    }
    JobManager::instance().add_job(pid, {pid}, full_command);
    JobManager::instance().set_last_background_pid(pid);

    set_error("Background job started");

    std::cerr << "[" << job_id << "] " << pid << std::endl;
    last_exit_code = 0;
    return job_id;
  }
}

int Exec::execute_pipeline(const std::vector<Command>& commands) {
  if (commands.empty()) {
    set_error("cjsh: cannot execute empty pipeline - no commands provided");
    last_exit_code = EX_USAGE;
    return EX_USAGE;
  }

  if (commands.size() == 1) {
    const Command& cmd = commands[0];

    if (cmd.background) {
      execute_command_async(cmd.args);
    } else {
      if (!cmd.args.empty() && g_shell && g_shell->get_built_ins() &&
          g_shell->get_built_ins()->is_builtin_command(cmd.args[0])) {
        if (cmd.args[0] == "__INTERNAL_SUBSHELL__") {
          int orig_stdin = dup(STDIN_FILENO);
          int orig_stdout = dup(STDOUT_FILENO);
          int orig_stderr = dup(STDERR_FILENO);

          if (orig_stdin == -1 || orig_stdout == -1 || orig_stderr == -1) {
            set_error("cjsh: failed to save original file descriptors");
            last_exit_code = EX_OSERR;
            return EX_OSERR;
          }

          int exit_code = 0;

          try {
            if (!cmd.input_file.empty()) {
              int fd = open(cmd.input_file.c_str(), O_RDONLY);
              if (fd == -1) {
                throw std::runtime_error("Failed to open input file: " +
                                         cmd.input_file);
              }
              if (dup2(fd, STDIN_FILENO) == -1) {
                close(fd);
                throw std::runtime_error("Failed to redirect stdin");
              }
              close(fd);
            }

            if (!cmd.here_string.empty()) {
              int here_pipe[2];
              if (pipe(here_pipe) == -1) {
                throw std::runtime_error(
                    "Failed to create pipe for here string");
              }

              std::string content = cmd.here_string + "\n";
              ssize_t bytes_written =
                  write(here_pipe[1], content.c_str(), content.length());
              if (bytes_written == -1) {
                close(here_pipe[1]);
                close(here_pipe[0]);
                throw std::runtime_error("Failed to write here string content");
              }
              close(here_pipe[1]);

              if (dup2(here_pipe[0], STDIN_FILENO) == -1) {
                close(here_pipe[0]);
                throw std::runtime_error(
                    "Failed to redirect stdin for here string");
              }
              close(here_pipe[0]);
            }

            if (!cmd.output_file.empty()) {
              // Check noclobber before opening output file
              if (should_noclobber_prevent_overwrite(cmd.output_file, cmd.force_overwrite)) {
                throw std::runtime_error("Cannot overwrite existing file '" + 
                                         cmd.output_file + "' (noclobber is set)");
              }
              
              int fd = open(cmd.output_file.c_str(),
                            O_WRONLY | O_CREAT | O_TRUNC, 0644);
              if (fd == -1) {
                throw std::runtime_error("Failed to open output file: " +
                                         cmd.output_file);
              }
              if (dup2(fd, STDOUT_FILENO) == -1) {
                close(fd);
                throw std::runtime_error("Failed to redirect stdout");
              }
              close(fd);
            }

            if (cmd.both_output && !cmd.both_output_file.empty()) {
              // Check noclobber before opening &> output file
              if (should_noclobber_prevent_overwrite(cmd.both_output_file)) {
                throw std::runtime_error("Cannot overwrite existing file '" + 
                                         cmd.both_output_file + "' (noclobber is set)");
              }
              
              int fd = open(cmd.both_output_file.c_str(),
                            O_WRONLY | O_CREAT | O_TRUNC, 0644);
              if (fd == -1) {
                throw std::runtime_error("Failed to open &> output file: " +
                                         cmd.both_output_file);
              }
              if (dup2(fd, STDOUT_FILENO) == -1) {
                close(fd);
                throw std::runtime_error("Failed to redirect stdout for &>");
              }
              if (dup2(fd, STDERR_FILENO) == -1) {
                close(fd);
                throw std::runtime_error("Failed to redirect stderr for &>");
              }
              close(fd);
            }

            if (!cmd.append_file.empty()) {
              int fd = open(cmd.append_file.c_str(),
                            O_WRONLY | O_CREAT | O_APPEND, 0644);
              if (fd == -1) {
                throw std::runtime_error("Failed to open append file: " +
                                         cmd.append_file);
              }
              if (dup2(fd, STDOUT_FILENO) == -1) {
                close(fd);
                throw std::runtime_error(
                    "Failed to redirect stdout for append");
              }
              close(fd);
            }

            if (!cmd.stderr_file.empty()) {
              // Check noclobber for stderr redirection (only if not appending)
              if (!cmd.stderr_append && should_noclobber_prevent_overwrite(cmd.stderr_file)) {
                throw std::runtime_error("Cannot overwrite existing file '" + 
                                         cmd.stderr_file + "' (noclobber is set)");
              }
              
              int flags =
                  O_WRONLY | O_CREAT | (cmd.stderr_append ? O_APPEND : O_TRUNC);
              int fd = open(cmd.stderr_file.c_str(), flags, 0644);
              if (fd == -1) {
                throw std::runtime_error("Failed to open stderr file: " +
                                         cmd.stderr_file);
              }
              if (dup2(fd, STDERR_FILENO) == -1) {
                close(fd);
                throw std::runtime_error("Failed to redirect stderr");
              }
              close(fd);
            }

            if (cmd.stderr_to_stdout) {
              if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1) {
                throw std::runtime_error("Failed to redirect stderr to stdout");
              }
            }

            if (cmd.stdout_to_stderr) {
              if (dup2(STDERR_FILENO, STDOUT_FILENO) == -1) {
                throw std::runtime_error("Failed to redirect stdout to stderr");
              }
            }

            exit_code = g_shell->get_built_ins()->builtin_command(cmd.args);

          } catch (const std::exception& e) {
            set_error("cjsh: " + std::string(e.what()));
            exit_code = EX_OSERR;
          }

          dup2(orig_stdin, STDIN_FILENO);
          dup2(orig_stdout, STDOUT_FILENO);
          dup2(orig_stderr, STDERR_FILENO);
          close(orig_stdin);
          close(orig_stdout);
          close(orig_stderr);

          set_error(exit_code == 0 ? "builtin command completed successfully"
                                   : "builtin command failed with exit code " +
                                         std::to_string(exit_code));
          last_exit_code = exit_code;
          return exit_code;
        }
      }

      pid_t pid = fork();

      if (pid == -1) {
        set_error("cjsh: failed to fork process: " +
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

        if (!cmd.here_doc.empty()) {
          int here_pipe[2];
          if (pipe(here_pipe) == -1) {
            perror("pipe for here document");
            _exit(EXIT_FAILURE);
          }

          ssize_t bytes_written =
              write(here_pipe[1], cmd.here_doc.c_str(), cmd.here_doc.length());
          if (bytes_written == -1) {
            perror("write here document content");
            close(here_pipe[1]);
            _exit(EXIT_FAILURE);
          }
          bytes_written = write(here_pipe[1], "\n", 1);
          if (bytes_written == -1) {
            perror("write here document newline");
            close(here_pipe[1]);
            _exit(EXIT_FAILURE);
          }
          close(here_pipe[1]);

          if (dup2(here_pipe[0], STDIN_FILENO) == -1) {
            perror("dup2 here document");
            close(here_pipe[0]);
            _exit(EXIT_FAILURE);
          }
          close(here_pipe[0]);
        }

        else if (!cmd.here_string.empty()) {
          int here_pipe[2];
          if (pipe(here_pipe) == -1) {
            perror("pipe for here string");
            _exit(EXIT_FAILURE);
          }

          std::string content = cmd.here_string + "\n";
          ssize_t bytes_written =
              write(here_pipe[1], content.c_str(), content.length());
          if (bytes_written == -1) {
            perror("write here string content");
            close(here_pipe[1]);
            _exit(EXIT_FAILURE);
          }
          close(here_pipe[1]);

          if (dup2(here_pipe[0], STDIN_FILENO) == -1) {
            perror("dup2 here string");
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
            std::cerr << "cjsh: failed to redirect input from '"
                      << cmd.input_file << "': " << strerror(errno)
                      << std::endl;
            close(fd);
            _exit(EXIT_FAILURE);
          }
          close(fd);
        }

        if (!cmd.output_file.empty()) {
          // Check noclobber before opening output file
          if (should_noclobber_prevent_overwrite(cmd.output_file, cmd.force_overwrite)) {
            std::cerr << "cjsh: " << cmd.output_file << ": cannot overwrite existing file (noclobber is set)" << std::endl;
            _exit(EXIT_FAILURE);
          }
          
          int flags = O_WRONLY | O_CREAT | O_TRUNC;

          int fd = open(cmd.output_file.c_str(), flags, 0644);
          if (fd == -1) {
            std::cerr << "cjsh: " << cmd.output_file << ": " << strerror(errno)
                      << std::endl;
            _exit(EXIT_FAILURE);
          }
          if (dup2(fd, STDOUT_FILENO) == -1) {
            std::cerr << "cjsh: failed to redirect output to '"
                      << cmd.output_file << "': " << strerror(errno)
                      << std::endl;
            close(fd);
            _exit(EXIT_FAILURE);
          }
          close(fd);
        }

        if (cmd.both_output && !cmd.both_output_file.empty()) {
          // Check noclobber before opening &> output file
          if (should_noclobber_prevent_overwrite(cmd.both_output_file)) {
            std::cerr << "cjsh: " << cmd.both_output_file << ": cannot overwrite existing file (noclobber is set)" << std::endl;
            _exit(EXIT_FAILURE);
          }
          
          int fd = open(cmd.both_output_file.c_str(),
                        O_WRONLY | O_CREAT | O_TRUNC, 0644);
          if (fd == -1) {
            std::cerr << "cjsh: " << cmd.both_output_file << ": "
                      << strerror(errno) << std::endl;
            _exit(EXIT_FAILURE);
          }
          if (dup2(fd, STDOUT_FILENO) == -1) {
            std::cerr << "cjsh: dup2 failed for stdout in &> redirection: "
                      << strerror(errno) << std::endl;
            close(fd);
            _exit(EXIT_FAILURE);
          }
          if (dup2(fd, STDERR_FILENO) == -1) {
            std::cerr << "cjsh: dup2 failed for stderr in &> redirection: "
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

        if (!cmd.stderr_file.empty()) {
          // Check noclobber for stderr redirection (only if not appending)
          if (!cmd.stderr_append && should_noclobber_prevent_overwrite(cmd.stderr_file)) {
            std::cerr << "cjsh: " << cmd.stderr_file << ": cannot overwrite existing file (noclobber is set)" << std::endl;
            _exit(EXIT_FAILURE);
          }
          
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

        if (cmd.stdout_to_stderr) {
          if (dup2(STDERR_FILENO, STDOUT_FILENO) == -1) {
            perror("dup2 >&2");
            _exit(EXIT_FAILURE);
          }
        }

        for (const auto& fd_redir : cmd.fd_redirections) {
          int fd_num = fd_redir.first;
          const std::string& spec = fd_redir.second;

          int file_fd;
          if (spec.find("input:") == 0) {
            std::string file = spec.substr(6);
            file_fd = open(file.c_str(), O_RDONLY);
          } else if (spec.find("output:") == 0) {
            std::string file = spec.substr(7);
            file_fd = open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
          } else {
            if (fd_num == 0) {
              file_fd = open(spec.c_str(), O_RDONLY);
            } else {
              file_fd = open(spec.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }
          }

          if (file_fd == -1) {
            std::cerr << "cjsh: " << spec << ": " << strerror(errno)
                      << std::endl;
            _exit(EXIT_FAILURE);
          }

          if (dup2(file_fd, fd_num) == -1) {
            std::cerr << "cjsh: dup2 failed for file descriptor " << fd_num
                      << ": " << strerror(errno) << std::endl;
            close(file_fd);
            _exit(EXIT_FAILURE);
          }
          close(file_fd);
        }

        for (const auto& fd_dup : cmd.fd_duplications) {
          int src_fd = fd_dup.first;
          int dst_fd = fd_dup.second;

          if (dup2(dst_fd, src_fd) == -1) {
            std::cerr << "cjsh: dup2 failed for " << src_fd << ">&" << dst_fd
                      << ": " << strerror(errno) << std::endl;
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

      if (g_shell && !g_shell->get_interactive_mode()) {
        int status = 0;
        pid_t wpid;
        do {
          wpid = waitpid(pid, &status, 0);
        } while (wpid == -1 && errno == EINTR);

        int exit_code = 0;
        if (wpid == -1) {
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
        set_error("cjsh: failed to create pipe " + std::to_string(i + 1) +
                  " for pipeline: " + std::string(strerror(errno)));
        last_exit_code = EX_OSERR;
        return EX_OSERR;
      }
    }

    for (size_t i = 0; i < commands.size(); i++) {
      const Command& cmd = commands[i];

      if (cmd.args.empty()) {
        set_error("cjsh: command " + std::to_string(i + 1) +
                  " in pipeline is empty");
        std::cerr << last_terminal_output_error << std::endl;

        for (size_t j = 0; j < commands.size() - 1; j++) {
          close(pipes[j][0]);
          close(pipes[j][1]);
        }

        return 1;
      }

      pid_t pid = fork();

      if (pid == -1) {
        std::string cmd_name = cmd.args.empty() ? "unknown" : cmd.args[0];
        set_error("cjsh: failed to create process for '" + cmd_name +
                  "' (command " + std::to_string(i + 1) +
                  " in pipeline): " + std::string(strerror(errno)));
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
          if (!cmd.here_doc.empty()) {
            int here_pipe[2];
            if (pipe(here_pipe) == -1) {
              perror("pipe for here document");
              _exit(EXIT_FAILURE);
            }

            ssize_t bytes_written = write(here_pipe[1], cmd.here_doc.c_str(),
                                          cmd.here_doc.length());
            if (bytes_written == -1) {
              perror("write here document content");
              close(here_pipe[1]);
              _exit(EXIT_FAILURE);
            }
            bytes_written = write(here_pipe[1], "\n", 1);
            if (bytes_written == -1) {
              perror("write here document newline");
              close(here_pipe[1]);
              _exit(EXIT_FAILURE);
            }
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

        if (g_shell && g_shell->get_built_ins() &&
            g_shell->get_built_ins()->is_builtin_command(cmd.args[0])) {
          int exit_code = g_shell->get_built_ins()->builtin_command(cmd.args);

          fflush(stdout);
          fflush(stderr);

          _exit(exit_code);
        } else {
          std::vector<char*> c_args;
          for (auto& arg : cmd.args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
          }
          c_args.push_back(nullptr);

          execvp(cmd.args[0].c_str(), c_args.data());
          _exit(127);
        }
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

  std::string pipeline_command;
  for (size_t i = 0; i < commands.size(); ++i) {
    if (i > 0)
      pipeline_command += " | ";
    for (size_t j = 0; j < commands[i].args.size(); ++j) {
      if (j > 0)
        pipeline_command += " ";
      pipeline_command += commands[i].args[j];
    }
  }
  int new_job_id = JobManager::instance().add_job(pgid, pids, pipeline_command);

  if (job.background) {
    JobManager::instance().set_last_background_pid(pids.empty() ? -1
                                                                : pids.back());
  }

  if (job.background) {
    put_job_in_background(job_id, false);
    std::cerr << "[" << job_id << "] " << pgid << std::endl;
    last_exit_code = 0;
  } else {
    put_job_in_foreground(job_id, false);

    std::lock_guard<std::mutex> lock(jobs_mutex);
    auto it = jobs.find(job_id);
    if (it != jobs.end()) {
      JobManager::instance().remove_job(new_job_id);

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
    std::cerr << "cjsh: job [" << job_id << "] not found" << std::endl;
    return;
  }

  Job& job = it->second;

  bool terminal_control_acquired = false;
  if (shell_is_interactive && isatty(shell_terminal)) {
    if (tcsetpgrp(shell_terminal, job.pgid) == 0) {
      terminal_control_acquired = true;
      if (g_debug_mode) {
        std::cerr << "DEBUG: Gave terminal control to job " << job_id
                  << " (pgid: " << job.pgid << ")" << std::endl;
      }
    } else {
      if (errno != ENOTTY && errno != EINVAL && errno != EPERM) {
        if (g_debug_mode) {
          std::cerr << "DEBUG: Could not give terminal control to job "
                    << job_id << ": " << strerror(errno) << std::endl;
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

  if (terminal_control_acquired && shell_is_interactive &&
      isatty(shell_terminal)) {
    if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
      if (errno != ENOTTY && errno != EINVAL) {
        std::cerr << "cjsh: warning: failed to restore terminal control: "
                  << strerror(errno) << std::endl;
      }
    }

    if (tcgetattr(shell_terminal, &shell_tmodes) == 0) {
      if (tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes) < 0) {
        perror("tcsetattr");
      }
    }
  }
}

void Exec::put_job_in_background(int job_id, bool cont) {
  std::lock_guard<std::mutex> lock(jobs_mutex);

  auto it = jobs.find(job_id);
  if (it == jobs.end()) {
    std::cerr << "cjsh: job [" << job_id << "] not found" << std::endl;
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

    if (pid == last_pid) {
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

  if (g_debug_mode && !jobs.empty()) {
    std::cerr << "DEBUG: Starting graceful termination of " << jobs.size()
              << " jobs" << std::endl;
  }

  bool any_jobs_terminated = false;
  for (auto& job_pair : jobs) {
    Job& job = job_pair.second;
    if (!job.completed) {
      if (killpg(job.pgid, 0) == 0) {
        if (killpg(job.pgid, SIGTERM) == 0) {
          any_jobs_terminated = true;
          if (g_debug_mode) {
            std::cerr << "DEBUG: Sent SIGTERM to job " << job_pair.first
                      << " (pgid " << job.pgid << ")" << std::endl;
          }
        } else {
          if (errno != ESRCH) {
            std::cerr << "cjsh: warning: failed to terminate job ["
                      << job_pair.first << "] '" << job.command
                      << "': " << strerror(errno) << std::endl;
          }
        }
        std::cerr << "[" << job_pair.first << "] Terminated\t" << job.command
                  << std::endl;
      }
    }
  }

  if (any_jobs_terminated) {
    usleep(50000);
  }

  for (auto& job_pair : jobs) {
    Job& job = job_pair.second;
    if (!job.completed) {
      if (killpg(job.pgid, 0) == 0) {
        if (killpg(job.pgid, SIGKILL) == 0) {
          if (g_debug_mode) {
            std::cerr << "DEBUG: Sent SIGKILL to stubborn job "
                      << job_pair.first << " (pgid " << job.pgid << ")"
                      << std::endl;
          }
        } else {
          if (errno != ESRCH) {
            perror("killpg (SIGKILL) in terminate_all_child_process");
          }
        }
      }

      job.completed = true;
      job.stopped = false;
      job.status = 0;
    }
  }

  int status;
  pid_t pid;
  int zombie_count = 0;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0 && zombie_count < 10) {
    zombie_count++;
    if (g_debug_mode && zombie_count <= 3) {
      std::cerr << "DEBUG: Reaped zombie process " << pid << std::endl;
    }
  }

  set_error("All child processes terminated");
}

std::map<int, Job> Exec::get_jobs() {
  std::lock_guard<std::mutex> lock(jobs_mutex);
  return jobs;
}
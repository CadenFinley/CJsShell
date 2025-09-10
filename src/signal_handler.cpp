#include "signal_handler.h"

#include <algorithm>

#include "cjsh.h"
#include "exec.h"
#include "job_control.h"

// CADEN DONT TOUCH IT WORKS
// ty to Fish shell for lookup table

std::atomic<SignalHandler*> SignalHandler::s_instance(nullptr);
volatile sig_atomic_t SignalHandler::s_sigint_received = 0;
volatile sig_atomic_t SignalHandler::s_sigchld_received = 0;
volatile sig_atomic_t SignalHandler::s_sighup_received = 0;
volatile sig_atomic_t SignalHandler::s_sigterm_received = 0;
pid_t SignalHandler::s_main_pid = getpid();
std::vector<int> SignalHandler::s_observed_signals;

// Signal lookup table with names and descriptions
const std::vector<SignalInfo> SignalHandler::s_signal_table = {
#ifdef SIGHUP
    {SIGHUP, "SIGHUP", "Terminal hung up"},
#endif
#ifdef SIGINT
    {SIGINT, "SIGINT", "Interrupt (Ctrl+C)"},
#endif
#ifdef SIGQUIT
    {SIGQUIT, "SIGQUIT", "Quit with core dump (Ctrl+\\)"},
#endif
#ifdef SIGILL
    {SIGILL, "SIGILL", "Illegal instruction"},
#endif
#ifdef SIGTRAP
    {SIGTRAP, "SIGTRAP", "Trace/breakpoint trap"},
#endif
#ifdef SIGABRT
    {SIGABRT, "SIGABRT", "Abort"},
#endif
#ifdef SIGBUS
    {SIGBUS, "SIGBUS", "Bus error (misaligned address)"},
#endif
#ifdef SIGFPE
    {SIGFPE, "SIGFPE", "Floating point exception"},
#endif
#ifdef SIGKILL
    {SIGKILL, "SIGKILL", "Kill (cannot be caught or ignored)"},
#endif
#ifdef SIGUSR1
    {SIGUSR1, "SIGUSR1", "User-defined signal 1"},
#endif
#ifdef SIGUSR2
    {SIGUSR2, "SIGUSR2", "User-defined signal 2"},
#endif
#ifdef SIGSEGV
    {SIGSEGV, "SIGSEGV", "Segmentation violation"},
#endif
#ifdef SIGPIPE
    {SIGPIPE, "SIGPIPE", "Broken pipe"},
#endif
#ifdef SIGALRM
    {SIGALRM, "SIGALRM", "Alarm clock"},
#endif
#ifdef SIGTERM
    {SIGTERM, "SIGTERM", "Termination"},
#endif
#ifdef SIGCHLD
    {SIGCHLD, "SIGCHLD", "Child status changed"},
#endif
#ifdef SIGCONT
    {SIGCONT, "SIGCONT", "Continue executing if stopped"},
#endif
#ifdef SIGSTOP
    {SIGSTOP, "SIGSTOP", "Stop executing (cannot be caught or ignored)"},
#endif
#ifdef SIGTSTP
    {SIGTSTP, "SIGTSTP", "Terminal stop (Ctrl+Z)"},
#endif
#ifdef SIGTTIN
    {SIGTTIN, "SIGTTIN", "Background process trying to read from TTY"},
#endif
#ifdef SIGTTOU
    {SIGTTOU, "SIGTTOU", "Background process trying to write to TTY"},
#endif
#ifdef SIGURG
    {SIGURG, "SIGURG", "Urgent condition on socket"},
#endif
#ifdef SIGXCPU
    {SIGXCPU, "SIGXCPU", "CPU time limit exceeded"},
#endif
#ifdef SIGXFSZ
    {SIGXFSZ, "SIGXFSZ", "File size limit exceeded"},
#endif
#ifdef SIGVTALRM
    {SIGVTALRM, "SIGVTALRM", "Virtual timer expired"},
#endif
#ifdef SIGPROF
    {SIGPROF, "SIGPROF", "Profiling timer expired"},
#endif
#ifdef SIGWINCH
    {SIGWINCH, "SIGWINCH", "Window size change"},
#endif
#ifdef SIGIO
    {SIGIO, "SIGIO", "I/O now possible"},
#endif
#ifdef SIGPWR
    {SIGPWR, "SIGPWR", "Power failure restart"},
#endif
#ifdef SIGSYS
    {SIGSYS, "SIGSYS", "Bad system call"},
#endif
#ifdef SIGINFO
    {SIGINFO, "SIGINFO", "Information request"},
#endif
};

SignalHandler* g_signal_handler = nullptr;

SignalHandler::SignalHandler() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Constructing SignalHandler" << std::endl;
  signal_unblock_all();
  s_instance.store(this);
}

SignalHandler::~SignalHandler() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Destroying SignalHandler" << std::endl;
  restore_original_handlers();
  s_instance.store(nullptr);
}

const char* SignalHandler::get_signal_name(int signum) {
  for (const auto& signal : s_signal_table) {
    if (signal.signal == signum) {
      return signal.name;
    }
  }
  return "UNKNOWN";
}

const char* SignalHandler::get_signal_description(int signum) {
  for (const auto& signal : s_signal_table) {
    if (signal.signal == signum) {
      return signal.description;
    }
  }
  return "Unknown signal";
}

int SignalHandler::name_to_signal(const std::string& name) {
  std::string search_name = name;

  // Strip "SIG" prefix if present
  if (search_name.size() > 3 && (search_name.substr(0, 3) == "SIG" ||
                                 search_name.substr(0, 3) == "sig")) {
    search_name = search_name.substr(3);
  }

  // Convert to uppercase for case-insensitive comparison
  for (char& c : search_name) {
    c = toupper(c);
  }

  // Look for matching signal
  for (const auto& signal : s_signal_table) {
    std::string signal_name = signal.name;
    if (signal_name.size() > 3 && signal_name.substr(0, 3) == "SIG") {
      signal_name = signal_name.substr(3);
    }

    if (signal_name == search_name) {
      return signal.signal;
    }
  }

  // Try parsing as a number
  try {
    return std::stoi(name);
  } catch (const std::exception&) {
    return -1;
  }
}

bool SignalHandler::is_forked_child() {
  return getpid() != s_main_pid;
}

void SignalHandler::signal_unblock_all() {
  sigset_t iset;
  sigemptyset(&iset);
  sigprocmask(SIG_SETMASK, &iset, nullptr);
}

void SignalHandler::signal_handler(int signum, siginfo_t* info, void* context) {
  (void)context;
  (void)info;

  // Check if we're in a forked child - if so, reset to default handler and
  // re-raise
  if (is_forked_child()) {
    // Use sigaction for atomic signal handler replacement to avoid race conditions
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(signum, &sa, nullptr);
    raise(signum);
    return;
  }

  // Only use async-signal-safe operations in signal handler
  // Defer complex operations and debugging output to process_pending_signals()

  // Check if this signal is being observed by scripts
  bool is_observed = is_signal_observed(signum);

  switch (signum) {
    case SIGINT: {
      s_sigint_received = 1;
      // Only write newline and handle terminal interrupt if signal is not being
      // observed by a script
      if (!is_observed) {
        ssize_t bytes_written = write(STDOUT_FILENO, "\n", 1);
        (void)bytes_written;
      }
      break;
    }

    case SIGCHLD: {
      s_sigchld_received = 1;
      break;
    }

    case SIGHUP: {
      s_sighup_received = 1;
      // Only exit if signal is not being observed
      if (!is_observed) {
        g_exit_flag = true;
      }
      break;
    }

    case SIGTERM: {
      s_sigterm_received = 1;
      // Only exit if signal is not being observed
      if (!is_observed) {
        g_exit_flag = true;
      }
      break;
    }

#ifdef SIGWINCH
    case SIGWINCH: {
      // Window size changed - simply let it be processed in process_pending_signals
      break;
    }
#endif
  }
}

void SignalHandler::setup_signal_handlers() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Setting up signal handlers." << std::endl;

  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sigset_t block_mask;
  sigfillset(&block_mask);

  // Common signal handlers for both interactive and non-interactive modes

  // Ignore SIGPIPE - we'll handle write errors explicitly
  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  sa.sa_mask = block_mask;
  sigaction(SIGPIPE, &sa, nullptr);

  // Always ignore terminal control signals to avoid suspension
  sigaction(SIGTTOU, &sa, &m_old_sigttou_handler);
  sigaction(SIGTTIN, &sa, &m_old_sigttin_handler);

  // SIGCHLD should be caught but not interrupt syscalls
  sa.sa_sigaction = signal_handler;
  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction(SIGCHLD, &sa, &m_old_sigchld_handler);

  // SIGINT handler (without SA_RESTART to interrupt syscalls)
  sa.sa_sigaction = signal_handler;
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGINT, &sa, &m_old_sigint_handler);

  // Handle SIGTERM and SIGHUP
  sa.sa_sigaction = signal_handler;
  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction(SIGHUP, &sa, &m_old_sighup_handler);
  sigaction(SIGTERM, &sa, &m_old_sigterm_handler);
}

void SignalHandler::setup_interactive_handlers() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Setting up interactive-specific signal handlers"
              << std::endl;

  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sigset_t block_mask;
  sigfillset(&block_mask);

  // For interactive mode, ignore job control signals
  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  sa.sa_mask = block_mask;

  // Ignore keyboard-generated signals in interactive mode
  sigaction(SIGQUIT, &sa, &m_old_sigquit_handler);  /* Ctrl+\ */
  sigaction(SIGTSTP, &sa, &m_old_sigtstp_handler);  // Ctrl+Z

#ifdef SIGWINCH
  // Catch window size changes in interactive mode
  sa.sa_sigaction = signal_handler;
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGWINCH, &sa, nullptr);
#endif
}

void SignalHandler::restore_original_handlers() {
  sigaction(SIGINT, &m_old_sigint_handler, nullptr);
  sigaction(SIGCHLD, &m_old_sigchld_handler, nullptr);
  sigaction(SIGHUP, &m_old_sighup_handler, nullptr);
  sigaction(SIGTERM, &m_old_sigterm_handler, nullptr);
  sigaction(SIGQUIT, &m_old_sigquit_handler, nullptr);
  sigaction(SIGTSTP, &m_old_sigtstp_handler, nullptr);
  sigaction(SIGTTIN, &m_old_sigttin_handler, nullptr);
  sigaction(SIGTTOU, &m_old_sigttou_handler, nullptr);
}

void SignalHandler::process_pending_signals(Exec* shell_exec) {
  if (g_debug_mode && (s_sigint_received || s_sigchld_received ||
                       s_sighup_received || s_sigterm_received)) {
    std::cerr << "DEBUG: Processing pending signals: "
              << "SIGINT=" << s_sigint_received << ", "
              << "SIGCHLD=" << s_sigchld_received << ", "
              << "SIGHUP=" << s_sighup_received << ", "
              << "SIGTERM=" << s_sigterm_received << std::endl;
  }

  // Check for all signals that might have been received
  if (s_sigint_received) {
    s_sigint_received = 0;
    
    if (g_debug_mode) {
      std::cerr << "DEBUG: Processing SIGINT signal" << std::endl;
    }

    // Check if SIGINT is being observed by scripts
    bool is_observed = is_signal_observed(SIGINT);

    // If not observed, propagate to foreground job
    if (!is_observed && shell_exec) {
      auto jobs = shell_exec->get_jobs();
      for (const auto& job_pair : jobs) {
        const auto& job = job_pair.second;
        if (!job.background && !job.completed && !job.stopped) {
          if (kill(-job.pgid, SIGINT) < 0) {
            perror("kill (SIGINT) in process_pending_signals");
          }
          break;
        }
      }
    }

    fflush(stdout);
  }

  if (s_sigchld_received) {
    s_sigchld_received = 0;

    if (g_debug_mode) {
      std::cerr << "DEBUG: Processing SIGCHLD signal" << std::endl;
    }

    if (shell_exec) {
      pid_t pid;
      int status;
      int reaped_count = 0;
      
      // Aggressively reap all available children to prevent zombies
      while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        reaped_count++;
        shell_exec->handle_child_signal(pid, status);

        // Also update JobManager for job control integration
        auto& job_manager = JobManager::instance();
        auto job = job_manager.get_job_by_pgid(pid);
        if (!job) {
          // Check if this PID is part of any job
          auto all_jobs = job_manager.get_all_jobs();
          for (auto& j : all_jobs) {
            auto it = std::find(j->pids.begin(), j->pids.end(), pid);
            if (it != j->pids.end()) {
              job = j;
              break;
            }
          }
        }

        if (job) {
          if (WIFEXITED(status) || WIFSIGNALED(status)) {
            job->state =
                WIFEXITED(status) ? JobState::DONE : JobState::TERMINATED;
            job->exit_status =
                WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status);

            // Remove finished PID from job
            job->pids.erase(
                std::remove(job->pids.begin(), job->pids.end(), pid),
                job->pids.end());

            // If all PIDs are done, mark job for cleanup
            if (job->pids.empty()) {
              job->notified = true;
            }
          } else if (WIFSTOPPED(status)) {
            job->state = JobState::STOPPED;
          } else if (WIFCONTINUED(status)) {
            job->state = JobState::RUNNING;
          }
        }
      }
      
      // Reduce debug output frequency to avoid performance impact
      if (g_debug_mode && reaped_count > 0 && reaped_count <= 3) {
        std::cerr << "DEBUG: SIGCHLD handler reaped " << reaped_count << " children" << std::endl;
      }
      
      // If waitpid returned -1 with ECHILD, all children have been reaped
      if (pid == -1 && errno == ECHILD && g_debug_mode) {
        static int echild_count = 0;
        if (++echild_count <= 5) {  // Limit this debug message
          std::cerr << "DEBUG: All children have been reaped" << std::endl;
        }
      }
    }
  }

  if (s_sighup_received) {
    s_sighup_received = 0;
    
    // Terminate all background jobs when SIGHUP is received
    if (shell_exec && g_exit_flag) {
      if (g_debug_mode) {
        std::cerr << "DEBUG: SIGHUP received, terminating background jobs" << std::endl;
      }
      shell_exec->terminate_all_child_process();
      
      // Also clean up through JobManager
      auto& job_manager = JobManager::instance();
      auto all_jobs = job_manager.get_all_jobs();
      for (auto& job : all_jobs) {
        if (job->state == JobState::RUNNING || job->state == JobState::STOPPED) {
          if (g_debug_mode) {
            std::cerr << "DEBUG: Terminating job " << job->job_id << " via JobManager" << std::endl;
          }
          if (killpg(job->pgid, SIGTERM) == 0) {
            job->state = JobState::TERMINATED;
          }
        }
      }
    }
  }

  if (s_sigterm_received) {
    s_sigterm_received = 0;
    
    // Terminate all background jobs when SIGTERM is received
    if (shell_exec && g_exit_flag) {
      if (g_debug_mode) {
        std::cerr << "DEBUG: SIGTERM received, terminating background jobs" << std::endl;
      }
      shell_exec->terminate_all_child_process();
      
      // Also clean up through JobManager
      auto& job_manager = JobManager::instance();
      auto all_jobs = job_manager.get_all_jobs();
      for (auto& job : all_jobs) {
        if (job->state == JobState::RUNNING || job->state == JobState::STOPPED) {
          if (g_debug_mode) {
            std::cerr << "DEBUG: Terminating job " << job->job_id << " via JobManager" << std::endl;
          }
          if (killpg(job->pgid, SIGTERM) == 0) {
            job->state = JobState::TERMINATED;
          }
        }
      }
    }
  }
}

void SignalHandler::observe_signal(int signum) {
  // Don't add duplicates
  if (!is_signal_observed(signum)) {
    s_observed_signals.push_back(signum);
    if (g_debug_mode) {
      std::cerr << "DEBUG: Signal " << signum << " (" << get_signal_name(signum)
                << ") is now being observed by scripts" << std::endl;
    }
  }
}

void SignalHandler::unobserve_signal(int signum) {
  auto it =
      std::find(s_observed_signals.begin(), s_observed_signals.end(), signum);
  if (it != s_observed_signals.end()) {
    s_observed_signals.erase(it);
    if (g_debug_mode) {
      std::cerr << "DEBUG: Signal " << signum << " (" << get_signal_name(signum)
                << ") is no longer being observed by scripts" << std::endl;
    }
  }
}

bool SignalHandler::is_signal_observed(int signum) {
  return std::find(s_observed_signals.begin(), s_observed_signals.end(),
                   signum) != s_observed_signals.end();
}

std::vector<int> SignalHandler::get_observed_signals() {
  return s_observed_signals;
}

#include "signal_handler.h"

#include "exec.h"
#include "main.h"

std::atomic<SignalHandler*> SignalHandler::s_instance(nullptr);
volatile sig_atomic_t SignalHandler::s_sigint_received = 0;
volatile sig_atomic_t SignalHandler::s_sigchld_received = 0;
volatile sig_atomic_t SignalHandler::s_sighup_received = 0;
volatile sig_atomic_t SignalHandler::s_sigterm_received = 0;
volatile sig_atomic_t SignalHandler::s_sigtstp_received = 0;
volatile sig_atomic_t SignalHandler::s_sigcont_received = 0;

SignalHandler* g_signal_handler = nullptr;

SignalHandler::SignalHandler() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Constructing SignalHandler" << std::endl;
  s_instance.store(this);
}

SignalHandler::~SignalHandler() {
  if (g_debug_mode) std::cerr << "DEBUG: Destroying SignalHandler" << std::endl;
  restore_original_handlers();
  s_instance.store(nullptr);
}

void SignalHandler::signal_handler(int signum, siginfo_t* info, void* context) {
  (void)context;
  (void)info;
  switch (signum) {
    case SIGINT: {
      s_sigint_received = 1;
      write(STDOUT_FILENO, "\n", 1);
      break;
    }

    case SIGCHLD: {
      s_sigchld_received = 1;
      break;
    }

    case SIGHUP: {
      s_sighup_received = 1;
      break;
    }

    case SIGTERM: {
      s_sigterm_received = 1;
      break;
    }

    case SIGTSTP: {
      s_sigtstp_received = 1;
      write(STDOUT_FILENO, "\n", 1);
      break;
    }

    case SIGCONT: {
      s_sigcont_received = 1;
      break;
    }
  }
}

void SignalHandler::setup_signal_handlers() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Setting up signal handlers" << std::endl;

  struct sigaction sa;
  memset(&sa, 0, sizeof(struct sigaction));
  sigemptyset(&sa.sa_mask);
  sigset_t block_mask;
  sigemptyset(&block_mask);

  sigfillset(&block_mask);

  sa.sa_sigaction = signal_handler;
  sa.sa_mask = block_mask;

  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction(SIGHUP, &sa, &m_old_sighup_handler);
  sigaction(SIGTERM, &sa, &m_old_sigterm_handler);
  sigaction(SIGCHLD, &sa, &m_old_sigchld_handler);
  sigaction(SIGCONT, &sa, &m_old_sigcont_handler);

  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGINT, &sa, &m_old_sigint_handler);
  sigaction(SIGTSTP, &sa, &m_old_sigtstp_handler);

  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  sigaction(SIGPIPE, &sa, &m_old_sigpipe_handler);

  sigaction(SIGQUIT, &sa, &m_old_sigquit_handler);
  sigaction(SIGTTIN, &sa, &m_old_sigttin_handler);
  sigaction(SIGTTOU, &sa, &m_old_sigttou_handler);
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
  sigaction(SIGCONT, &m_old_sigcont_handler, nullptr);
  sigaction(SIGPIPE, &m_old_sigpipe_handler, nullptr);
}

void SignalHandler::process_pending_signals(Exec* shell_exec) {
  if (g_debug_mode &&
      (s_sigint_received || s_sigchld_received || s_sigtstp_received ||
       s_sighup_received || s_sigterm_received || s_sigcont_received)) {
    std::cerr << "DEBUG: Processing pending signals: "
              << "SIGINT=" << s_sigint_received << ", "
              << "SIGCHLD=" << s_sigchld_received << ", "
              << "SIGTSTP=" << s_sigtstp_received << ", "
              << "SIGHUP=" << s_sighup_received << ", "
              << "SIGTERM=" << s_sigterm_received << ", "
              << "SIGCONT=" << s_sigcont_received << std::endl;
  }

  if (s_sigcont_received) {
    s_sigcont_received = 0;

    if (g_debug_mode) {
      std::cerr << "DEBUG: SIGCONT received, restoring terminal settings"
                << std::endl;
    }

    if (isatty(g_shell_terminal)) {
      if (tcsetpgrp(g_shell_terminal, g_shell_pgid) < 0) {
        if (errno != ENOTTY && errno != EINVAL) {
          std::cerr << "tcsetpgrp failed in SIGCONT handler" << std::endl;
        }
      }

      if (g_terminal_state_saved) {
        if (tcsetattr(g_shell_terminal, TCSADRAIN, &g_shell_tmodes) < 0) {
          std::cerr << "tcsetattr failed in SIGCONT handler" << std::endl;
        }
      }
    }

    if (g_shell) {
      g_shell->process_pending_signals();
    }
  }

  if (s_sighup_received || s_sigterm_received) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: Termination signal received, cleaning up"
                << std::endl;
    }

    if (shell_exec) {
      shell_exec->terminate_all_child_process();
    }

    s_sighup_received = 0;
    s_sigterm_received = 0;

    std::cout << "\nReceived termination signal. Exiting...\n";
    exit(0);
  }
  if (s_sigint_received) {
    s_sigint_received = 0;

    if (shell_exec) {
      auto jobs = shell_exec->get_jobs();
      for (const auto& job_pair : jobs) {
        const auto& job = job_pair.second;
        if (!job.background && !job.completed && !job.stopped) {
          if (kill(-job.pgid, SIGINT) < 0) {
            if (errno != ESRCH) {
              std::cerr << "kill (SIGINT) in process_pending_signals: "
                        << strerror(errno) << std::endl;
            }
          }
          break;
        }
      }
    }
    fflush(stdout);
  }
  if (s_sigtstp_received) {
    s_sigtstp_received = 0;

    if (shell_exec) {
      auto jobs = shell_exec->get_jobs();
      for (const auto& job_pair : jobs) {
        const auto& job = job_pair.second;
        if (!job.background && !job.completed && !job.stopped) {
          if (kill(-job.pgid, SIGTSTP) < 0) {
            if (errno != ESRCH) {
              std::cerr << "kill (SIGTSTP) in process_pending_signals: "
                        << strerror(errno) << std::endl;
            }
          }
          break;
        }
      }
    }
    fflush(stdout);
  }
  if (s_sigchld_received) {
    s_sigchld_received = 0;

    if (shell_exec) {
      sigset_t mask, prev_mask;
      sigemptyset(&mask);
      sigaddset(&mask, SIGCHLD);
      sigprocmask(SIG_BLOCK, &mask, &prev_mask);

      pid_t pid;
      int status;
      while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        shell_exec->handle_child_signal(pid, status);
      }
      sigprocmask(SIG_SETMASK, &prev_mask, nullptr);
    }
  }
}

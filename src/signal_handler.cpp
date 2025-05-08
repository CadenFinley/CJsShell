#include "signal_handler.h"

#include "exec.h"
#include "main.h"

// CADEN DONT TOUCH IT WORKS

std::atomic<SignalHandler*> SignalHandler::s_instance(nullptr);
volatile sig_atomic_t SignalHandler::s_sigint_received = 0;
volatile sig_atomic_t SignalHandler::s_sigchld_received = 0;
volatile sig_atomic_t SignalHandler::s_sighup_received = 0;
volatile sig_atomic_t SignalHandler::s_sigterm_received = 0;

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

  if (g_debug_mode) {
    std::cerr << "DEBUG: Signal received: " << signum << std::endl;
  }

  switch (signum) {
    case SIGINT: {
      s_sigint_received = 1;
      ssize_t bytes_written = write(STDOUT_FILENO, "\n", 1);
      (void)bytes_written;
      if (g_debug_mode)
        std::cerr << "DEBUG: SIGINT handler executed" << std::endl;
      break;
    }

    case SIGCHLD: {
      s_sigchld_received = 1;
      if (g_debug_mode)
        std::cerr << "DEBUG: SIGCHLD handler executed" << std::endl;
      break;
    }

    case SIGHUP: {
      s_sighup_received = 1;
      if (g_debug_mode)
        std::cerr << "DEBUG: SIGHUP handler executed, exiting" << std::endl;
      _exit(0);
      break;
    }

    case SIGTERM: {
      s_sigterm_received = 1;
      if (g_debug_mode)
        std::cerr << "DEBUG: SIGTERM handler executed, exiting" << std::endl;
      _exit(0);
      break;
    }
  }
}

void SignalHandler::setup_signal_handlers() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Setting up signal handlers" << std::endl;

  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sigset_t block_mask;
  sigfillset(&block_mask);

  sa.sa_sigaction = signal_handler;
  sa.sa_mask = block_mask;
  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction(SIGHUP, &sa, &m_old_sighup_handler);

  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction(SIGTERM, &sa, &m_old_sigterm_handler);

  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction(SIGCHLD, &sa, &m_old_sigchld_handler);

  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGINT, &sa, &m_old_sigint_handler);

  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  sa.sa_mask = block_mask;
  sigaction(SIGQUIT, &sa, &m_old_sigquit_handler);
  sigaction(SIGTSTP, &sa, &m_old_sigtstp_handler);
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
}

void SignalHandler::process_pending_signals(Exec* shell_exec) {
  if (g_debug_mode && (s_sigint_received || s_sigchld_received)) {
    std::cerr << "DEBUG: Processing pending signals: "
              << "SIGINT=" << s_sigint_received << ", "
              << "SIGCHLD=" << s_sigchld_received << std::endl;
  }

  if (s_sigint_received) {
    s_sigint_received = 0;

    if (shell_exec) {
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

    if (shell_exec) {
      pid_t pid;
      int status;
      while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        shell_exec->handle_child_signal(pid, status);
      }
    }
  }
}

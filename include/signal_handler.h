#pragma once

#include <signal.h>
#include <unistd.h>

#include <atomic>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

class Exec;

/// Struct describing a signal with its name and description
struct SignalInfo {
  int signal;               // Signal number
  const char* name;         // Signal name
  const char* description;  // Signal description
};

// Thread-safe signal event class to replace volatile sig_atomic_t
class SignalEvent {
 private:
  std::atomic<bool> received{false};

 public:
  void set() { received.store(true, std::memory_order_relaxed); }
  bool test_and_clear() {
    bool was_set = received.exchange(false, std::memory_order_relaxed);
    return was_set;
  }
  bool is_set() const { return received.load(std::memory_order_relaxed); }
};

class SignalHandler {
 public:
  SignalHandler();
  ~SignalHandler();

  void signal_unblock_all();

  /// Setup signal handlers
  void setup_signal_handlers();

  /// Setup interactive-specific signal handlers
  void setup_interactive_handlers();

  void process_pending_signals(Exec* shell_exec);

  /// Get the name of a signal
  static const char* get_signal_name(int signum);

  /// Get the description of a signal
  static const char* get_signal_description(int signum);

  /// Convert a signal name to signal number
  static int name_to_signal(const std::string& name);

  /// Check if this process is a forked child of the main shell
  static bool is_forked_child();

  /// Register a signal to be observed by scripts
  static void observe_signal(int signum);

  /// Unregister a signal from being observed by scripts
  static void unobserve_signal(int signum);

  /// Check if a signal is being observed by scripts
  static bool is_signal_observed(int signum);

  /// Get all currently observed signals
  static std::vector<int> get_observed_signals();

  static void signal_handler(int signum, siginfo_t* info, void* context);

 private:
  static std::atomic<SignalHandler*> s_instance;
  // Thread-safe signal events instead of volatile flags
  static SignalEvent s_sigint_event;
  static SignalEvent s_sigchld_event;
  static SignalEvent s_sighup_event;
  static SignalEvent s_sigterm_event;
  static SignalEvent s_sigwinch_event;
  static const std::vector<SignalInfo> s_signal_table;
  static pid_t s_main_pid;
  static std::vector<int>
      s_observed_signals;  // Signals being observed by scripts

  struct sigaction m_old_sigint_handler;
  struct sigaction m_old_sigchld_handler;
  struct sigaction m_old_sighup_handler;
  struct sigaction m_old_sigterm_handler;
  struct sigaction m_old_sigquit_handler;
  struct sigaction m_old_sigtstp_handler;
  struct sigaction m_old_sigttin_handler;
  struct sigaction m_old_sigttou_handler;

  void restore_original_handlers();
};
extern SignalHandler* g_signal_handler;

/*
  signal_handler.h

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#pragma once

#include <signal.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class Exec;

struct SignalProcessingResult {
    bool sigint = false;
    bool sighup = false;
    bool sigterm = false;
    std::vector<int> trapped_signals;
};

class SignalMask {
   private:
    sigset_t old_mask{};
    bool active;

   public:
    explicit SignalMask(int signum);

    explicit SignalMask(const std::vector<int>& signals);

    ~SignalMask();

    SignalMask(const SignalMask&) = delete;
    SignalMask& operator=(const SignalMask&) = delete;
};
enum class SignalDisposition : std::uint8_t {
    DEFAULT,
    IGNORE,
    TRAPPED,
    SYSTEM
};

struct SignalInfo {
    int signal;
    const char* name;
    const char* description;
    bool can_trap;
    bool can_ignore;
};

struct SignalState {
    SignalDisposition disposition = SignalDisposition::DEFAULT;
    struct sigaction original_action{};
    volatile sig_atomic_t pending_count = 0;
    bool is_blocked = false;
};

class SignalHandler {
   public:
    SignalHandler();
    ~SignalHandler();

    void signal_unblock_all();
    void setup_signal_handlers();
    void setup_interactive_handlers();

    SignalProcessingResult process_pending_signals(Exec* shell_exec);
    static bool has_pending_signals();
    static const std::vector<SignalInfo>& available_signals();

    static int name_to_signal(const std::string& name);
    static bool is_valid_signal(int signum);
    static bool can_trap_signal(int signum);
    static bool can_ignore_signal(int signum);
    static bool is_forked_child();

    static void set_signal_disposition(int signum, SignalDisposition disp,
                                       const std::string& trap_command = "");
    static void ignore_signal(int signum);

    static void observe_signal(int signum);
    static void unobserve_signal(int signum);
    static bool is_signal_observed(int signum);

    static void signal_handler(int signum, siginfo_t* info, void* context);

    static sigset_t get_current_mask();

   private:
    static std::atomic<SignalHandler*> s_instance;

    static volatile sig_atomic_t s_sigint_received;
    static volatile sig_atomic_t s_sigchld_received;
    static volatile sig_atomic_t s_sighup_received;
    static volatile sig_atomic_t s_sigterm_received;
    static volatile sig_atomic_t s_sigquit_received;
    static volatile sig_atomic_t s_sigtstp_received;
    static volatile sig_atomic_t s_sigusr1_received;
    static volatile sig_atomic_t s_sigusr2_received;
    static volatile sig_atomic_t s_sigabrt_received;
    static volatile sig_atomic_t s_sigalrm_received;
    static volatile sig_atomic_t s_sigwinch_received;
    static volatile sig_atomic_t s_sigpipe_received;
    static volatile sig_atomic_t s_sigttin_received;
    static volatile sig_atomic_t s_sigttou_received;
    static volatile sig_atomic_t s_sigcont_received;

    static std::atomic<bool> s_signal_pending;
    static const std::vector<SignalInfo>& signal_table();
    static pid_t s_main_pid;

    static std::unordered_map<int, SignalState> s_signal_states;
    static std::vector<int> s_observed_signals;

    struct sigaction m_old_sigint_handler;
    struct sigaction m_old_sigchld_handler;
    struct sigaction m_old_sighup_handler;
    struct sigaction m_old_sigterm_handler;
    struct sigaction m_old_sigquit_handler;
    struct sigaction m_old_sigtstp_handler;
    struct sigaction m_old_sigttin_handler;
    struct sigaction m_old_sigttou_handler;
    struct sigaction m_old_sigusr1_handler;
    struct sigaction m_old_sigusr2_handler;
    struct sigaction m_old_sigalrm_handler;
    struct sigaction m_old_sigwinch_handler;
    struct sigaction m_old_sigpipe_handler;

    void restore_original_handlers();
    static void install_signal_handler(int signum, struct sigaction* old_action);
    static void process_trapped_signal(int signum);
};

extern SignalHandler* g_signal_handler;

void reset_child_signals();

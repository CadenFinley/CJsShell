#pragma once

#include <signal.h>

#include <atomic>
#include <functional>
#include <string>
#include <vector>

class Exec;

class SignalMask {
   private:
    sigset_t old_mask;
    bool active;

   public:
    SignalMask(int signum) : active(false) {
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, signum);
        if (sigprocmask(SIG_BLOCK, &mask, &old_mask) == 0) {
            active = true;
        }
    }

    SignalMask(const std::vector<int>& signals) : active(false) {
        if (signals.empty())
            return;
        sigset_t mask;
        sigemptyset(&mask);
        for (int sig : signals) {
            sigaddset(&mask, sig);
        }
        if (sigprocmask(SIG_BLOCK, &mask, &old_mask) == 0) {
            active = true;
        }
    }

    ~SignalMask() {
        if (active) {
            sigprocmask(SIG_SETMASK, &old_mask, nullptr);
        }
    }

    SignalMask(const SignalMask&) = delete;
    SignalMask& operator=(const SignalMask&) = delete;
};

struct SignalInfo {
    int signal;
    const char* name;
    const char* description;
};

class SignalHandler {
   public:
    SignalHandler();
    ~SignalHandler();

    void signal_unblock_all();
    void setup_signal_handlers();
    void setup_interactive_handlers();
    void process_pending_signals(Exec* shell_exec);
    static const char* get_signal_name(int signum);
    static const char* get_signal_description(int signum);
    static int name_to_signal(const std::string& name);
    static bool is_forked_child();
    static void observe_signal(int signum);
    static void unobserve_signal(int signum);
    static bool is_signal_observed(int signum);
    static std::vector<int> get_observed_signals();

    static void signal_handler(int signum, siginfo_t* info, void* context);

   private:
    static std::atomic<SignalHandler*> s_instance;
    static volatile sig_atomic_t s_sigint_received;
    static volatile sig_atomic_t s_sigchld_received;
    static volatile sig_atomic_t s_sighup_received;
    static volatile sig_atomic_t s_sigterm_received;
    static const std::vector<SignalInfo> s_signal_table;
    static pid_t s_main_pid;
    static std::vector<int> s_observed_signals;

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

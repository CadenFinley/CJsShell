#pragma once

#include <signal.h>
#include <unistd.h>
#include <iostream>
#include <functional>
#include <atomic>
#include <cstring>

class Exec;

class SignalHandler {
public:
    SignalHandler();
    ~SignalHandler();

    void setup_signal_handlers();
    void process_pending_signals(Exec* shell_exec);
    
    static void signal_handler(int signum, siginfo_t* info, void* context);

private:
    static std::atomic<SignalHandler*> s_instance;
    
    static volatile sig_atomic_t s_sigint_received;
    static volatile sig_atomic_t s_sigchld_received;
    static volatile sig_atomic_t s_sighup_received;
    static volatile sig_atomic_t s_sigterm_received;
    static volatile sig_atomic_t s_sigtstp_received;
    static volatile sig_atomic_t s_sigcont_received;
    
    struct sigaction m_old_sigint_handler;
    struct sigaction m_old_sigchld_handler;
    struct sigaction m_old_sighup_handler;
    struct sigaction m_old_sigterm_handler;
    struct sigaction m_old_sigquit_handler;
    struct sigaction m_old_sigtstp_handler;
    struct sigaction m_old_sigttin_handler;
    struct sigaction m_old_sigttou_handler;
    struct sigaction m_old_sigcont_handler;
    struct sigaction m_old_sigpipe_handler;

    void restore_original_handlers();
};
extern SignalHandler* g_signal_handler;

#pragma once

#include <signal.h>

#include <atomic>
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
    explicit SignalMask(int signum) : active(false) {
        sigset_t mask{};
        sigemptyset(&mask);
        sigaddset(&mask, signum);
        if (sigprocmask(SIG_BLOCK, &mask, &old_mask) == 0) {
            active = true;
        }
    }

    explicit SignalMask(const std::vector<int>& signals) : active(false) {
        if (signals.empty())
            return;
        sigset_t mask{};
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

enum class SignalDisposition {
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

    static const char* get_signal_name(int signum);
    static const char* get_signal_description(int signum);
    static int name_to_signal(const std::string& name);
    static bool is_valid_signal(int signum);
    static bool can_trap_signal(int signum);
    static bool can_ignore_signal(int signum);

    static void set_signal_disposition(int signum, SignalDisposition disp,
                                       const std::string& trap_command = "");
    static SignalDisposition get_signal_disposition(int signum);
    static void reset_signal_to_default(int signum);
    static void ignore_signal(int signum);

    static void block_signal(int signum);
    static void unblock_signal(int signum);
    static bool is_signal_blocked(int signum);
    static void block_all_trappable_signals();
    static void unblock_all_signals();

    static bool is_forked_child();
    static void reset_signals_for_child();
    static void apply_signal_state_for_exec();

    static void observe_signal(int signum);
    static void unobserve_signal(int signum);
    static bool is_signal_observed(int signum);
    static std::vector<int> get_observed_signals();

    static void signal_handler(int signum, siginfo_t* info, void* context);

    static sigset_t get_current_mask();
    static std::vector<int> get_blocked_signals();

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
    static volatile sig_atomic_t s_sigalrm_received;
    static volatile sig_atomic_t s_sigwinch_received;
    static volatile sig_atomic_t s_sigpipe_received;

    static std::atomic<bool> s_signal_pending;
    static const std::vector<SignalInfo> s_signal_table;
    static pid_t s_main_pid;

    static std::unordered_map<int, SignalState> s_signal_states;
    static std::vector<int> s_observed_signals;
    static sigset_t s_blocked_mask;

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

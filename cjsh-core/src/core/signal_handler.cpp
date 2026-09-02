/*
  signal_handler.cpp

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

#include "signal_handler.h"

#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>

#include "error_out.h"
#include "exec.h"
// #include "isocline.h"
#include "job_control.h"
#include "shell.h"
#include "shell_env.h"
#include "trap_command.h"

SignalMask::SignalMask(int signum) : active(false) {
    sigset_t mask{};
    sigemptyset(&mask);
    sigaddset(&mask, signum);
    if (sigprocmask(SIG_BLOCK, &mask, &old_mask) == 0) {
        active = true;
    }
}

SignalMask::SignalMask(const std::vector<int>& signals) : active(false) {
    if (signals.empty()) {
        return;
    }
    sigset_t mask{};
    sigemptyset(&mask);
    for (int sig : signals) {
        sigaddset(&mask, sig);
    }
    if (sigprocmask(SIG_BLOCK, &mask, &old_mask) == 0) {
        active = true;
    }
}

SignalMask::~SignalMask() {
    if (active) {
        (void)sigprocmask(SIG_SETMASK, &old_mask, nullptr);
    }
}

std::atomic<SignalHandler*> SignalHandler::s_instance(nullptr);

SignalHandler* SignalHandler::instance() {
    return s_instance.load(std::memory_order_acquire);
}

volatile sig_atomic_t SignalHandler::s_sigint_received = 0;
volatile sig_atomic_t SignalHandler::s_sigchld_received = 0;
volatile sig_atomic_t SignalHandler::s_sighup_received = 0;
volatile sig_atomic_t SignalHandler::s_sigterm_received = 0;
volatile sig_atomic_t SignalHandler::s_sigquit_received = 0;
volatile sig_atomic_t SignalHandler::s_sigtstp_received = 0;
volatile sig_atomic_t SignalHandler::s_sigusr1_received = 0;
volatile sig_atomic_t SignalHandler::s_sigusr2_received = 0;
volatile sig_atomic_t SignalHandler::s_sigabrt_received = 0;
volatile sig_atomic_t SignalHandler::s_sigalrm_received = 0;
volatile sig_atomic_t SignalHandler::s_sigwinch_received = 0;
volatile sig_atomic_t SignalHandler::s_sigpipe_received = 0;
volatile sig_atomic_t SignalHandler::s_sigttin_received = 0;
volatile sig_atomic_t SignalHandler::s_sigttou_received = 0;
volatile sig_atomic_t SignalHandler::s_sigcont_received = 0;

std::atomic<bool> SignalHandler::s_signal_pending(false);
pid_t SignalHandler::s_main_pid = 0;
std::vector<int> SignalHandler::s_observed_signals;
std::unordered_map<int, SignalState> SignalHandler::s_signal_states;

const std::vector<SignalInfo>& SignalHandler::signal_table() {
    static const std::vector<SignalInfo> kSignalTable = {
#ifdef SIGHUP
        {SIGHUP, "SIGHUP", "Terminal hung up", true, true},
#endif
#ifdef SIGINT
        {SIGINT, "SIGINT", "Interrupt (Ctrl+C)", true, true},
#endif
#ifdef SIGQUIT
        {SIGQUIT, "SIGQUIT", "Quit with core dump (Ctrl+\\)", true, true},
#endif
#ifdef SIGILL
        {SIGILL, "SIGILL", "Illegal instruction", true, true},
#endif
#ifdef SIGTRAP
        {SIGTRAP, "SIGTRAP", "Trace/breakpoint trap", true, true},
#endif
#ifdef SIGABRT
        {SIGABRT, "SIGABRT", "Abort", true, true},
#endif
#ifdef SIGBUS
        {SIGBUS, "SIGBUS", "Bus error (misaligned address)", true, true},
#endif
#ifdef SIGFPE
        {SIGFPE, "SIGFPE", "Floating point exception", true, true},
#endif
#ifdef SIGKILL
        {SIGKILL, "SIGKILL", "Kill (cannot be caught or ignored)", false, false},
#endif
#ifdef SIGUSR1
        {SIGUSR1, "SIGUSR1", "User-defined signal 1", true, true},
#endif
#ifdef SIGUSR2
        {SIGUSR2, "SIGUSR2", "User-defined signal 2", true, true},
#endif
#ifdef SIGSEGV
        {SIGSEGV, "SIGSEGV", "Segmentation violation", true, true},
#endif
#ifdef SIGPIPE
        {SIGPIPE, "SIGPIPE", "Broken pipe", true, true},
#endif
#ifdef SIGALRM
        {SIGALRM, "SIGALRM", "Alarm clock", true, true},
#endif
#ifdef SIGTERM
        {SIGTERM, "SIGTERM", "Termination", true, true},
#endif
#ifdef SIGCHLD
        {SIGCHLD, "SIGCHLD", "Child status changed", true, true},
#endif
#ifdef SIGCONT
        {SIGCONT, "SIGCONT", "Continue executing if stopped", true, true},
#endif
#ifdef SIGSTOP
        {SIGSTOP, "SIGSTOP", "Stop executing (cannot be caught or ignored)", false, false},
#endif
#ifdef SIGTSTP
        {SIGTSTP, "SIGTSTP", "Terminal stop (Ctrl+Z)", true, true},
#endif
#ifdef SIGTTIN
        {SIGTTIN, "SIGTTIN", "Background process trying to read from TTY", true, true},
#endif
#ifdef SIGTTOU
        {SIGTTOU, "SIGTTOU", "Background process trying to write to TTY", true, true},
#endif
#ifdef SIGURG
        {SIGURG, "SIGURG", "Urgent condition on socket", true, true},
#endif
#ifdef SIGXCPU
        {SIGXCPU, "SIGXCPU", "CPU time limit exceeded", true, true},
#endif
#ifdef SIGXFSZ
        {SIGXFSZ, "SIGXFSZ", "File size limit exceeded", true, true},
#endif
#ifdef SIGVTALRM
        {SIGVTALRM, "SIGVTALRM", "Virtual timer expired", true, true},
#endif
#ifdef SIGPROF
        {SIGPROF, "SIGPROF", "Profiling timer expired", true, true},
#endif
#ifdef SIGWINCH
        {SIGWINCH, "SIGWINCH", "Window size change", true, true},
#endif
#ifdef SIGIO
        {SIGIO, "SIGIO", "I/O now possible", true, true},
#endif
#ifdef SIGPWR
        {SIGPWR, "SIGPWR", "Power failure restart", true, true},
#endif
#ifdef SIGSYS
        {SIGSYS, "SIGSYS", "Bad system call", true, true},
#endif
#ifdef SIGINFO
        {SIGINFO, "SIGINFO", "Information request", true, true},
#endif
    };

    return kSignalTable;
}

const std::vector<SignalInfo>& SignalHandler::available_signals() {
    return signal_table();
}

SignalHandler::SignalHandler()

    : m_old_sigint_handler(),
      m_old_sigchld_handler(),
      m_old_sighup_handler(),
      m_old_sigterm_handler(),
      m_old_sigquit_handler(),
      m_old_sigtstp_handler(),
      m_old_sigttin_handler(),
      m_old_sigttou_handler(),
      m_old_sigusr1_handler(),
      m_old_sigusr2_handler(),
      m_old_sigalrm_handler(),
      m_old_sigwinch_handler(),
      m_old_sigpipe_handler() {
    signal_unblock_all();
    s_instance.store(this);
}

void reset_child_signals() {
    (void)signal(SIGINT, SIG_DFL);
    (void)signal(SIGQUIT, SIG_DFL);
    (void)signal(SIGTSTP, SIG_DFL);
    (void)signal(SIGTTIN, SIG_DFL);
    (void)signal(SIGTTOU, SIG_DFL);
    (void)signal(SIGCHLD, SIG_DFL);
    (void)signal(SIGTERM, SIG_DFL);
    (void)signal(SIGHUP, SIG_DFL);
    (void)signal(SIGPIPE, SIG_DFL);

#ifdef SIGUSR1
    (void)signal(SIGUSR1, SIG_DFL);
#endif
#ifdef SIGUSR2
    (void)signal(SIGUSR2, SIG_DFL);
#endif
#ifdef SIGALRM
    (void)signal(SIGALRM, SIG_DFL);
#endif
#ifdef SIGWINCH
    (void)signal(SIGWINCH, SIG_DFL);
#endif

    sigset_t set{};
    sigemptyset(&set);
    (void)sigprocmask(SIG_SETMASK, &set, nullptr);
}

bool SignalHandler::has_direct_pending_signal() {
    return s_sigint_received != 0 || s_sigchld_received != 0 || s_sighup_received != 0 ||
           s_sigterm_received != 0 || s_sigttin_received != 0 || s_sigttou_received != 0 ||
           s_sigquit_received != 0 || s_sigtstp_received != 0 || s_sigusr1_received != 0 ||
           s_sigusr2_received != 0 || s_sigabrt_received != 0 || s_sigalrm_received != 0 ||
           s_sigcont_received != 0 || s_sigwinch_received != 0 || s_sigpipe_received != 0;
}

bool SignalHandler::has_pending_signals() {
    return s_signal_pending.load(std::memory_order_acquire) || has_direct_pending_signal();
}

SignalHandler::~SignalHandler() {
    restore_original_handlers();
    s_instance.store(nullptr);
}

int SignalHandler::name_to_signal(const std::string& name) {
    std::string search_name = name;

    if (search_name.size() > 3 &&
        (search_name.substr(0, 3) == "SIG" || search_name.substr(0, 3) == "sig")) {
        search_name = search_name.substr(3);
    }

    for (char& c : search_name) {
        c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    }

    for (const auto& signal : signal_table()) {
        std::string signal_name = signal.name;
        if (signal_name.size() > 3 && signal_name.substr(0, 3) == "SIG") {
            signal_name = signal_name.substr(3);
        }

        if (signal_name == search_name) {
            return signal.signal;
        }
    }

    try {
        return std::stoi(name);
    } catch (const std::exception&) {
        return -1;
    }
}

int SignalHandler::parse_trap_signal_token(const std::string& token) {
    std::string search_name = token;
    for (char& c : search_name) {
        c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    }

    if (search_name == "EXIT" || token == "0") {
        return 0;
    }
    if (search_name == "ERR") {
        return -2;
    }
    if (search_name == "DEBUG") {
        return -3;
    }
    if (search_name == "RETURN") {
        return -4;
    }

    int signal = name_to_signal(search_name);
    if (signal <= 0 && token != "0") {
        return -1;
    }
    if (signal == 0) {
        return 0;
    }
    if (!is_valid_signal(signal)) {
        return -1;
    }
    return signal;
}

std::string SignalHandler::signal_to_name(int signum, bool strip_sig_prefix) {
    for (const auto& signal : signal_table()) {
        if (signal.signal != signum || signal.name == nullptr) {
            continue;
        }

        std::string name(signal.name);
        if (strip_sig_prefix && name.rfind("SIG", 0) == 0) {
            return name.substr(3);
        }
        return name;
    }

    return std::to_string(signum);
}

std::vector<std::pair<int, std::string>> SignalHandler::trap_signal_names() {
    std::vector<std::pair<int, std::string>> names;
    const auto& table = signal_table();
    names.reserve(table.size() + 4);
    for (const auto& signal : table) {
        if (signal.name == nullptr) {
            continue;
        }
        (void)names.emplace_back(signal.signal, signal_to_name(signal.signal, true));
    }

    (void)names.emplace_back(0, "EXIT");
    (void)names.emplace_back(-2, "ERR");
    (void)names.emplace_back(-3, "DEBUG");
    (void)names.emplace_back(-4, "RETURN");
    return names;
}

bool SignalHandler::is_valid_signal(int signum) {
    if (signum <= 0) {
        return false;
    }
    const auto& table = signal_table();
    return std::any_of(table.begin(), table.end(),
                       [signum](const auto& signal_info) { return signal_info.signal == signum; });
}

bool SignalHandler::can_trap_signal(int signum) {
    const auto& table = signal_table();
    return std::any_of(table.begin(), table.end(), [signum](const auto& signal) {
        return signal.signal == signum && signal.can_trap;
    });
}

bool SignalHandler::can_ignore_signal(int signum) {
    const auto& table = signal_table();
    return std::any_of(table.begin(), table.end(), [signum](const auto& signal) {
        return signal.signal == signum && signal.can_ignore;
    });
}

bool SignalHandler::is_forked_child() {
    pid_t current_pid = getpid();
    if (s_main_pid == 0) {
        s_main_pid = current_pid;
        return false;
    }
    return current_pid != s_main_pid;
}

void SignalHandler::signal_unblock_all() {
    sigset_t iset{};
    sigemptyset(&iset);
    (void)sigprocmask(SIG_SETMASK, &iset, nullptr);
}

void SignalHandler::set_signal_disposition(int signum, SignalDisposition disp, const std::string&) {
    if (!is_valid_signal(signum)) {
        return;
    }

    if (signum == SIGKILL || signum == SIGSTOP) {
        return;
    }

    SignalState& state = s_signal_states[signum];
    struct sigaction sa{};
    sigemptyset(&sa.sa_mask);

    switch (disp) {
        case SignalDisposition::DEFAULT: {
            sa.sa_handler = SIG_DFL;
            sa.sa_flags = 0;
            if (sigaction(signum, &sa, nullptr) == 0) {
                state.disposition = SignalDisposition::DEFAULT;
                unobserve_signal(signum);
            }
            break;
        }

        case SignalDisposition::IGNORE: {
            if (!can_ignore_signal(signum)) {
                return;
            }
            sa.sa_handler = SIG_IGN;
            sa.sa_flags = 0;
            if (sigaction(signum, &sa, nullptr) == 0) {
                state.disposition = SignalDisposition::IGNORE;
                unobserve_signal(signum);
            }
            break;
        }

        case SignalDisposition::TRAPPED: {
            if (!can_trap_signal(signum)) {
                return;
            }

            struct sigaction old_action{};
            install_signal_handler(signum, &old_action);
            state.original_action = old_action;
            state.disposition = SignalDisposition::TRAPPED;
            observe_signal(signum);

            break;
        }

        case SignalDisposition::SYSTEM: {
            struct sigaction sys_old_action{};
            install_signal_handler(signum, &sys_old_action);
            state.original_action = sys_old_action;
            state.disposition = SignalDisposition::SYSTEM;
            break;
        }
    }
}

void SignalHandler::ignore_signal(int signum) {
    set_signal_disposition(signum, SignalDisposition::IGNORE);
}

void SignalHandler::install_signal_handler(int signum, struct sigaction* old_action) {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigfillset(&sa.sa_mask);

    sa.sa_flags = 0;

    if (signum != SIGINT) {
        sa.sa_flags |= SA_RESTART;
    }

    (void)sigaction(signum, &sa, old_action);
}

void SignalHandler::process_trapped_signal(int signum) {
    if (trap_manager_has_trap(signum)) {
        trap_manager_execute_trap(signum);
    }
}

void SignalHandler::signal_handler(int signum) {
    if (is_forked_child()) {
        struct sigaction sa{};
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        (void)sigaction(signum, &sa, nullptr);
        (void)raise(signum);
        return;
    }

    bool is_observed = is_signal_observed(signum);
    bool should_mark_pending = is_observed;

    switch (signum) {
        case SIGINT: {
            s_sigint_received = 1;

            if (!is_observed) {
                if (!config::interactive_mode) {
                    cjsh_env::request_exit();
                    _exit(128 + SIGINT);
                }
            }
            should_mark_pending = true;
            break;
        }

        case SIGCHLD: {
            s_sigchld_received = 1;
            should_mark_pending = true;
            break;
        }

        case SIGHUP: {
            s_sighup_received = 1;
            cjsh_env::request_exit();
            pid_t bg_pgid = JobManager::get_last_background_pid_atomic();
            if (bg_pgid > 0) {
                (void)killpg(bg_pgid, SIGHUP);
            }
            should_mark_pending = true;
            break;
        }

        case SIGTERM: {
            s_sigterm_received = 1;
            cjsh_env::request_exit();

            if (!is_observed) {
                _exit(128 + SIGTERM);
            }
            should_mark_pending = true;
            break;
        }

        case SIGQUIT: {
            s_sigquit_received = 1;

            if (!is_observed) {
                if (!config::interactive_mode) {
                    _exit(128 + SIGQUIT);
                }
            }
            break;
        }

        case SIGTSTP: {
            s_sigtstp_received = 1;
            should_mark_pending = true;

            if (!config::interactive_mode && !is_observed) {
                // In orphaned process groups (common in CI), default SIGTSTP can be discarded.
                // Force a real stop to preserve expected shell behavior for non-interactive runs.
                (void)kill(getpid(), SIGSTOP);
            }

            break;
        }

#ifdef SIGUSR1
        case SIGUSR1: {
            s_sigusr1_received = 1;
            break;
        }
#endif

#ifdef SIGUSR2
        case SIGUSR2: {
            s_sigusr2_received = 1;
            break;
        }
#endif

#ifdef SIGABRT
        case SIGABRT: {
            s_sigabrt_received = 1;
            break;
        }
#endif

#ifdef SIGCONT
        case SIGCONT: {
            s_sigcont_received = 1;
            should_mark_pending = true;
            break;
        }
#endif

#ifdef SIGALRM
        case SIGALRM: {
            s_sigalrm_received = 1;
            break;
        }
#endif

#ifdef SIGWINCH
        case SIGWINCH: {
            s_sigwinch_received = 1;
            // this is currently disabled as this lets isocline know about terminal resizing, which
            // is currently broken.
            // ic_notify_resize();

            break;
        }
#endif

#ifdef SIGPIPE
        case SIGPIPE: {
            s_sigpipe_received = 1;

            if (!is_observed) {
                return;
            }
            break;
        }
#endif

#ifdef SIGTTIN
        case SIGTTIN: {
            s_sigttin_received = 1;
            break;
        }
#endif

#ifdef SIGTTOU
        case SIGTTOU: {
            s_sigttou_received = 1;
            break;
        }
#endif

        default: {
            break;
        }
    }

    if (should_mark_pending) {
        s_signal_pending.store(true, std::memory_order_release);
    }
}

void SignalHandler::setup_signal_handlers() {
    struct sigaction sa{};
    sigemptyset(&sa.sa_mask);
    sigset_t block_mask{};
    sigfillset(&block_mask);

    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sa.sa_mask = block_mask;
    (void)sigaction(SIGPIPE, &sa, &m_old_sigpipe_handler);

    s_signal_states[SIGPIPE].disposition = SignalDisposition::IGNORE;

    (void)sigaction(SIGTTOU, &sa, &m_old_sigttou_handler);
    (void)sigaction(SIGTTIN, &sa, &m_old_sigttin_handler);

    install_signal_handler(SIGCHLD, &m_old_sigchld_handler);
    install_signal_handler(SIGINT, &m_old_sigint_handler);
    install_signal_handler(SIGHUP, &m_old_sighup_handler);
    install_signal_handler(SIGTERM, &m_old_sigterm_handler);

#ifdef SIGTSTP
    if (!config::interactive_mode) {
        install_signal_handler(SIGTSTP, &m_old_sigtstp_handler);
        s_signal_states[SIGTSTP].disposition = SignalDisposition::SYSTEM;
    }
#endif

    s_signal_states[SIGCHLD].disposition = SignalDisposition::SYSTEM;
    s_signal_states[SIGHUP].disposition = SignalDisposition::SYSTEM;
    s_signal_states[SIGTERM].disposition = SignalDisposition::SYSTEM;

    s_signal_states[SIGINT].disposition = SignalDisposition::SYSTEM;
}

void SignalHandler::setup_interactive_handlers() {
    struct sigaction sa{};
    sigemptyset(&sa.sa_mask);
    sigset_t block_mask{};
    sigfillset(&block_mask);

    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sa.sa_mask = block_mask;

    (void)sigaction(SIGQUIT, &sa, &m_old_sigquit_handler);
    (void)sigaction(SIGTSTP, &sa, &m_old_sigtstp_handler);

#ifdef SIGWINCH

    install_signal_handler(SIGWINCH, &m_old_sigwinch_handler);
    s_signal_states[SIGWINCH].disposition = SignalDisposition::SYSTEM;
#endif

#ifdef SIGUSR1

    (void)sigaction(SIGUSR1, nullptr, &m_old_sigusr1_handler);
#endif

#ifdef SIGUSR2
    (void)sigaction(SIGUSR2, nullptr, &m_old_sigusr2_handler);
#endif

#ifdef SIGALRM
    (void)sigaction(SIGALRM, nullptr, &m_old_sigalrm_handler);
#endif
}

void SignalHandler::restore_original_handlers() {
    (void)sigaction(SIGINT, &m_old_sigint_handler, nullptr);
    (void)sigaction(SIGCHLD, &m_old_sigchld_handler, nullptr);
    (void)sigaction(SIGHUP, &m_old_sighup_handler, nullptr);
    (void)sigaction(SIGTERM, &m_old_sigterm_handler, nullptr);
    (void)sigaction(SIGQUIT, &m_old_sigquit_handler, nullptr);
    (void)sigaction(SIGTSTP, &m_old_sigtstp_handler, nullptr);
    (void)sigaction(SIGTTIN, &m_old_sigttin_handler, nullptr);
    (void)sigaction(SIGTTOU, &m_old_sigttou_handler, nullptr);
    (void)sigaction(SIGPIPE, &m_old_sigpipe_handler, nullptr);

#ifdef SIGUSR1
    (void)sigaction(SIGUSR1, &m_old_sigusr1_handler, nullptr);
#endif

#ifdef SIGUSR2
    (void)sigaction(SIGUSR2, &m_old_sigusr2_handler, nullptr);
#endif

#ifdef SIGALRM
    (void)sigaction(SIGALRM, &m_old_sigalrm_handler, nullptr);
#endif

#ifdef SIGWINCH
    (void)sigaction(SIGWINCH, &m_old_sigwinch_handler, nullptr);
#endif
}

SignalProcessingResult SignalHandler::process_pending_signals(Exec* shell_exec) {
    bool should_process = s_signal_pending.exchange(false, std::memory_order_acq_rel);
    if (!should_process && !has_direct_pending_signal()) {
        return {};
    }

    SignalProcessingResult result{};

    if (s_sigint_received != 0) {
        s_sigint_received = 0;
        result.sigint = true;

        bool is_observed = is_signal_observed(SIGINT);

        if (!is_observed && (shell_exec != nullptr)) {
            auto jobs = shell_exec->get_jobs();
            for (const auto& job_pair : jobs) {
                const auto& job = job_pair.second;
                if (!job.background && !job.completed && !job.stopped) {
                    if (kill(-job.pgid, SIGINT) < 0) {
                        print_error_errno({ErrorType::RUNTIME_ERROR,
                                           "signal",
                                           "kill SIGINT in process_pending_signals",
                                           {}});
                    }
                    break;
                }
            }
        }

        (void)fflush(stdout);
    }

    if (s_sigchld_received != 0) {
        s_sigchld_received = 0;

        if (shell_exec != nullptr) {
            pid_t pid = 0;
            int status = 0;
            int reaped_count = 0;
            const int max_reap_iterations = 100;

            // if (s_sigchld_received == 1) {
            // usleep(1000);
            // }

            while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0 &&
                   reaped_count < max_reap_iterations) {
                reaped_count++;
                shell_exec->handle_child_signal(pid, status);
                JobManager::instance().handle_child_status(pid, status);
            }

            if (reaped_count >= max_reap_iterations) {
                print_error({ErrorType::RUNTIME_ERROR,
                             ErrorSeverity::WARNING,
                             "signal-handler",
                             "SIGCHLD handler hit maximum iteration limit (" +
                                 std::to_string(max_reap_iterations) +
                                 "); breaking to prevent infinite loop",
                             {"Investigate stuck child processes or signal storms."}});
            }
        }
    }

    if (s_sighup_received != 0) {
        s_sighup_received = 0;
        result.sighup = true;
        cjsh_env::request_exit();

        bool enforce_hup = !g_shell || g_shell->get_shell_option(ShellOption::Huponexit);

        auto& job_manager = JobManager::instance();
        auto jobs_snapshot = job_manager.get_all_jobs();

        for (const auto& job : jobs_snapshot) {
            const JobState state = job->state.load(std::memory_order_relaxed);
            if (state == JobState::RUNNING || state == JobState::STOPPED) {
                if (fprintf(stderr, "cjsh(debug): hup propagate pgid=%d state=%d\n", job->pgid,
                            static_cast<int>(state)) < 0) {
                    (void)0;
                }
                if (killpg(job->pgid, SIGHUP) < 0) {
                    if (fprintf(stderr, "cjsh(debug): killpg HUP failed for %d: %s\n", job->pgid,
                                strerror(errno)) < 0) {
                        (void)0;
                    }
                }
#ifdef SIGCONT
                if (state == JobState::STOPPED) {
                    (void)killpg(job->pgid, SIGCONT);
                }
#endif
            }
        }

        if (jobs_snapshot.empty()) {
            pid_t orphan_pid = job_manager.get_last_background_pid();
            if (orphan_pid > 0) {
                (void)kill(orphan_pid, SIGHUP);
            }
        }

        if (shell_exec != nullptr) {
            if (enforce_hup) {
                shell_exec->terminate_all_child_process();
            } else {
                shell_exec->abandon_all_child_processes();
            }
        }

        if (enforce_hup) {
            for (auto& job : jobs_snapshot) {
                const JobState state = job->state.load(std::memory_order_relaxed);
                if (state == JobState::RUNNING || state == JobState::STOPPED) {
                    (void)killpg(job->pgid, SIGTERM);
                    (void)usleep(10000);
                    (void)killpg(job->pgid, SIGKILL);
                    job->state.store(JobState::TERMINATED, std::memory_order_relaxed);
                }
            }
        }

        job_manager.clear_all_jobs();

        if (!is_signal_observed(SIGHUP)) {
#ifdef __APPLE__
            std::_Exit(129);
#else
            std::quick_exit(129);
#endif
        } else {
            (void)alarm(1);
        }
    }

    if (s_sigterm_received != 0) {
        s_sigterm_received = 0;
        result.sigterm = true;

        cjsh_env::request_exit();

        if (shell_exec != nullptr) {
            shell_exec->terminate_all_child_process();

            auto& job_manager = JobManager::instance();
            auto all_jobs = job_manager.get_all_jobs();
            for (auto& job : all_jobs) {
                const JobState state = job->state.load(std::memory_order_relaxed);
                if (state == JobState::RUNNING || state == JobState::STOPPED) {
                    if (killpg(job->pgid, SIGTERM) == 0) {
                        job->state.store(JobState::TERMINATED, std::memory_order_relaxed);
                    }
                }
            }
        }

        if (is_signal_observed(SIGTERM)) {
            process_trapped_signal(SIGTERM);
            result.trapped_signals.push_back(SIGTERM);
        }
    }

    if (s_sigquit_received != 0) {
        s_sigquit_received = 0;
        if (is_signal_observed(SIGQUIT)) {
            process_trapped_signal(SIGQUIT);
            result.trapped_signals.push_back(SIGQUIT);
        }
    }

    if (s_sigtstp_received != 0) {
        s_sigtstp_received = 0;
        if (is_signal_observed(SIGTSTP)) {
            process_trapped_signal(SIGTSTP);
            result.trapped_signals.push_back(SIGTSTP);
        }
    }

#ifdef SIGUSR1
    if (s_sigusr1_received != 0) {
        s_sigusr1_received = 0;
        if (is_signal_observed(SIGUSR1)) {
            process_trapped_signal(SIGUSR1);
            result.trapped_signals.push_back(SIGUSR1);
        }
    }
#endif

#ifdef SIGUSR2
    if (s_sigusr2_received != 0) {
        s_sigusr2_received = 0;
        if (is_signal_observed(SIGUSR2)) {
            process_trapped_signal(SIGUSR2);
            result.trapped_signals.push_back(SIGUSR2);
        }
    }
#endif

#ifdef SIGABRT
    if (s_sigabrt_received != 0) {
        s_sigabrt_received = 0;
        if (is_signal_observed(SIGABRT)) {
            process_trapped_signal(SIGABRT);
            result.trapped_signals.push_back(SIGABRT);
        }
    }
#endif

#ifdef SIGCONT
    if (s_sigcont_received != 0) {
        s_sigcont_received = 0;

        if (is_signal_observed(SIGCONT)) {
            process_trapped_signal(SIGCONT);
            result.trapped_signals.push_back(SIGCONT);
        }
    }
#endif

#ifdef SIGALRM
    if (s_sigalrm_received != 0) {
        s_sigalrm_received = 0;
        if (is_signal_observed(SIGALRM)) {
            process_trapped_signal(SIGALRM);
            result.trapped_signals.push_back(SIGALRM);
        }
    }
#endif

#ifdef SIGWINCH
    if (s_sigwinch_received != 0) {
        s_sigwinch_received = 0;
        if (is_signal_observed(SIGWINCH)) {
            process_trapped_signal(SIGWINCH);
            result.trapped_signals.push_back(SIGWINCH);
        }
    }
#endif

#ifdef SIGPIPE
    if (s_sigpipe_received != 0) {
        s_sigpipe_received = 0;
        if (is_signal_observed(SIGPIPE)) {
            process_trapped_signal(SIGPIPE);
            result.trapped_signals.push_back(SIGPIPE);
        }
    }
#endif

#ifdef SIGTTIN
    if (s_sigttin_received != 0) {
        s_sigttin_received = 0;
        if (is_signal_observed(SIGTTIN)) {
            process_trapped_signal(SIGTTIN);
            result.trapped_signals.push_back(SIGTTIN);
        }
    }
#endif

#ifdef SIGTTOU
    if (s_sigttou_received != 0) {
        s_sigttou_received = 0;
        if (is_signal_observed(SIGTTOU)) {
            process_trapped_signal(SIGTTOU);
            result.trapped_signals.push_back(SIGTTOU);
        }
    }
#endif

    if (result.sigint && is_signal_observed(SIGINT)) {
        process_trapped_signal(SIGINT);
        result.trapped_signals.push_back(SIGINT);
    }

    if (result.sighup && is_signal_observed(SIGHUP)) {
        process_trapped_signal(SIGHUP);
        result.trapped_signals.push_back(SIGHUP);
    }

    return result;
}

void SignalHandler::observe_signal(int signum) {
    if (!is_signal_observed(signum)) {
        s_observed_signals.push_back(signum);
    }
}

void SignalHandler::unobserve_signal(int signum) {
    auto it = std::find(s_observed_signals.begin(), s_observed_signals.end(), signum);
    if (it != s_observed_signals.end()) {
        (void)s_observed_signals.erase(it);
    }
}

bool SignalHandler::is_signal_observed(int signum) {
    return std::find(s_observed_signals.begin(), s_observed_signals.end(), signum) !=
           s_observed_signals.end();
}

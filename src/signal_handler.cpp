#include "signal_handler.h"

#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>

#include "builtin/trap_command.h"
#include "cjsh.h"
#include "error_out.h"
#include "exec.h"
#include "job_control.h"
#include "shell.h"

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
        sigprocmask(SIG_SETMASK, &old_mask, nullptr);
    }
}

std::atomic<SignalHandler*> SignalHandler::s_instance(nullptr);

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

SignalHandler* g_signal_handler = nullptr;

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
    sigprocmask(SIG_SETMASK, &set, nullptr);
}

bool SignalHandler::has_pending_signals() {
    if (s_signal_pending.load(std::memory_order_acquire)) {
        return true;
    }
    return s_sigint_received != 0 || s_sigchld_received != 0 || s_sighup_received != 0 ||
           s_sigterm_received != 0 || s_sigttin_received != 0 || s_sigttou_received != 0;
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
    sigprocmask(SIG_SETMASK, &iset, nullptr);
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

sigset_t SignalHandler::get_current_mask() {
    sigset_t current_mask{};
    sigprocmask(SIG_SETMASK, nullptr, &current_mask);
    return current_mask;
}

void SignalHandler::install_signal_handler(int signum, struct sigaction* old_action) {
    struct sigaction sa{};
    sa.sa_sigaction = signal_handler;
    sigemptyset(&sa.sa_mask);

    sigfillset(&sa.sa_mask);

    sa.sa_flags = SA_SIGINFO;

    if (signum != SIGINT) {
        sa.sa_flags |= SA_RESTART;
    }

    sigaction(signum, &sa, old_action);
}

void SignalHandler::process_trapped_signal(int signum) {
    if (trap_manager_has_trap(signum)) {
        trap_manager_execute_trap(signum);
    }
}

void SignalHandler::signal_handler(int signum, siginfo_t* info, void* context) {
    (void)context;
    (void)info;

    if (is_forked_child()) {
        struct sigaction sa{};
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(signum, &sa, nullptr);
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
                    g_exit_flag = true;
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
            g_exit_flag = true;
            pid_t bg_pgid = JobManager::get_last_background_pid_atomic();
            if (bg_pgid > 0) {
                killpg(bg_pgid, SIGHUP);
            }
            should_mark_pending = true;
            break;
        }

        case SIGTERM: {
            s_sigterm_received = 1;
            g_exit_flag = true;

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
    sigaction(SIGPIPE, &sa, &m_old_sigpipe_handler);

    s_signal_states[SIGPIPE].disposition = SignalDisposition::IGNORE;

    sigaction(SIGTTOU, &sa, &m_old_sigttou_handler);
    sigaction(SIGTTIN, &sa, &m_old_sigttin_handler);

    install_signal_handler(SIGCHLD, &m_old_sigchld_handler);
    install_signal_handler(SIGINT, &m_old_sigint_handler);
    install_signal_handler(SIGHUP, &m_old_sighup_handler);
    install_signal_handler(SIGTERM, &m_old_sigterm_handler);

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

    sigaction(SIGQUIT, &sa, &m_old_sigquit_handler);
    sigaction(SIGTSTP, &sa, &m_old_sigtstp_handler);

#ifdef SIGWINCH

    install_signal_handler(SIGWINCH, &m_old_sigwinch_handler);
    s_signal_states[SIGWINCH].disposition = SignalDisposition::SYSTEM;
#endif

#ifdef SIGUSR1

    sigaction(SIGUSR1, nullptr, &m_old_sigusr1_handler);
#endif

#ifdef SIGUSR2
    sigaction(SIGUSR2, nullptr, &m_old_sigusr2_handler);
#endif

#ifdef SIGALRM
    sigaction(SIGALRM, nullptr, &m_old_sigalrm_handler);
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
    sigaction(SIGPIPE, &m_old_sigpipe_handler, nullptr);

#ifdef SIGUSR1
    sigaction(SIGUSR1, &m_old_sigusr1_handler, nullptr);
#endif

#ifdef SIGUSR2
    sigaction(SIGUSR2, &m_old_sigusr2_handler, nullptr);
#endif

#ifdef SIGALRM
    sigaction(SIGALRM, &m_old_sigalrm_handler, nullptr);
#endif

#ifdef SIGWINCH
    sigaction(SIGWINCH, &m_old_sigwinch_handler, nullptr);
#endif
}

SignalProcessingResult SignalHandler::process_pending_signals(Exec* shell_exec) {
    bool should_process = s_signal_pending.exchange(false, std::memory_order_acq_rel);
    if (!should_process && s_sigint_received == 0 && s_sigchld_received == 0 &&
        s_sighup_received == 0 && s_sigterm_received == 0) {
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
                        perror("kill (SIGINT) in process_pending_signals");
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

            if (s_sigchld_received == 1) {
                usleep(1000);
            }

            while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0 &&
                   reaped_count < max_reap_iterations) {
                reaped_count++;
                shell_exec->handle_child_signal(pid, status);

                auto& job_manager = JobManager::instance();
                auto job = job_manager.get_job_by_pgid(pid);
                if (!job) {
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
                        job->state = WIFEXITED(status) ? JobState::DONE : JobState::TERMINATED;
                        job->exit_status =
                            WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status);

                        JobManager::instance().clear_stdin_signal(job->pgid);

                        job->pids.erase(std::remove(job->pids.begin(), job->pids.end(), pid),
                                        job->pids.end());

                        if (job->pids.empty()) {
                            job->notified = true;
                        }
                    } else if (WIFSTOPPED(status)) {
                        job->state = JobState::STOPPED;
                        JobManager::instance().notify_job_stopped(job);
#ifdef SIGTTIN
                        if (WSTOPSIG(status) == SIGTTIN) {
                            JobManager::instance().clear_stdin_signal(job->pgid);
                        }
#endif
                    } else if (WIFCONTINUED(status)) {
                        job->state = JobState::RUNNING;
                        job->stop_notified = false;
                        JobManager::instance().clear_stdin_signal(job->pgid);
                    }
                }
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
        g_exit_flag = true;

        bool enforce_hup = !g_shell || g_shell->get_shell_option("huponexit");

        auto& job_manager = JobManager::instance();
        auto jobs_snapshot = job_manager.get_all_jobs();

        for (const auto& job : jobs_snapshot) {
            if (job->state == JobState::RUNNING || job->state == JobState::STOPPED) {
                if (fprintf(stderr, "cjsh(debug): hup propagate pgid=%d state=%d\n", job->pgid,
                            static_cast<int>(job->state)) < 0) {
                    (void)0;
                }
                if (killpg(job->pgid, SIGHUP) < 0) {
                    if (fprintf(stderr, "cjsh(debug): killpg HUP failed for %d: %s\n", job->pgid,
                                strerror(errno)) < 0) {
                        (void)0;
                    }
                }
#ifdef SIGCONT
                if (job->state == JobState::STOPPED) {
                    killpg(job->pgid, SIGCONT);
                }
#endif
            }
        }

        if (jobs_snapshot.empty()) {
            pid_t orphan_pid = job_manager.get_last_background_pid();
            if (orphan_pid > 0) {
                kill(orphan_pid, SIGHUP);
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
                if (job->state == JobState::RUNNING || job->state == JobState::STOPPED) {
                    killpg(job->pgid, SIGTERM);
                    usleep(10000);
                    killpg(job->pgid, SIGKILL);
                    job->state = JobState::TERMINATED;
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
            alarm(1);
        }
    }

    if (s_sigterm_received != 0) {
        s_sigterm_received = 0;
        result.sigterm = true;

        g_exit_flag = true;

        if (shell_exec != nullptr) {
            shell_exec->terminate_all_child_process();

            auto& job_manager = JobManager::instance();
            auto all_jobs = job_manager.get_all_jobs();
            for (auto& job : all_jobs) {
                if (job->state == JobState::RUNNING || job->state == JobState::STOPPED) {
                    if (killpg(job->pgid, SIGTERM) == 0) {
                        job->state = JobState::TERMINATED;
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
        s_observed_signals.erase(it);
    }
}

bool SignalHandler::is_signal_observed(int signum) {
    return std::find(s_observed_signals.begin(), s_observed_signals.end(), signum) !=
           s_observed_signals.end();
}

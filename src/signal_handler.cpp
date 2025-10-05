#include "signal_handler.h"

#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>
#include <csignal>
#include <iostream>

#include "cjsh.h"
#include "exec.h"
#include "job_control.h"

std::atomic<SignalHandler*> SignalHandler::s_instance(nullptr);
volatile sig_atomic_t SignalHandler::s_sigint_received = 0;
volatile sig_atomic_t SignalHandler::s_sigchld_received = 0;
volatile sig_atomic_t SignalHandler::s_sighup_received = 0;
volatile sig_atomic_t SignalHandler::s_sigterm_received = 0;
pid_t SignalHandler::s_main_pid = getpid();
std::vector<int> SignalHandler::s_observed_signals;

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

SignalHandler::SignalHandler()
    : m_old_sigint_handler(),
      m_old_sigchld_handler(),
      m_old_sighup_handler(),
      m_old_sigterm_handler(),
      m_old_sigquit_handler(),
      m_old_sigtstp_handler(),
      m_old_sigttin_handler(),
      m_old_sigttou_handler() {
    signal_unblock_all();
    s_instance.store(this);
}

SignalHandler::~SignalHandler() {
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

    if (search_name.size() > 3 &&
        (search_name.substr(0, 3) == "SIG" || search_name.substr(0, 3) == "sig")) {
        search_name = search_name.substr(3);
    }

    for (char& c : search_name) {
        c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    }

    for (const auto& signal : s_signal_table) {
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

bool SignalHandler::is_forked_child() {
    return getpid() != s_main_pid;
}

void SignalHandler::signal_unblock_all() {
    sigset_t iset{};
    sigemptyset(&iset);
    sigprocmask(SIG_SETMASK, &iset, nullptr);
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

    switch (signum) {
        case SIGINT: {
            s_sigint_received = 1;

            if (!is_observed) {
                if (!config::interactive_mode) {
                    g_exit_flag = true;
                    exit(128 + SIGINT);
                }
            }
            break;
        }

        case SIGCHLD: {
            s_sigchld_received = 1;
            break;
        }

        case SIGHUP: {
            s_sighup_received = 1;
            g_exit_flag = true;

            if (!is_observed) {
                _exit(129);
            }
            break;
        }

        case SIGTERM: {
            s_sigterm_received = 1;
            g_exit_flag = true;
            _exit(128 + SIGTERM);
        }

        default: {
            // Handle any other signals
            break;
        }
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
    sigaction(SIGPIPE, &sa, nullptr);

    sigaction(SIGTTOU, &sa, &m_old_sigttou_handler);
    sigaction(SIGTTIN, &sa, &m_old_sigttin_handler);

    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGCHLD, &sa, &m_old_sigchld_handler);

    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGINT, &sa, &m_old_sigint_handler);

    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGHUP, &sa, &m_old_sighup_handler);
    sigaction(SIGTERM, &sa, &m_old_sigterm_handler);
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
    if (s_sigint_received != 0) {
        s_sigint_received = 0;

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
#ifdef SIGTTIN
                        if (WSTOPSIG(status) == SIGTTIN) {
                            JobManager::instance().mark_job_reads_stdin(pid, true);
                            JobManager::instance().record_stdin_signal(pid, WSTOPSIG(status));
                        }
#endif
                    } else if (WIFCONTINUED(status)) {
                        job->state = JobState::RUNNING;
                        JobManager::instance().clear_stdin_signal(job->pgid);
                    }
                }
            }

            if (reaped_count >= max_reap_iterations) {
                std::cerr << "WARNING: SIGCHLD handler hit maximum iteration limit ("
                          << max_reap_iterations << "), breaking to prevent infinite loop" << '\n';
            }
        }
    }

    if (s_sighup_received != 0) {
        s_sighup_received = 0;
        g_exit_flag = true;

        if (shell_exec != nullptr) {
            shell_exec->terminate_all_child_process();

            auto& job_manager = JobManager::instance();
            auto all_jobs = job_manager.get_all_jobs();
            for (auto& job : all_jobs) {
                if (job->state == JobState::RUNNING || job->state == JobState::STOPPED) {
                    killpg(job->pgid, SIGTERM);
                    usleep(10000);
                    killpg(job->pgid, SIGKILL);
                    job->state = JobState::TERMINATED;
                }
            }
        }

        if (!is_signal_observed(SIGHUP)) {
            std::quick_exit(129);
        } else {
            alarm(1);
        }
    }

    if (s_sigterm_received != 0) {
        s_sigterm_received = 0;

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
    }
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

std::vector<int> SignalHandler::get_observed_signals() {
    return s_observed_signals;
}

#include "exec.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "job_control.h"
#include "parser.h"
#include "shell.h"
#include "shell_env.h"
#include "signal_handler.h"
#include "suggestion_utils.h"

namespace {

int extract_exit_code(int status) {
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
}

std::string join_arguments(const std::vector<std::string>& args) {
    std::string result;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            result += " ";
        }
        result += args[i];
    }
    return result;
}

bool is_shell_control_structure(const Command& cmd) {
    if (cmd.args.empty()) {
        return false;
    }

    const std::string& keyword = cmd.args[0];
    return keyword == "if" || keyword == "for" || keyword == "while" || keyword == "until" ||
           keyword == "case" || keyword == "select" || keyword == "function";
}

std::string command_text_for_interpretation(const Command& cmd) {
    if (!cmd.original_text.empty()) {
        return cmd.original_text;
    }
    return join_arguments(cmd.args);
}

struct ProcessSubstitutionResources {
    std::vector<std::string> fifo_paths;
    std::vector<pid_t> child_pids;
};

void cleanup_process_substitutions(ProcessSubstitutionResources& resources,
                                   bool terminate_children = false);

std::string generate_process_substitution_fifo_path(size_t index) {
    static std::atomic<uint64_t> counter{0};
    uint64_t sequence = counter.fetch_add(1, std::memory_order_relaxed);

    std::ostringstream path;
    path << "/tmp/cjsh_procsub_" << getpid() << "_" << std::time(nullptr) << "_" << index << "_"
         << sequence;
    return path.str();
}

void replace_all_instances(std::string& target, const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = target.find(from, pos)) != std::string::npos) {
        target.replace(pos, from.length(), to);
        pos += to.length();
    }
}

bool replace_first_instance(std::string& target, const std::string& from, const std::string& to) {
    size_t pos = target.find(from);
    if (pos == std::string::npos) {
        return false;
    }
    target.replace(pos, from.length(), to);
    return true;
}

ProcessSubstitutionResources setup_process_substitutions(Command& cmd) {
    ProcessSubstitutionResources resources;

    if (!g_shell) {
        return resources;
    }

    try {
        for (size_t i = 0; i < cmd.process_substitutions.size(); ++i) {
            const std::string& proc_sub = cmd.process_substitutions[i];

            if (proc_sub.length() < 4 || proc_sub.back() != ')' ||
                (proc_sub[0] != '<' && proc_sub[0] != '>') || proc_sub[1] != '(') {
                throw std::runtime_error("cjsh: invalid process substitution: " + proc_sub);
            }

            bool is_input = proc_sub[0] == '<';
            std::string command = proc_sub.substr(2, proc_sub.length() - 3);

            std::string fifo_path = generate_process_substitution_fifo_path(i);
            if (mkfifo(fifo_path.c_str(), 0600) == -1) {
                throw std::runtime_error("cjsh: failed to create FIFO for process substitution '" +
                                         proc_sub + "': " + std::string(strerror(errno)));
            }

            pid_t pid = fork();
            if (pid == -1) {
                unlink(fifo_path.c_str());
                throw std::runtime_error("cjsh: failed to fork for process substitution '" +
                                         proc_sub + "': " + std::string(strerror(errno)));
            }

            if (pid == 0) {
                if (is_input) {
                    auto fifo_result = cjsh_filesystem::safe_open(fifo_path, O_WRONLY);
                    if (fifo_result.is_error()) {
                        std::cerr << "cjsh: file not found: open: failed to "
                                     "open FIFO for writing: "
                                  << fifo_result.error() << '\n';
                        _exit(EXIT_FAILURE);
                    }

                    auto dup_result =
                        cjsh_filesystem::safe_dup2(fifo_result.value(), STDOUT_FILENO);
                    if (dup_result.is_error()) {
                        std::cerr << "cjsh: runtime error: dup2: failed to duplicate "
                                     "stdout descriptor: "
                                  << dup_result.error() << '\n';
                        cjsh_filesystem::safe_close(fifo_result.value());
                        _exit(EXIT_FAILURE);
                    }

                    cjsh_filesystem::safe_close(fifo_result.value());
                } else {
                    auto fifo_result = cjsh_filesystem::safe_open(fifo_path, O_RDONLY);
                    if (fifo_result.is_error()) {
                        std::cerr << "cjsh: file not found: open: failed to "
                                     "open FIFO for reading: "
                                  << fifo_result.error() << '\n';
                        _exit(EXIT_FAILURE);
                    }

                    auto dup_result = cjsh_filesystem::safe_dup2(fifo_result.value(), STDIN_FILENO);
                    if (dup_result.is_error()) {
                        std::cerr << "cjsh: runtime error: dup2: failed to duplicate "
                                     "stdin descriptor: "
                                  << dup_result.error() << '\n';
                        cjsh_filesystem::safe_close(fifo_result.value());
                        _exit(EXIT_FAILURE);
                    }

                    cjsh_filesystem::safe_close(fifo_result.value());
                }

                int result = g_shell->execute(command);
                _exit(result);
            }

            resources.child_pids.push_back(pid);
            resources.fifo_paths.push_back(fifo_path);

            bool replaced_arg = false;
            for (auto& arg : cmd.args) {
                if (replace_first_instance(arg, proc_sub, fifo_path)) {
                    replaced_arg = true;
                    break;
                }
            }

            if (!cmd.input_file.empty()) {
                replace_first_instance(cmd.input_file, proc_sub, fifo_path);
            }
            if (!cmd.output_file.empty()) {
                replace_first_instance(cmd.output_file, proc_sub, fifo_path);
            }
            if (!cmd.append_file.empty()) {
                replace_first_instance(cmd.append_file, proc_sub, fifo_path);
            }
            if (!cmd.stderr_file.empty()) {
                replace_first_instance(cmd.stderr_file, proc_sub, fifo_path);
            }
            if (!cmd.both_output_file.empty()) {
                replace_first_instance(cmd.both_output_file, proc_sub, fifo_path);
            }
            for (auto& [fd, path] : cmd.fd_redirections) {
                replace_first_instance(path, proc_sub, fifo_path);
            }

            if (!replaced_arg) {
                for (auto& arg : cmd.args) {
                    replace_all_instances(arg, proc_sub, fifo_path);
                }
            }
        }
    } catch (...) {
        cleanup_process_substitutions(resources, true);
        throw;
    }

    return resources;
}

void cleanup_process_substitutions(ProcessSubstitutionResources& resources,
                                   bool terminate_children) {
    if (terminate_children) {
        for (pid_t pid : resources.child_pids) {
            if (pid > 0) {
                kill(pid, SIGTERM);
            }
        }
    }

    for (pid_t pid : resources.child_pids) {
        if (pid <= 0) {
            continue;
        }
        int status = 0;
        while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {
        }
    }

    for (const std::string& path : resources.fifo_paths) {
        unlink(path.c_str());
    }

    resources.child_pids.clear();
    resources.fifo_paths.clear();
}

enum class FdOperationErrorType {
    Redirect,
    Duplication
};

struct FdOperationError {
    FdOperationErrorType type;
    int fd_num;
    int src_fd;
    std::string spec;
    std::string error;
};

struct RedirectSpecInfo {
    std::string file;
    int flags{0};
};

RedirectSpecInfo parse_fd_redirect_spec(int fd_num, const std::string& spec) {
    RedirectSpecInfo info;

    if (spec.rfind("input:", 0) == 0) {
        info.file = spec.substr(6);
        info.flags = O_RDONLY;
    } else if (spec.rfind("output:", 0) == 0) {
        info.file = spec.substr(7);
        info.flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else {
        info.file = spec;
        info.flags = (fd_num == 0) ? O_RDONLY : (O_WRONLY | O_CREAT | O_TRUNC);
    }

    return info;
}

template <typename FailureHandler>
bool apply_fd_operations(const Command& cmd, FailureHandler&& on_failure) {
    for (const auto& fd_redir : cmd.fd_redirections) {
        int fd_num = fd_redir.first;
        const std::string& spec = fd_redir.second;
        RedirectSpecInfo info = parse_fd_redirect_spec(fd_num, spec);

        auto redirect_result = cjsh_filesystem::redirect_fd(info.file, fd_num, info.flags);
        if (redirect_result.is_error()) {
            on_failure(FdOperationError{FdOperationErrorType::Redirect, fd_num, -1, spec,
                                        redirect_result.error()});
            return false;
        }
    }

    for (const auto& fd_dup : cmd.fd_duplications) {
        int dst_fd = fd_dup.first;
        int src_fd = fd_dup.second;

        if (src_fd == -1) {
            cjsh_filesystem::safe_close(dst_fd);
            continue;
        }

        auto dup_result = cjsh_filesystem::safe_dup2(src_fd, dst_fd);
        if (dup_result.is_error()) {
            on_failure(FdOperationError{FdOperationErrorType::Duplication, dst_fd, src_fd, "",
                                        dup_result.error()});
            return false;
        }
    }

    return true;
}

enum class HereDocErrorKind {
    Pipe,
    ContentWrite,
    NewlineWrite,
    Duplication
};

template <typename ErrorHandler>
bool setup_here_document_stdin(const std::string& here_doc, ErrorHandler&& on_error) {
    if (here_doc.empty()) {
        return true;
    }

    int here_pipe[2] = {-1, -1};
    auto pipe_result = cjsh_filesystem::create_pipe_cloexec(here_pipe);
    if (pipe_result.is_error()) {
        on_error(HereDocErrorKind::Pipe, pipe_result.error());
        return false;
    }

    auto write_result = cjsh_filesystem::write_all(here_pipe[1], std::string_view{here_doc});
    if (write_result.is_error()) {
        on_error(HereDocErrorKind::ContentWrite, write_result.error());
        cjsh_filesystem::close_pipe(here_pipe);
        return false;
    }

    auto newline_result = cjsh_filesystem::write_all(here_pipe[1], std::string_view("\n", 1));
    if (newline_result.is_error()) {
        on_error(HereDocErrorKind::NewlineWrite, newline_result.error());
        cjsh_filesystem::close_pipe(here_pipe);
        return false;
    }

    cjsh_filesystem::safe_close(here_pipe[1]);

    auto dup_result = cjsh_filesystem::safe_dup2(here_pipe[0], STDIN_FILENO);
    cjsh_filesystem::safe_close(here_pipe[0]);
    if (dup_result.is_error()) {
        on_error(HereDocErrorKind::Duplication, dup_result.error());
        return false;
    }

    return true;
}

enum class StreamRedirectErrorKind {
    Noclobber,
    Redirect,
    Duplication
};

struct StreamRedirectError {
    StreamRedirectErrorKind kind;
    std::string target;
    std::string detail;
    int src_fd;
    int dst_fd;
};

template <typename ErrorHandler>
bool configure_stderr_redirects(const Command& cmd, ErrorHandler&& on_error) {
    if (!cmd.stderr_file.empty()) {
        if (!cmd.stderr_append &&
            cjsh_filesystem::should_noclobber_prevent_overwrite(cmd.stderr_file)) {
            on_error(StreamRedirectError{StreamRedirectErrorKind::Noclobber, cmd.stderr_file,
                                         "cannot overwrite existing file (noclobber is set)", -1,
                                         -1});
            return false;
        }

        int flags = O_WRONLY | O_CREAT | (cmd.stderr_append ? O_APPEND : O_TRUNC);
        auto redirect_result = cjsh_filesystem::redirect_fd(cmd.stderr_file, STDERR_FILENO, flags);
        if (redirect_result.is_error()) {
            on_error(StreamRedirectError{StreamRedirectErrorKind::Redirect, cmd.stderr_file,
                                         redirect_result.error(), -1, STDERR_FILENO});
            return false;
        }
    } else if (cmd.stderr_to_stdout) {
        auto dup_result = cjsh_filesystem::safe_dup2(STDOUT_FILENO, STDERR_FILENO);
        if (dup_result.is_error()) {
            on_error(StreamRedirectError{StreamRedirectErrorKind::Duplication, "",
                                         dup_result.error(), STDOUT_FILENO, STDERR_FILENO});
            return false;
        }
    }

    if (cmd.stdout_to_stderr) {
        auto dup_result = cjsh_filesystem::safe_dup2(STDERR_FILENO, STDOUT_FILENO);
        if (dup_result.is_error()) {
            on_error(StreamRedirectError{StreamRedirectErrorKind::Duplication, "",
                                         dup_result.error(), STDERR_FILENO, STDOUT_FILENO});
            return false;
        }
    }

    return true;
}

template <typename ErrorHandler>
void maybe_set_foreground_terminal(bool enabled, int terminal_fd, pid_t pgid,
                                   ErrorHandler&& on_error) {
    if (!enabled) {
        return;
    }

    if (tcsetpgrp(terminal_fd, pgid) < 0) {
        int saved_errno = errno;
        on_error(saved_errno);
    }
}

[[noreturn]] void exec_external_child(const std::vector<std::string>& args) {
    auto c_args = cjsh_env::build_exec_argv(args);
    execvp(args[0].c_str(), c_args.data());
    int saved_errno = errno;
    _exit((saved_errno == EACCES || saved_errno == EISDIR || saved_errno == ENOEXEC) ? 126 : 127);
}

}  // namespace

Exec::Exec()
    : shell_pgid(getpid()),
      shell_terminal(STDIN_FILENO),
      shell_is_interactive(isatty(shell_terminal)),
      last_terminal_output_error("") {
    if (shell_is_interactive) {
        if (tcgetattr(shell_terminal, &shell_tmodes) < 0) {
            set_error(ErrorType::RUNTIME_ERROR, "tcgetattr",
                      "failed to get terminal attributes in constructor: " +
                          std::string(strerror(errno)));
        }
    }
}

Exec::~Exec() {
    int status = 0;
    pid_t pid = 0;
    int zombie_count = 0;
    const int max_cleanup_iterations = 50;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0 && zombie_count < max_cleanup_iterations) {
        zombie_count++;
    }

    if (zombie_count >= max_cleanup_iterations) {
        std::cerr << "WARNING: Exec destructor hit maximum cleanup iterations, "
                     "some zombies may remain"
                  << '\n';
    }
}

void Exec::init_shell() {
    if (shell_is_interactive) {
        shell_pgid = getpid();
        if (setpgid(shell_pgid, shell_pgid) < 0) {
            if (errno != EPERM) {
                set_error(ErrorType::RUNTIME_ERROR, "setpgid",
                          "failed to set process group ID: " + std::string(strerror(errno)));
                return;
            }
        }

        if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
            set_error(
                ErrorType::RUNTIME_ERROR, "tcsetpgrp",
                "failed to set terminal foreground process group: " + std::string(strerror(errno)));
        }

        if (tcgetattr(shell_terminal, &shell_tmodes) < 0) {
            set_error(ErrorType::RUNTIME_ERROR, "tcgetattr",
                      "failed to get terminal attributes: " + std::string(strerror(errno)));
        }
    }
}

void Exec::handle_child_signal(pid_t pid, int status) {
    static bool use_signal_masking = false;
    static int signal_count = 0;

    if (++signal_count > 10) {
        use_signal_masking = true;
    }

    std::unique_ptr<SignalMask> mask;
    if (use_signal_masking) {
        mask = std::make_unique<SignalMask>(SIGCHLD);
    }

    std::lock_guard<std::mutex> lock(jobs_mutex);

    for (auto& job_pair : jobs) {
        int job_id = job_pair.first;
        Job& job = job_pair.second;

        auto it = std::find(job.pids.begin(), job.pids.end(), pid);
        if (it != job.pids.end()) {
            if (pid == job.last_pid) {
                job.last_status = status;
            }
            if (WIFSTOPPED(status)) {
                job.stopped = true;
                job.status = status;
            } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                job.pids.erase(it);

                if (job.pids.empty()) {
                    job.completed = true;
                    job.stopped = false;
                    job.status = status;

                    if (job.background) {
                        std::cerr << "\n[" << job_id << "] Done\t" << job.command << '\n';
                    }
                }
            }
            break;
        }
    }
}

void Exec::set_error(const ErrorInfo& error) {
    std::lock_guard<std::mutex> lock(error_mutex);
    last_error = error;
    last_terminal_output_error = error.message;
}

void Exec::set_error(ErrorType type, const std::string& command, const std::string& message,
                     const std::vector<std::string>& suggestions) {
    ErrorInfo error = {type, command, message, suggestions};
    set_error(error);
}

ErrorInfo Exec::get_error() {
    std::lock_guard<std::mutex> lock(error_mutex);
    return last_error;
}

std::string Exec::get_error_string() {
    std::lock_guard<std::mutex> lock(error_mutex);
    return last_terminal_output_error;
}

void Exec::print_last_error() {
    std::lock_guard<std::mutex> lock(error_mutex);
    print_error(last_error);
}

bool Exec::requires_fork(const Command& cmd) const {
    return !cmd.input_file.empty() || !cmd.output_file.empty() || !cmd.append_file.empty() ||
           cmd.background || !cmd.stderr_file.empty() || cmd.stderr_to_stdout ||
           cmd.stdout_to_stderr || !cmd.here_doc.empty() || !cmd.here_string.empty() ||
           cmd.both_output || !cmd.process_substitutions.empty() || !cmd.fd_redirections.empty() ||
           !cmd.fd_duplications.empty();
}

bool Exec::can_execute_in_process(const Command& cmd) const {
    if (cmd.args.empty())
        return false;

    if (g_shell && (g_shell->get_built_ins() != nullptr) &&
        (g_shell->get_built_ins()->is_builtin_command(cmd.args[0]) != 0)) {
        return !requires_fork(cmd);
    }

    return false;
}

int Exec::execute_builtin_with_redirections(Command cmd) {
    if (!g_shell || (g_shell->get_built_ins() == nullptr)) {
        set_error(ErrorType::RUNTIME_ERROR, "builtin",
                  "no shell context available for builtin execution");
        last_exit_code = EX_SOFTWARE;
        return EX_SOFTWARE;
    }

    auto duplicate_fd = [](int fd) {
        int min_fd = std::max(fd + 1, 10);
        int dup_fd = fcntl(fd, F_DUPFD_CLOEXEC, min_fd);
        if (dup_fd == -1) {
            dup_fd = fcntl(fd, F_DUPFD, min_fd);
        }
        if (dup_fd == -1) {
            dup_fd = dup(fd);
        }
        return dup_fd;
    };

    int orig_stdin = duplicate_fd(STDIN_FILENO);
    int orig_stdout = duplicate_fd(STDOUT_FILENO);
    int orig_stderr = duplicate_fd(STDERR_FILENO);

    if (orig_stdin == -1 || orig_stdout == -1 || orig_stderr == -1) {
        set_error(ErrorType::RUNTIME_ERROR, cmd.args.empty() ? "builtin" : cmd.args[0],
                  "failed to save original file descriptors", {});
        if (orig_stdin != -1) {
            cjsh_filesystem::safe_close(orig_stdin);
        }
        if (orig_stdout != -1) {
            cjsh_filesystem::safe_close(orig_stdout);
        }
        if (orig_stderr != -1) {
            cjsh_filesystem::safe_close(orig_stderr);
        }
        last_exit_code = EX_OSERR;
        return EX_OSERR;
    }

    ProcessSubstitutionResources proc_resources;
    int exit_code = 0;
    bool persist_fd_changes = (!cmd.args.empty() && cmd.args[0] == "exec" && cmd.args.size() == 1);

    auto restore_descriptors = [&](bool terminate_process_subs) {
        cleanup_process_substitutions(proc_resources, terminate_process_subs);

        if (!persist_fd_changes) {
            cjsh_filesystem::safe_dup2(orig_stdin, STDIN_FILENO);
            cjsh_filesystem::safe_dup2(orig_stdout, STDOUT_FILENO);
            cjsh_filesystem::safe_dup2(orig_stderr, STDERR_FILENO);
        }

        cjsh_filesystem::safe_close(orig_stdin);
        cjsh_filesystem::safe_close(orig_stdout);
        cjsh_filesystem::safe_close(orig_stderr);
    };

    try {
        proc_resources = setup_process_substitutions(cmd);

        if (!cmd.here_doc.empty()) {
            int here_pipe[2] = {-1, -1};
            auto pipe_result = cjsh_filesystem::create_pipe_cloexec(here_pipe);
            if (pipe_result.is_error()) {
                throw std::runtime_error("cjsh: failed to create pipe for here document: " +
                                         pipe_result.error());
            }

            std::string error;
            auto write_result =
                cjsh_filesystem::write_all(here_pipe[1], std::string_view{cmd.here_doc});
            if (write_result.is_error()) {
                error = write_result.error();
            } else {
                auto newline_result =
                    cjsh_filesystem::write_all(here_pipe[1], std::string_view("\n", 1));
                if (newline_result.is_error()) {
                    error = newline_result.error();
                }
            }

            cjsh_filesystem::safe_close(here_pipe[1]);

            if (!error.empty()) {
                cjsh_filesystem::safe_close(here_pipe[0]);
                throw std::runtime_error("cjsh: failed to write here document content: " + error);
            }

            auto dup_result = cjsh_filesystem::safe_dup2(here_pipe[0], STDIN_FILENO);
            cjsh_filesystem::safe_close(here_pipe[0]);
            if (dup_result.is_error()) {
                throw std::runtime_error("cjsh: failed to redirect stdin for here document: " +
                                         dup_result.error());
            }
        }

        if (!cmd.input_file.empty()) {
            auto redirect_result =
                cjsh_filesystem::redirect_fd(cmd.input_file, STDIN_FILENO, O_RDONLY);
            if (redirect_result.is_error()) {
                throw std::runtime_error("cjsh: failed to redirect stdin from " + cmd.input_file +
                                         ": " + redirect_result.error());
            }
        }

        if (!cmd.here_string.empty()) {
            auto here_error = cjsh_filesystem::setup_here_string_stdin(cmd.here_string);
            if (here_error.has_value()) {
                switch (here_error->type) {
                    case cjsh_filesystem::HereStringErrorType::Pipe:
                        throw std::runtime_error("cjsh: failed to create pipe for here string");
                    case cjsh_filesystem::HereStringErrorType::Write:
                        throw std::runtime_error("cjsh: failed to write here string content");
                    case cjsh_filesystem::HereStringErrorType::Dup:
                        throw std::runtime_error(
                            "cjsh: failed to redirect stdin for here string: " +
                            here_error->detail);
                }
            }
        }

        if (!cmd.output_file.empty()) {
            if (cjsh_filesystem::should_noclobber_prevent_overwrite(cmd.output_file,
                                                                    cmd.force_overwrite)) {
                throw std::runtime_error("cjsh: cannot overwrite existing file '" +
                                         cmd.output_file + "' (noclobber is set)");
            }

            auto redirect_result = cjsh_filesystem::redirect_fd(cmd.output_file, STDOUT_FILENO,
                                                                O_WRONLY | O_CREAT | O_TRUNC);
            if (redirect_result.is_error()) {
                throw std::runtime_error("cjsh: failed to redirect stdout to file '" +
                                         cmd.output_file + "': " + redirect_result.error());
            }
        }

        if (cmd.both_output && !cmd.both_output_file.empty()) {
            if (cjsh_filesystem::should_noclobber_prevent_overwrite(cmd.both_output_file)) {
                throw std::runtime_error("cjsh: cannot overwrite existing file '" +
                                         cmd.both_output_file + "' (noclobber is set)");
            }

            auto stdout_result = cjsh_filesystem::redirect_fd(cmd.both_output_file, STDOUT_FILENO,
                                                              O_WRONLY | O_CREAT | O_TRUNC);
            if (stdout_result.is_error()) {
                throw std::runtime_error("cjsh: failed to redirect stdout for &>: " +
                                         cmd.both_output_file + ": " + stdout_result.error());
            }

            auto stderr_result = cjsh_filesystem::safe_dup2(STDOUT_FILENO, STDERR_FILENO);
            if (stderr_result.is_error()) {
                throw std::runtime_error("cjsh: failed to redirect stderr for &>: " +
                                         stderr_result.error());
            }
        }

        if (!cmd.append_file.empty()) {
            auto redirect_result = cjsh_filesystem::redirect_fd(cmd.append_file, STDOUT_FILENO,
                                                                O_WRONLY | O_CREAT | O_APPEND);
            if (redirect_result.is_error()) {
                throw std::runtime_error("cjsh: failed to redirect stdout for append: " +
                                         cmd.append_file + ": " + redirect_result.error());
            }
        }

        if (!cmd.stderr_file.empty()) {
            if (!cmd.stderr_append &&
                cjsh_filesystem::should_noclobber_prevent_overwrite(cmd.stderr_file)) {
                throw std::runtime_error("cjsh: cannot overwrite existing file '" +
                                         cmd.stderr_file + "' (noclobber is set)");
            }

            int flags = O_WRONLY | O_CREAT | (cmd.stderr_append ? O_APPEND : O_TRUNC);
            auto redirect_result =
                cjsh_filesystem::redirect_fd(cmd.stderr_file, STDERR_FILENO, flags);
            if (redirect_result.is_error()) {
                throw std::runtime_error("cjsh: failed to redirect stderr to file '" +
                                         cmd.stderr_file + "': " + redirect_result.error());
            }
        }

        if (cmd.stderr_to_stdout) {
            auto dup_result = cjsh_filesystem::safe_dup2(STDOUT_FILENO, STDERR_FILENO);
            if (dup_result.is_error()) {
                throw std::runtime_error("cjsh: failed to redirect stderr to stdout: " +
                                         dup_result.error());
            }
        }

        if (cmd.stdout_to_stderr) {
            auto dup_result = cjsh_filesystem::safe_dup2(STDERR_FILENO, STDOUT_FILENO);
            if (dup_result.is_error()) {
                throw std::runtime_error("cjsh: failed to redirect stdout to stderr: " +
                                         dup_result.error());
            }
        }

        bool is_exec_builtin = !cmd.args.empty() && cmd.args[0] == "exec";

        if (!cmd.fd_redirections.empty() || !cmd.fd_duplications.empty()) {
            auto fd_error_handler = [&](const FdOperationError& error) -> void {
                switch (error.type) {
                    case FdOperationErrorType::Redirect:
                        throw std::runtime_error("cjsh: exec: " + error.spec + ": " + error.error);
                    case FdOperationErrorType::Duplication:
                        throw std::runtime_error("cjsh: exec: dup2 failed for " +
                                                 std::to_string(error.fd_num) + ">&" +
                                                 std::to_string(error.src_fd) + ": " + error.error);
                }
            };
            (void)apply_fd_operations(cmd, fd_error_handler);
        }

        std::cout.flush();
        std::cerr.flush();
        std::clog.flush();

        if (is_exec_builtin) {
            if (cmd.args.size() == 1) {
                exit_code = 0;
            } else {
                std::vector<std::string> exec_args(cmd.args.begin() + 1, cmd.args.end());
                exit_code = g_shell->execute_command(exec_args, false);
            }
        } else {
            exit_code = g_shell->get_built_ins()->builtin_command(cmd.args);
        }

        std::cout.flush();
        std::cerr.flush();
        std::clog.flush();
    } catch (const std::exception& e) {
        restore_descriptors(true);
        set_error(ErrorType::RUNTIME_ERROR, cmd.args.empty() ? "builtin" : cmd.args[0],
                  std::string(e.what()));
        last_exit_code = EX_OSERR;
        return EX_OSERR;
    }

    restore_descriptors(false);

    std::string command_name = cmd.args.empty() ? "" : cmd.args[0];
    auto exit_result = job_utils::make_exit_error_result(command_name, exit_code,
                                                         "builtin command completed successfully",
                                                         "builtin command failed with exit code ");
    set_error(exit_result.type, command_name, exit_result.message, exit_result.suggestions);
    last_exit_code = exit_code;
    return exit_code;
}

bool Exec::handle_empty_args(const std::vector<std::string>& args) {
    if (!args.empty()) {
        return false;
    }

    set_error(ErrorType::INVALID_ARGUMENT, "",
              "cannot execute empty command - no arguments provided");
    last_exit_code = EX_DATAERR;
    return true;
}

int Exec::execute_command_sync(const std::vector<std::string>& args) {
    if (handle_empty_args(args)) {
        return last_exit_code;
    }
    std::vector<std::pair<std::string, std::string>> env_assignments;
    size_t cmd_start_idx = cjsh_env::collect_env_assignments(args, env_assignments);

    if (cmd_start_idx >= args.size()) {
        if (g_shell) {
            auto& env_vars = g_shell->get_env_vars();
            for (const auto& env : env_assignments) {
                env_vars[env.first] = env.second;

                if (env.first == "PATH" || env.first == "PWD" || env.first == "HOME" ||
                    env.first == "USER" || env.first == "SHELL") {
                    setenv(env.first.c_str(), env.second.c_str(), 1);
                }
            }

            if (g_shell->get_parser() != nullptr) {
                g_shell->get_parser()->set_env_vars(env_vars);
            }
        }

        last_exit_code = 0;
        return 0;
    }

    std::vector<std::string> cmd_args(
        std::next(args.begin(), static_cast<std::ptrdiff_t>(cmd_start_idx)), args.end());

    Command proc_cmd;
    proc_cmd.args = cmd_args;
    for (const auto& arg : cmd_args) {
        if (arg.size() >= 4 && arg.back() == ')' &&
            (arg.rfind("<(", 0) == 0 || arg.rfind(">(", 0) == 0)) {
            proc_cmd.process_substitutions.push_back(arg);
        }
    }

    ProcessSubstitutionResources proc_resources;
    if (!proc_cmd.process_substitutions.empty()) {
        try {
            proc_resources = setup_process_substitutions(proc_cmd);
            cmd_args = proc_cmd.args;
        } catch (const std::exception& e) {
            set_error(ErrorType::RUNTIME_ERROR, cmd_args.empty() ? "command" : cmd_args[0],
                      e.what(), {});
            last_exit_code = EX_OSERR;
            return EX_OSERR;
        }
    }

    pid_t pid = fork();

    if (pid == -1) {
        set_error(ErrorType::RUNTIME_ERROR, cmd_args.empty() ? "unknown" : cmd_args[0],
                  "failed to fork process: " + std::string(strerror(errno)), {});
        last_exit_code = EX_OSERR;
        cleanup_process_substitutions(proc_resources, true);
        return EX_OSERR;
    }

    if (pid == 0) {
        cjsh_env::apply_env_assignments(env_assignments);

        pid_t child_pid = getpid();
        if (setpgid(child_pid, child_pid) < 0) {
            std::cerr << "cjsh: runtime error: setpgid: failed to set process group "
                         "ID in child: "
                      << strerror(errno) << '\n';
            _exit(EXIT_FAILURE);
        }

        reset_child_signals();

        auto c_args = cjsh_env::build_exec_argv(cmd_args);
        execvp(cmd_args[0].c_str(), c_args.data());

        int saved_errno = errno;
        int exit_code =
            (saved_errno == EACCES || saved_errno == EISDIR || saved_errno == ENOEXEC) ? 126 : 127;

        if (saved_errno == ENOENT) {
            auto suggestions = suggestion_utils::generate_command_suggestions(cmd_args[0]);
            set_error(ErrorType::COMMAND_NOT_FOUND, cmd_args[0], "", suggestions);
        } else if (saved_errno == EACCES || saved_errno == EISDIR) {
            set_error(ErrorType::PERMISSION_DENIED, cmd_args[0], "", {});
        } else if (saved_errno == ENOEXEC) {
            set_error(ErrorType::INVALID_ARGUMENT, cmd_args[0], "invalid executable format", {});
        } else {
            set_error(ErrorType::RUNTIME_ERROR, cmd_args[0],
                      "execution failed: " + std::string(strerror(saved_errno)), {});
        }
        _exit(exit_code);
    }

    if (setpgid(pid, pid) < 0) {
        if (errno != EACCES && errno != ESRCH) {
            set_error(ErrorType::RUNTIME_ERROR, "setpgid",
                      "failed to set process group ID in parent: " + std::string(strerror(errno)));
        }
    }

    Job job;
    job.pgid = pid;
    job.command = args[0];
    job.background = false;
    job.completed = false;
    job.stopped = false;
    job.pids.push_back(pid);

    int job_id = add_job(job);

    std::string full_command = join_arguments(args);
    bool reads_stdin = true;

    if (g_shell && (g_shell->get_parser() != nullptr)) {
        try {
            auto command_pipeline = g_shell->get_parser()->parse_pipeline(full_command);
            if (!command_pipeline.empty()) {
                reads_stdin = job_utils::pipeline_consumes_terminal_stdin(command_pipeline);
            }
        } catch (const std::exception& e) {
        }
    }

    int new_job_id =
        JobManager::instance().add_job(pid, {pid}, full_command, job.background, reads_stdin);

    put_job_in_foreground(job_id, false);

    std::lock_guard<std::mutex> lock(jobs_mutex);
    auto it = jobs.find(job_id);
    int exit_code = 0;

    if (it != jobs.end() && it->second.completed) {
        exit_code = extract_exit_code(it->second.status);
        JobManager::instance().remove_job(new_job_id);
    }

    auto exit_result = job_utils::make_exit_error_result(
        args[0], exit_code, "command completed successfully", "command failed with exit code ");
    set_error(exit_result.type, args[0], exit_result.message, exit_result.suggestions);
    last_exit_code = exit_code;

    cleanup_process_substitutions(proc_resources, false);

    if (exit_code == 0 && !cmd_args.empty()) {
        const std::string& command_name = cmd_args[0];

        std::string basename_command;
        size_t last_slash = command_name.find_last_of('/');
        if (last_slash != std::string::npos) {
            basename_command = command_name.substr(last_slash + 1);
        } else {
            basename_command = command_name;
        }

        bool already_in_cache = cjsh_filesystem::is_executable_in_cache(basename_command);

        if (!already_in_cache) {
            std::string full_path = cjsh_filesystem::find_executable_in_path(basename_command);

            if (!full_path.empty()) {
                cjsh_filesystem::add_executable_to_cache(basename_command, full_path);
            }
        }
    } else if (exit_code == 127 && !cmd_args.empty()) {
        const std::string& command_name = cmd_args[0];

        if (cjsh_filesystem::is_executable_in_cache(command_name)) {
            cjsh_filesystem::remove_executable_from_cache(command_name);
        }
    }

    return exit_code;
}

int Exec::execute_command_async(const std::vector<std::string>& args) {
    if (handle_empty_args(args)) {
        return last_exit_code;
    }

    std::vector<std::pair<std::string, std::string>> env_assignments;
    size_t cmd_start_idx = cjsh_env::collect_env_assignments(args, env_assignments);

    if (cmd_start_idx >= args.size()) {
        cjsh_env::apply_env_assignments(env_assignments);
        set_error(ErrorType::RUNTIME_ERROR, "", "Environment variables set", {});
        last_exit_code = 0;
        return 0;
    }

    std::vector<std::string> cmd_args(
        std::next(args.begin(), static_cast<std::ptrdiff_t>(cmd_start_idx)), args.end());

    pid_t pid = fork();

    if (pid == -1) {
        std::string cmd_name = cmd_args.empty() ? "unknown" : cmd_args[0];
        set_error(ErrorType::RUNTIME_ERROR, cmd_name,
                  "failed to create background process: " + std::string(strerror(errno)), {});
        last_exit_code = EX_OSERR;
        return EX_OSERR;
    }

    if (pid == 0) {
        cjsh_env::apply_env_assignments(env_assignments);

        if (setpgid(0, 0) < 0) {
            std::cerr << "cjsh: runtime error: setpgid: failed to set process group "
                         "ID in background child: "
                      << strerror(errno) << '\n';
            _exit(EXIT_FAILURE);
        }

        (void)signal(SIGINT, SIG_IGN);
        (void)signal(SIGQUIT, SIG_IGN);
        (void)signal(SIGTSTP, SIG_IGN);
        (void)signal(SIGTTIN, SIG_IGN);
        (void)signal(SIGTTOU, SIG_IGN);

        auto c_args = cjsh_env::build_exec_argv(cmd_args);
        execvp(cmd_args[0].c_str(), c_args.data());
        int saved_errno = errno;
        _exit((saved_errno == EACCES || saved_errno == EISDIR || saved_errno == ENOEXEC) ? 126
                                                                                         : 127);
    } else {
        if (setpgid(pid, pid) < 0 && errno != EACCES && errno != EPERM) {
            set_error(ErrorType::RUNTIME_ERROR, "setpgid",
                      "failed to set process group ID for background process: " +
                          std::string(strerror(errno)));
        }

        Job job;
        job.pgid = pid;
        job.command = args[0];
        job.background = true;
        job.completed = false;
        job.stopped = false;
        job.pids.push_back(pid);

        int job_id = add_job(job);

        std::string full_command = join_arguments(args);
        JobManager::instance().add_job(pid, {pid}, full_command, true, false);
        JobManager::instance().set_last_background_pid(pid);

        std::cerr << "[" << job_id << "] " << pid << '\n';
        last_exit_code = 0;
        return 0;
    }
}

int Exec::execute_pipeline(const std::vector<Command>& commands) {
    if (commands.empty()) {
        set_error(ErrorType::INVALID_ARGUMENT, "",
                  "cannot execute empty pipeline - no commands provided", {});
        last_exit_code = EX_USAGE;
        return EX_USAGE;
    }

    if (commands.size() == 1) {
        Command cmd = commands[0];

        if (can_execute_in_process(cmd)) {
            last_exit_code = g_shell->get_built_ins()->builtin_command(cmd.args);
            return last_exit_code;
        }

        if (cmd.background) {
            return execute_command_async(cmd.args);
        }
        if (!cmd.args.empty() && g_shell && (g_shell->get_built_ins() != nullptr) &&
            (g_shell->get_built_ins()->is_builtin_command(cmd.args[0]) != 0)) {
            return execute_builtin_with_redirections(cmd);
        }

        ProcessSubstitutionResources proc_resources;
        try {
            proc_resources = setup_process_substitutions(cmd);
        } catch (const std::exception& e) {
            set_error(ErrorType::RUNTIME_ERROR, cmd.args.empty() ? "command" : cmd.args[0],
                      std::string(e.what()));
            last_exit_code = EX_OSERR;
            return EX_OSERR;
        }

        pid_t pid = fork();

        if (pid == -1) {
            cleanup_process_substitutions(proc_resources, true);
            set_error(ErrorType::RUNTIME_ERROR, cmd.args.empty() ? "unknown" : cmd.args[0],
                      "failed to fork process: " + std::string(strerror(errno)));
            last_exit_code = EX_OSERR;
            return EX_OSERR;
        }

        if (pid == 0) {
            pid_t child_pid = getpid();
            if (setpgid(child_pid, child_pid) < 0) {
                std::cerr << "cjsh: runtime error: setpgid: failed to set "
                             "process "
                             "group ID in child: "
                          << strerror(errno) << '\n';
                _exit(EXIT_FAILURE);
            }

            maybe_set_foreground_terminal(
                shell_is_interactive, shell_terminal, child_pid, [&](int err) {
                    std::cerr << "cjsh: runtime error: tcsetpgrp: failed to set "
                                 "terminal foreground process group in child: "
                              << strerror(err) << '\n';
                });

            reset_child_signals();

            if (!cmd.here_doc.empty()) {
                auto here_doc_error = [&](HereDocErrorKind kind, const std::string& detail) {
                    switch (kind) {
                        case HereDocErrorKind::Pipe:
                            std::cerr << "cjsh: runtime error: pipe: failed to secure here "
                                         "document pipe: "
                                      << detail << '\n';
                            break;
                        case HereDocErrorKind::ContentWrite:
                            std::cerr << "cjsh: runtime error: write: failed to write here "
                                         "document content: "
                                      << detail << '\n';
                            break;
                        case HereDocErrorKind::NewlineWrite:
                            std::cerr << "cjsh: runtime error: write: failed to write here "
                                         "document newline: "
                                      << detail << '\n';
                            break;
                        case HereDocErrorKind::Duplication:
                            std::cerr << "cjsh: runtime error: dup2: failed to duplicate here "
                                         "document descriptor: "
                                      << detail << '\n';
                            break;
                    }
                    _exit(EXIT_FAILURE);
                };

                if (!setup_here_document_stdin(cmd.here_doc, here_doc_error)) {
                    _exit(EXIT_FAILURE);
                }
            } else if (!cmd.here_string.empty()) {
                auto here_error = cjsh_filesystem::setup_here_string_stdin(cmd.here_string);
                if (here_error.has_value()) {
                    switch (here_error->type) {
                        case cjsh_filesystem::HereStringErrorType::Pipe:
                            std::cerr << "cjsh: runtime error: pipe: "
                                         "failed to create pipe for "
                                         "here string: "
                                      << here_error->detail << '\n';
                            break;
                        case cjsh_filesystem::HereStringErrorType::Write:
                            std::cerr << "cjsh: runtime error: write: "
                                         "failed to write here "
                                         "string content: "
                                      << here_error->detail << '\n';
                            break;
                        case cjsh_filesystem::HereStringErrorType::Dup:
                            std::cerr << "cjsh: runtime error: dup2: "
                                         "failed to duplicate here "
                                         "string descriptor: "
                                      << here_error->detail << '\n';
                            break;
                    }
                    _exit(EXIT_FAILURE);
                }
            } else if (!cmd.input_file.empty()) {
                auto redirect_result =
                    cjsh_filesystem::redirect_fd(cmd.input_file, STDIN_FILENO, O_RDONLY);
                if (redirect_result.is_error()) {
                    std::cerr << "cjsh: file not found: " << cmd.input_file << ": "
                              << redirect_result.error() << '\n';
                    _exit(EXIT_FAILURE);
                }
            }

            if (!cmd.output_file.empty()) {
                if (cjsh_filesystem::should_noclobber_prevent_overwrite(cmd.output_file,
                                                                        cmd.force_overwrite)) {
                    std::cerr << "cjsh: permission denied: " << cmd.output_file
                              << ": cannot overwrite existing file (noclobber is "
                                 "set)"
                              << '\n';
                    _exit(EXIT_FAILURE);
                }

                auto redirect_result = cjsh_filesystem::redirect_fd(cmd.output_file, STDOUT_FILENO,
                                                                    O_WRONLY | O_CREAT | O_TRUNC);
                if (redirect_result.is_error()) {
                    std::cerr << "cjsh: file not found: " << cmd.output_file << ": "
                              << redirect_result.error() << '\n';
                    _exit(EXIT_FAILURE);
                }
            }

            if (cmd.both_output && !cmd.both_output_file.empty()) {
                if (cjsh_filesystem::should_noclobber_prevent_overwrite(cmd.both_output_file)) {
                    std::cerr << "cjsh: permission denied: " << cmd.both_output_file
                              << ": cannot overwrite existing file "
                                 "(noclobber is set)"
                              << '\n';
                    _exit(EXIT_FAILURE);
                }

                auto stdout_result = cjsh_filesystem::redirect_fd(
                    cmd.both_output_file, STDOUT_FILENO, O_WRONLY | O_CREAT | O_TRUNC);
                if (stdout_result.is_error()) {
                    std::cerr << "cjsh: file not found: " << cmd.both_output_file << ": "
                              << stdout_result.error() << '\n';
                    _exit(EXIT_FAILURE);
                }

                auto stderr_result = cjsh_filesystem::safe_dup2(STDOUT_FILENO, STDERR_FILENO);
                if (stderr_result.is_error()) {
                    std::cerr << "cjsh: runtime error: dup2: failed for "
                                 "stderr in &> "
                                 "redirection: "
                              << stderr_result.error() << '\n';
                    _exit(EXIT_FAILURE);
                }
            }

            if (!cmd.append_file.empty()) {
                int fd = open(cmd.append_file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd == -1) {
                    std::cerr << "cjsh: file not found: " << cmd.append_file << ": "
                              << strerror(errno) << '\n';
                    _exit(EXIT_FAILURE);
                }
                if (dup2(fd, STDOUT_FILENO) == -1) {
                    std::cerr << "cjsh: runtime error: dup2: failed for "
                                 "append redirection: "
                              << strerror(errno) << '\n';
                    close(fd);
                    _exit(EXIT_FAILURE);
                }
                close(fd);
            }

            auto stream_error = [&](const StreamRedirectError& error) {
                switch (error.kind) {
                    case StreamRedirectErrorKind::Noclobber:
                        std::cerr << "cjsh: permission denied: " << error.target << ": "
                                  << error.detail << '\n';
                        break;
                    case StreamRedirectErrorKind::Redirect:
                        std::cerr << "cjsh: " << error.target << ": " << error.detail << '\n';
                        break;
                    case StreamRedirectErrorKind::Duplication:
                        if (error.src_fd == STDOUT_FILENO && error.dst_fd == STDERR_FILENO) {
                            std::cerr << "cjsh: dup2 2>&1: " << error.detail << '\n';
                        } else {
                            std::cerr << "cjsh: dup2 >&2: " << error.detail << '\n';
                        }
                        break;
                }
                _exit(EXIT_FAILURE);
            };

            if (!configure_stderr_redirects(cmd, stream_error)) {
                _exit(EXIT_FAILURE);
            }

            auto fd_failure = [&](const FdOperationError& error) {
                if (error.type == FdOperationErrorType::Redirect) {
                    std::cerr << "cjsh: file not found: " << error.spec << ": " << error.error
                              << '\n';
                } else {
                    std::cerr << "cjsh: runtime error: dup2: failed for " << error.fd_num << ">&"
                              << error.src_fd << ": " << error.error << '\n';
                }
                _exit(EXIT_FAILURE);
            };

            if (!apply_fd_operations(cmd, fd_failure)) {
                _exit(EXIT_FAILURE);
            }

            exec_external_child(cmd.args);
        }

        if (g_shell && !g_shell->get_interactive_mode()) {
            int status = 0;
            pid_t wpid = waitpid(pid, &status, 0);
            while (wpid == -1 && errno == EINTR) {
                wpid = waitpid(pid, &status, 0);
            }

            int exit_code = (wpid == -1) ? EX_OSERR : extract_exit_code(status);

            auto exit_result = job_utils::make_exit_error_result(cmd.args[0], exit_code,
                                                                 "command completed successfully",
                                                                 "command failed with exit code ");
            set_error(exit_result.type, cmd.args[0], exit_result.message, exit_result.suggestions);
            last_exit_code = exit_code;
            cleanup_process_substitutions(proc_resources, false);
            return last_exit_code;
        }
        Job job;
        job.pgid = pid;
        job.command = cmd.args[0];
        job.background = false;
        job.completed = false;
        job.stopped = false;
        job.pids.push_back(pid);
        job.last_pid = pid;

        int job_id = add_job(job);
        std::string full_command = join_arguments(cmd.args);
        bool reads_stdin = job_utils::command_consumes_terminal_stdin(cmd);
        int managed_job_id =
            JobManager::instance().add_job(pid, {pid}, full_command, job.background, reads_stdin);
        put_job_in_foreground(job_id, false);

        if (!cmd.output_file.empty() || !cmd.append_file.empty() || !cmd.stderr_file.empty()) {
            if (std::getenv("CJSH_FORCE_SYNC") != nullptr) {
                sync();
            }
        }

        {
            std::lock_guard<std::mutex> lock(jobs_mutex);
            auto it = jobs.find(job_id);
            if (it != jobs.end() && it->second.completed) {
                JobManager::instance().remove_job(managed_job_id);
            }
        }
        cleanup_process_substitutions(proc_resources, false);
        return last_exit_code;

        return 0;
    }
    std::vector<pid_t> pids;
    pid_t pgid = 0;

    std::vector<std::array<int, 2>> pipes(commands.size() - 1);

    try {
        for (size_t i = 0; i < commands.size() - 1; i++) {
            if (pipe(pipes[i].data()) == -1) {
                set_error(ErrorType::RUNTIME_ERROR, "",
                          "failed to create pipe " + std::to_string(i + 1) +
                              " for pipeline: " + std::string(strerror(errno)));
                last_exit_code = EX_OSERR;
                return EX_OSERR;
            }
        }

        for (size_t i = 0; i < commands.size(); i++) {
            const Command& cmd = commands[i];

            if (cmd.args.empty()) {
                set_error(ErrorType::INVALID_ARGUMENT, "",
                          "command " + std::to_string(i + 1) + " in pipeline is empty");
                print_last_error();

                for (size_t j = 0; j < commands.size() - 1; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }

                return 1;
            }

            pid_t pid = fork();

            if (pid == -1) {
                std::string cmd_name = cmd.args.empty() ? "unknown" : cmd.args[0];
                set_error(ErrorType::RUNTIME_ERROR, cmd_name,
                          "failed to create process (command " + std::to_string(i + 1) +
                              " in pipeline): " + std::string(strerror(errno)));
                last_exit_code = EX_OSERR;
                return EX_OSERR;
            }

            if (pid == 0) {
                if (i == 0) {
                    pgid = getpid();
                }

                if (setpgid(0, pgid) < 0) {
                    perror("cjsh: setpgid failed in child");
                    _exit(EXIT_FAILURE);
                }

                maybe_set_foreground_terminal(
                    shell_is_interactive && i == 0, shell_terminal, pgid, [&](int err) {
                        std::cerr << "cjsh: tcsetpgrp failed in child: " << strerror(err) << '\n';
                    });

                reset_child_signals();
                if (i == 0) {
                    if (!cmd.here_doc.empty()) {
                        auto here_doc_error = [&](HereDocErrorKind kind,
                                                  const std::string& detail) {
                            switch (kind) {
                                case HereDocErrorKind::Pipe:
                                    std::cerr << "cjsh: failed to create pipe for here document: "
                                              << detail << '\n';
                                    break;
                                case HereDocErrorKind::ContentWrite:
                                    std::cerr
                                        << "cjsh: failed to write here document content: " << detail
                                        << '\n';
                                    break;
                                case HereDocErrorKind::NewlineWrite:
                                    std::cerr
                                        << "cjsh: failed to write here document newline: " << detail
                                        << '\n';
                                    break;
                                case HereDocErrorKind::Duplication:
                                    std::cerr
                                        << "cjsh: failed to duplicate here document descriptor: "
                                        << detail << '\n';
                                    break;
                            }
                            _exit(EXIT_FAILURE);
                        };

                        if (!setup_here_document_stdin(cmd.here_doc, here_doc_error)) {
                            _exit(EXIT_FAILURE);
                        }
                    } else if (!cmd.input_file.empty()) {
                        int fd = open(cmd.input_file.c_str(), O_RDONLY);
                        if (fd == -1) {
                            std::cerr << "cjsh: " << cmd.input_file << ": " << strerror(errno)
                                      << '\n';
                            _exit(EXIT_FAILURE);
                        }
                        if (dup2(fd, STDIN_FILENO) == -1) {
                            perror("cjsh: dup2 input");
                            close(fd);
                            _exit(EXIT_FAILURE);
                        }
                        close(fd);
                    }
                } else {
                    if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
                        perror("cjsh: dup2 pipe input");
                        _exit(EXIT_FAILURE);
                    }
                }

                if (i == commands.size() - 1) {
                    if (!cmd.output_file.empty()) {
                        int fd = open(cmd.output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        if (fd == -1) {
                            std::cerr << "cjsh: " << cmd.output_file << ": " << strerror(errno)
                                      << '\n';
                            _exit(EXIT_FAILURE);
                        }
                        if (dup2(fd, STDOUT_FILENO) == -1) {
                            perror("cjsh: dup2 output");
                            close(fd);
                            _exit(EXIT_FAILURE);
                        }
                        close(fd);
                    } else if (!cmd.append_file.empty()) {
                        int fd = open(cmd.append_file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                        if (fd == -1) {
                            std::cerr << "cjsh: " << cmd.append_file << ": " << strerror(errno)
                                      << '\n';
                            _exit(EXIT_FAILURE);
                        }
                        if (dup2(fd, STDOUT_FILENO) == -1) {
                            perror("cjsh: dup2 append");
                            close(fd);
                            _exit(EXIT_FAILURE);
                        }
                        close(fd);
                    }
                } else {
                    if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                        perror("cjsh: dup2 pipe output");
                        _exit(EXIT_FAILURE);
                    }
                }

                auto stream_error = [&](const StreamRedirectError& error) {
                    switch (error.kind) {
                        case StreamRedirectErrorKind::Noclobber:
                            std::cerr << "cjsh: permission denied: " << error.target << ": "
                                      << error.detail << '\n';
                            break;
                        case StreamRedirectErrorKind::Redirect:
                            std::cerr << "cjsh: " << error.target << ": " << error.detail << '\n';
                            break;
                        case StreamRedirectErrorKind::Duplication:
                            if (error.src_fd == STDOUT_FILENO && error.dst_fd == STDERR_FILENO) {
                                std::cerr << "cjsh: dup2 2>&1: " << error.detail << '\n';
                            } else {
                                std::cerr << "cjsh: dup2 >&2: " << error.detail << '\n';
                            }
                            break;
                    }
                    _exit(EXIT_FAILURE);
                };

                if (!configure_stderr_redirects(cmd, stream_error)) {
                    _exit(EXIT_FAILURE);
                }

                auto fd_failure = [&](const FdOperationError& error) {
                    if (error.type == FdOperationErrorType::Redirect) {
                        std::cerr << "cjsh: file not found: " << error.spec << ": " << error.error
                                  << '\n';
                    } else {
                        std::cerr << "cjsh: runtime error: dup2: failed for " << error.fd_num
                                  << ">&" << error.src_fd << ": " << error.error << '\n';
                    }
                    _exit(EXIT_FAILURE);
                };

                if (!apply_fd_operations(cmd, fd_failure)) {
                    _exit(EXIT_FAILURE);
                }

                for (size_t j = 0; j < commands.size() - 1; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }

                if (is_shell_control_structure(cmd)) {
                    int exit_code = 1;
                    if (g_shell) {
                        exit_code = g_shell->execute(command_text_for_interpretation(cmd));
                    }
                    (void)fflush(stdout);
                    (void)fflush(stderr);
                    _exit(exit_code);
                } else if (g_shell && (g_shell->get_built_ins() != nullptr) &&
                           (g_shell->get_built_ins()->is_builtin_command(cmd.args[0]) != 0)) {
                    int exit_code = g_shell->get_built_ins()->builtin_command(cmd.args);

                    (void)fflush(stdout);
                    (void)fflush(stderr);

                    _exit(exit_code);
                } else {
                    exec_external_child(cmd.args);
                }
            }

            if (i == 0) {
                pgid = pid;
            }

            if (setpgid(pid, pgid) < 0) {
                if (errno != EACCES && errno != EPERM) {
                    set_error(ErrorType::RUNTIME_ERROR, "setpgid",
                              "failed to set process group ID in pipeline parent: " +
                                  std::string(strerror(errno)));
                }
            }

            pids.push_back(pid);
        }

        for (size_t i = 0; i < commands.size() - 1; i++) {
            close(pipes[i][0]);
            close(pipes[i][1]);
        }

    } catch (const std::exception& e) {
        set_error(ErrorType::RUNTIME_ERROR, "pipeline",
                  "Error executing pipeline: " + std::string(e.what()));
        print_last_error();
        for (pid_t pid : pids) {
            kill(pid, SIGTERM);
        }
        return 1;
    }

    Job job;
    job.pgid = pgid;
    job.command = commands[0].args[0] + " | ...";
    job.background = commands.back().background;
    job.completed = false;
    job.stopped = false;
    job.pids = pids;
    job.last_pid = pids.empty() ? -1 : pids.back();

    int job_id = add_job(job);

    std::string pipeline_command;
    for (size_t i = 0; i < commands.size(); ++i) {
        if (i > 0)
            pipeline_command += " | ";
        for (size_t j = 0; j < commands[i].args.size(); ++j) {
            if (j > 0)
                pipeline_command += " ";
            pipeline_command += commands[i].args[j];
        }
    }
    int new_job_id =
        JobManager::instance().add_job(pgid, pids, pipeline_command, job.background,
                                       job_utils::pipeline_consumes_terminal_stdin(commands));

    if (job.background) {
        JobManager::instance().set_last_background_pid(pids.empty() ? -1 : pids.back());
    }

    if (job.background) {
        put_job_in_background(job_id, false);
        std::cerr << "[" << job_id << "] " << pgid << '\n';
        last_exit_code = 0;
    } else {
        put_job_in_foreground(job_id, false);

        std::lock_guard<std::mutex> lock(jobs_mutex);
        auto it = jobs.find(job_id);
        if (it != jobs.end()) {
            JobManager::instance().remove_job(new_job_id);
            last_exit_code = extract_exit_code(it->second.last_status);
        }
    }

    return last_exit_code;
}

void Exec::report_missing_job(int job_id) {
    set_error(ErrorType::RUNTIME_ERROR, "job", "job [" + std::to_string(job_id) + "] not found");
    print_last_error();
}

int Exec::add_job(const Job& job) {
    std::lock_guard<std::mutex> lock(jobs_mutex);

    int job_id = next_job_id++;
    jobs[job_id] = job;

    return job_id;
}

void Exec::remove_job(int job_id) {
    std::lock_guard<std::mutex> lock(jobs_mutex);

    auto it = jobs.find(job_id);
    if (it != jobs.end()) {
        jobs.erase(it);
    }
}

void Exec::update_job_status(int job_id, bool completed, bool stopped, int status) {
    std::lock_guard<std::mutex> lock(jobs_mutex);

    auto it = jobs.find(job_id);
    if (it != jobs.end()) {
        it->second.completed = completed;
        it->second.stopped = stopped;
        it->second.status = status;
    }
}

void Exec::put_job_in_foreground(int job_id, bool cont) {
    std::lock_guard<std::mutex> lock(jobs_mutex);

    auto it = jobs.find(job_id);
    if (it == jobs.end()) {
        report_missing_job(job_id);
        return;
    }

    Job& job = it->second;

    bool terminal_control_acquired = false;
    if (shell_is_interactive && (isatty(shell_terminal) != 0)) {
        if (tcsetpgrp(shell_terminal, job.pgid) == 0) {
            terminal_control_acquired = true;
        } else {
            if (errno != ENOTTY && errno != EINVAL && errno != EPERM) {
                set_error(ErrorType::RUNTIME_ERROR, "tcsetpgrp",
                          "warning: failed to set terminal control to job: " +
                              std::string(strerror(errno)));
            }
        }
    }

    if (cont && job.stopped) {
        if (kill(-job.pgid, SIGCONT) < 0) {
            set_error(ErrorType::RUNTIME_ERROR, "kill",
                      "failed to send SIGCONT to job: " + std::string(strerror(errno)));
        }
        job.stopped = false;
    }

    jobs_mutex.unlock();

    wait_for_job(job_id);

    jobs_mutex.lock();

    if (terminal_control_acquired && shell_is_interactive && (isatty(shell_terminal) != 0)) {
        if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
            if (errno != ENOTTY && errno != EINVAL) {
                set_error(
                    ErrorType::RUNTIME_ERROR, "tcsetpgrp",
                    "warning: failed to restore terminal control: " + std::string(strerror(errno)));
            }
        }

        if (tcgetattr(shell_terminal, &shell_tmodes) == 0) {
            if (tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes) < 0) {
                set_error(ErrorType::RUNTIME_ERROR, "tcsetattr",
                          "failed to restore terminal attributes: " + std::string(strerror(errno)));
            }
        }
    }
}

void Exec::put_job_in_background(int job_id, bool cont) {
    std::lock_guard<std::mutex> lock(jobs_mutex);

    auto it = jobs.find(job_id);
    if (it == jobs.end()) {
        report_missing_job(job_id);
        return;
    }

    Job& job = it->second;

    if (cont && job.stopped) {
        if (kill(-job.pgid, SIGCONT) < 0) {
            set_error(ErrorType::RUNTIME_ERROR, "kill",
                      "failed to send SIGCONT to background job: " + std::string(strerror(errno)));
        }

        job.stopped = false;
    }
}

void Exec::wait_for_job(int job_id) {
    std::unique_lock<std::mutex> lock(jobs_mutex);

    auto it = jobs.find(job_id);
    if (it == jobs.end()) {
        return;
    }

    pid_t job_pgid = it->second.pgid;
    std::vector<pid_t> remaining_pids = it->second.pids;
    pid_t last_pid = it->second.last_pid;

    lock.unlock();

    int status = 0;
    pid_t pid = 0;

    bool job_stopped = false;
    bool saw_last = false;
    int last_status = 0;

    while (!remaining_pids.empty()) {
        pid = waitpid(-job_pgid, &status, WUNTRACED);

        if (pid == -1) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == ECHILD) {
                remaining_pids.clear();
                break;
            }
            set_error(ErrorType::RUNTIME_ERROR, "waitpid",
                      "failed to wait for child process: " + std::string(strerror(errno)));
            break;
        }

        auto pid_it = std::find(remaining_pids.begin(), remaining_pids.end(), pid);
        if (pid_it != remaining_pids.end()) {
            remaining_pids.erase(pid_it);
        }

        if (pid == last_pid) {
        }

        if (pid == last_pid) {
            saw_last = true;
            last_status = status;
        }

        if (WIFSTOPPED(status)) {
            job_stopped = true;
            break;
        }
    }

    lock.lock();

    it = jobs.find(job_id);
    if (it != jobs.end()) {
        Job& job = it->second;

        if (job_stopped) {
            job.stopped = true;
            job.status = status;
            last_exit_code = 128 + SIGTSTP;
        } else {
            job.completed = true;
            job.stopped = false;
            job.status = status;

            int final_status = saw_last ? last_status : status;
            if (!(WIFEXITED(final_status) || WIFSIGNALED(final_status))) {
                final_status = (job.last_status != 0) ? job.last_status : job.status;
            }
            job.last_status = final_status;

            if (WIFEXITED(final_status)) {
                int exit_status = WEXITSTATUS(final_status);
                last_exit_code = exit_status;
                job.completed = true;
                auto exit_result = job_utils::make_exit_error_result(
                    job.command, exit_status, "command completed successfully",
                    "command failed with exit code ");
                set_error(exit_result.type, job.command, exit_result.message,
                          exit_result.suggestions);
            } else if (WIFSIGNALED(final_status)) {
                last_exit_code = 128 + WTERMSIG(final_status);
                set_error(ErrorType::RUNTIME_ERROR, job.command,
                          "command terminated by signal " + std::to_string(WTERMSIG(final_status)));
            }
        }
    }
}

void Exec::terminate_all_child_process() {
    std::lock_guard<std::mutex> lock(jobs_mutex);

    bool any_jobs_terminated = false;
    for (auto& job_pair : jobs) {
        Job& job = job_pair.second;
        if (!job.completed) {
            if (killpg(job.pgid, 0) == 0) {
                if (killpg(job.pgid, SIGTERM) == 0) {
                    any_jobs_terminated = true;
                } else {
                    if (errno != ESRCH) {
                        std::cerr << "cjsh: warning: failed to terminate job [" << job_pair.first
                                  << "] '" << job.command << "': " << strerror(errno) << '\n';
                    }
                }
                std::cerr << "[" << job_pair.first << "] Terminated\t" << job.command << '\n';
            }
        }
    }

    if (any_jobs_terminated) {
        usleep(200000);
    }

    for (auto& job_pair : jobs) {
        Job& job = job_pair.second;
        if (!job.completed) {
            if (killpg(job.pgid, 0) == 0) {
                if (killpg(job.pgid, SIGKILL) == 0) {
                } else {
                    if (errno != ESRCH) {
                        set_error(ErrorType::RUNTIME_ERROR, "killpg",
                                  "failed to send SIGKILL in "
                                  "terminate_all_child_process: " +
                                      std::string(strerror(errno)));
                    }
                }

                for (pid_t pid : job.pids) {
                    kill(pid, SIGKILL);
                }
            }

            job.completed = true;
            job.stopped = false;
            job.status = 0;
        }
    }

    int status = 0;
    pid_t pid = 0;
    int zombie_count = 0;
    const int max_terminate_iterations = 50;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0 && zombie_count < max_terminate_iterations) {
        zombie_count++;
    }

    if (zombie_count >= max_terminate_iterations) {
        std::cerr << "WARNING: terminate_all_child_process hit maximum cleanup "
                     "iterations"
                  << '\n';
    }

    set_error(ErrorType::RUNTIME_ERROR, "", "All child processes terminated");
}

std::map<int, Job> Exec::get_jobs() {
    std::lock_guard<std::mutex> lock(jobs_mutex);
    return jobs;
}

namespace exec_utils {

static CommandOutput execute_args_for_output_impl(const std::vector<std::string>& args) {
    CommandOutput result{"", -1, false};

    if (args.empty()) {
        return result;
    }

    int pipefd[2];
    auto pipe_result = cjsh_filesystem::create_pipe_cloexec(pipefd);
    if (pipe_result.is_error()) {
        return result;
    }

    pid_t pid = fork();
    if (pid == -1) {
        cjsh_filesystem::close_pipe(pipefd);
        return result;
    }

    if (pid == 0) {
        cjsh_filesystem::safe_close(pipefd[0]);

        auto dup_result = cjsh_filesystem::safe_dup2(pipefd[1], STDOUT_FILENO);
        if (dup_result.is_error()) {
            _exit(127);
        }

        auto devnull_result = cjsh_filesystem::safe_open("/dev/null", O_WRONLY);
        if (devnull_result.is_ok()) {
            cjsh_filesystem::safe_dup2(devnull_result.value(), STDERR_FILENO);
            cjsh_filesystem::safe_close(devnull_result.value());
        }

        cjsh_filesystem::safe_close(pipefd[1]);

        std::vector<char*> argv;
        std::vector<std::unique_ptr<char[]>> arg_storage;
        argv.reserve(args.size() + 1);
        arg_storage.reserve(args.size());

        for (const auto& arg : args) {
            auto str_copy = std::make_unique<char[]>(arg.size() + 1);
            std::strcpy(str_copy.get(), arg.c_str());
            argv.push_back(str_copy.get());
            arg_storage.push_back(std::move(str_copy));
        }
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        _exit(127);
    }

    cjsh_filesystem::safe_close(pipefd[1]);

    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        result.output += buffer;
    }

    cjsh_filesystem::safe_close(pipefd[0]);

    int status;
    if (waitpid(pid, &status, 0) == -1) {
        return result;
    }

    result.exit_code = extract_exit_code(status);
    result.success = (result.exit_code == 0);
    return result;
}

CommandOutput execute_command_for_output(const std::string& command) {
    std::vector<std::string> args = cjsh_env::parse_shell_command(command);
    return execute_args_for_output_impl(args);
}

CommandOutput execute_command_vector_for_output(const std::vector<std::string>& args) {
    return execute_args_for_output_impl(args);
}

}  // namespace exec_utils
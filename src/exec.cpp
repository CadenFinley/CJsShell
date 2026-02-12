/*
  exec.cpp

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

#include "exec.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <unistd.h>

#include <stdlib.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
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
#include <thread>
#include <vector>

#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "interpreter.h"
#include "job_control.h"
#include "parser.h"
#include "script_dispatch.h"
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

[[noreturn]] void exec_external_child(const std::vector<std::string>& args,
                                      const char* cached_path);

struct PtyPair {
    int master_fd{-1};
    int slave_fd{-1};
};

std::optional<PtyPair> create_output_pty(int terminal_fd) {
    int master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (master_fd < 0) {
        return std::nullopt;
    }
    if (grantpt(master_fd) < 0 || unlockpt(master_fd) < 0) {
        cjsh_filesystem::safe_close(master_fd);
        return std::nullopt;
    }
    char* slave_name = ptsname(master_fd);
    if (slave_name == nullptr) {
        cjsh_filesystem::safe_close(master_fd);
        return std::nullopt;
    }

    int slave_fd = open(slave_name, O_RDWR | O_NOCTTY);
    if (slave_fd < 0) {
        cjsh_filesystem::safe_close(master_fd);
        return std::nullopt;
    }

    struct termios term_state{};
    if (tcgetattr(terminal_fd, &term_state) == 0) {
        (void)tcsetattr(slave_fd, TCSANOW, &term_state);
    }

    struct winsize ws{};
    if (ioctl(terminal_fd, TIOCGWINSZ, &ws) == 0) {
        (void)ioctl(slave_fd, TIOCSWINSZ, &ws);
    }

    (void)cjsh_filesystem::set_close_on_exec(master_fd);

    return PtyPair{master_fd, slave_fd};
}

std::shared_ptr<OutputRelayState> start_output_relay(int master_fd, bool forward) {
    auto relay = std::make_shared<OutputRelayState>();
    relay->master_fd = master_fd;
    relay->forward.store(forward);

    try {
        std::thread t([relay]() {
            char buffer[4096];
            while (true) {
                ssize_t bytes_read = read(relay->master_fd, buffer, sizeof(buffer));
                if (bytes_read == 0) {
                    break;
                }
                if (bytes_read < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    break;
                }
                if (relay->forward.load()) {
                    (void)cjsh_filesystem::write_all(
                        STDOUT_FILENO, std::string_view(buffer, static_cast<size_t>(bytes_read)));
                }
            }
            cjsh_filesystem::safe_close(relay->master_fd);
        });
        t.detach();
    } catch (...) {
        cjsh_filesystem::safe_close(relay->master_fd);
        throw;
    }

    return relay;
}

struct CommandExecutionPlan {
    bool is_builtin{false};
    std::string cached_exec_path;
};

CommandExecutionPlan resolve_command_exec_plan(const std::vector<std::string>& cmd_args) {
    CommandExecutionPlan plan;
    if (!cmd_args.empty() && g_shell && (g_shell->get_built_ins() != nullptr)) {
        plan.is_builtin = g_shell->get_built_ins()->is_builtin_command(cmd_args[0]) != 0;
    }

    if (!cmd_args.empty() && !plan.is_builtin) {
        plan.cached_exec_path = cjsh_filesystem::resolve_executable_for_execution(cmd_args[0]);
    }

    return plan;
}

[[noreturn]] void exec_builtin_or_external_child(const std::vector<std::string>& cmd_args,
                                                 bool is_builtin,
                                                 const std::string& cached_exec_path) {
    if (is_builtin && g_shell && (g_shell->get_built_ins() != nullptr)) {
        int exit_code = g_shell->get_built_ins()->builtin_command(cmd_args);
        (void)fflush(stdout);
        (void)fflush(stderr);
        _exit(exit_code);
    }

    const char* exec_override = cached_exec_path.empty() ? nullptr : cached_exec_path.c_str();
    exec_external_child(cmd_args, exec_override);
}

void attach_output_relay_to_job(Job& job, const std::optional<PtyPair>& output_pty,
                                std::shared_ptr<OutputRelayState>& output_relay) {
    if (!output_pty.has_value()) {
        return;
    }

    cjsh_filesystem::safe_close(output_pty->slave_fd);
    output_relay = start_output_relay(output_pty->master_fd, !job.background);
    job.output_relay = output_relay;
}

bool command_has_stdout_redirection(const Command& cmd) {
    return !cmd.output_file.empty() || !cmd.append_file.empty() || cmd.both_output ||
           cmd.stdout_to_stderr || cmd.has_fd_redirection(STDOUT_FILENO) ||
           cmd.has_fd_duplication(STDOUT_FILENO);
}

bool command_has_stderr_redirection(const Command& cmd) {
    return !cmd.stderr_file.empty() || cmd.stderr_to_stdout || cmd.both_output ||
           cmd.has_fd_redirection(STDERR_FILENO) || cmd.has_fd_duplication(STDERR_FILENO);
}

void apply_assignments_to_shell_env(
    const std::vector<std::pair<std::string, std::string>>& assignments) {
    if (!g_shell || assignments.empty()) {
        return;
    }

    auto& env_vars = cjsh_env::env_vars();
    for (const auto& env : assignments) {
        env_vars[env.first] = env.second;

        if (env.first == "PATH" || env.first == "PWD" || env.first == "HOME" ||
            env.first == "USER" || env.first == "SHELL") {
            setenv(env.first.c_str(), env.second.c_str(), 1);
        }
    }

    cjsh_env::sync_parser_env_vars(g_shell.get());
}

Job make_single_process_job(pid_t pid, const std::string& command, bool background,
                            bool auto_background_on_stop, bool auto_background_on_stop_silent) {
    Job job;
    job.pgid = pid;
    job.command = command;
    job.background = background;
    job.auto_background_on_stop = auto_background_on_stop;
    job.auto_background_on_stop_silent = auto_background_on_stop_silent;
    job.completed = false;
    job.stopped = false;
    job.pids.push_back(pid);
    job.last_pid = pid;
    job.pid_order.push_back(pid);
    job.pipeline_statuses.assign(1, -1);
    return job;
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

ErrorType classify_filesystem_error(int err) {
    switch (err) {
        case ENOENT:
            return ErrorType::FILE_NOT_FOUND;
        case EACCES:
            return ErrorType::PERMISSION_DENIED;
        default:
            return ErrorType::RUNTIME_ERROR;
    }
}

[[noreturn]] void child_exit_with_error(ErrorType type, const std::string& command,
                                        const std::string& message) {
    ErrorInfo info{type, ErrorSeverity::ERROR, command, message, {}};
    print_error(info);
    _exit(EXIT_FAILURE);
}

bool replace_first_instance(std::string& target, const std::string& from, const std::string& to) {
    size_t pos = target.find(from);
    if (pos == std::string::npos) {
        return false;
    }
    target.replace(pos, from.length(), to);
    return true;
}

[[noreturn]] void report_exec_failure(const std::vector<std::string>& args, int saved_errno) {
    const std::string command_name = args.empty() ? std::string{} : args[0];

    if (saved_errno == ENOENT) {
        const bool has_explicit_path = command_name.find('/') != std::string::npos;
        if (has_explicit_path) {
            const char* err_detail = strerror(saved_errno);
            std::string message_detail = err_detail ? err_detail : "no such file or directory";
            print_error({ErrorType::FILE_NOT_FOUND,
                         ErrorSeverity::ERROR,
                         command_name,
                         message_detail,
                         {"try checking the file path or creating the file."}});
            _exit(ShellScriptInterpreter::exit_command_not_found);
        }

        std::vector<std::string> suggestions;
        if (!command_name.empty()) {
            suggestions = suggestion_utils::generate_command_suggestions(command_name);
        }
        print_error(
            {ErrorType::COMMAND_NOT_FOUND, ErrorSeverity::ERROR, command_name, "", suggestions});
        _exit(ShellScriptInterpreter::exit_command_not_found);
    }

    const bool permission_error = (saved_errno == EACCES || saved_errno == EISDIR);
    const bool exec_format_error = (saved_errno == ENOEXEC);
    const int exit_code = (permission_error || exec_format_error) ? 126 : 127;

    std::string detail = permission_error    ? "permission denied"
                         : exec_format_error ? "exec format error"
                                             : "execution failed";

    std::string message_detail;
    if (!permission_error && !exec_format_error) {
        message_detail = detail;
        if (saved_errno != 0) {
            message_detail += ": ";
            message_detail += strerror(saved_errno);
        }
    }

    ErrorType error_type = permission_error    ? ErrorType::PERMISSION_DENIED
                           : exec_format_error ? ErrorType::INVALID_ARGUMENT
                                               : ErrorType::RUNTIME_ERROR;

    std::vector<std::string> suggestions;
    if (!message_detail.empty()) {
        suggestions.push_back("Detail: " + message_detail);
    }

    print_error({error_type, ErrorSeverity::ERROR, command_name, message_detail, suggestions});
    _exit(exit_code);
}

bool strip_temporary_env_assignments(
    std::vector<std::string>& args, size_t cmd_start_idx, size_t original_arg_count,
    const std::vector<std::pair<std::string, std::string>>& assignments) {
    const bool has_temporary_env = !assignments.empty() && cmd_start_idx < original_arg_count;
    if (has_temporary_env && cmd_start_idx > 0) {
        auto erase_end =
            args.begin() + static_cast<std::vector<std::string>::difference_type>(cmd_start_idx);
        args.erase(args.begin(), erase_end);
    }
    return has_temporary_env;
}

class TemporaryEnvAssignmentScope {
   public:
    TemporaryEnvAssignmentScope(Shell* shell,
                                const std::vector<std::pair<std::string, std::string>>& assignments)
        : shell_(shell) {
        if (!shell_ || assignments.empty()) {
            return;
        }

        auto& env_vars = cjsh_env::env_vars();
        for (const auto& assignment : assignments) {
            const std::string& name = assignment.first;
            const std::string& value = assignment.second;

            if (!has_backup(name)) {
                Backup backup;
                backup.name = name;
                auto it = env_vars.find(name);
                if (it != env_vars.end()) {
                    backup.had_previous = true;
                    backup.previous_value = it->second;
                }
                backups_.push_back(std::move(backup));
            }

            env_vars[name] = value;
            setenv(name.c_str(), value.c_str(), 1);
        }

        refresh_parser_env();
    }

    ~TemporaryEnvAssignmentScope() {
        if (!shell_ || backups_.empty()) {
            return;
        }

        auto& env_vars = cjsh_env::env_vars();
        for (auto it = backups_.rbegin(); it != backups_.rend(); ++it) {
            if (it->had_previous) {
                env_vars[it->name] = it->previous_value;
                setenv(it->name.c_str(), it->previous_value.c_str(), 1);
            } else {
                env_vars.erase(it->name);
                unsetenv(it->name.c_str());
            }
        }

        refresh_parser_env();
    }

   private:
    struct Backup {
        std::string name;
        bool had_previous{false};
        std::string previous_value;
    };

    bool has_backup(const std::string& name) const {
        return std::any_of(backups_.begin(), backups_.end(),
                           [&](const Backup& entry) { return entry.name == name; });
    }

    void refresh_parser_env() {
        if (!shell_) {
            return;
        }
        cjsh_env::sync_parser_env_vars(shell_);
    }

    Shell* shell_ = nullptr;
    std::vector<Backup> backups_;
};

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
                const std::string substitution_label =
                    command.empty() ? "process substitution" : command;
                auto exit_with_error = [&](ErrorType type, const std::string& message) {
                    child_exit_with_error(type, substitution_label, message);
                };

                if (is_input) {
                    auto fifo_result = cjsh_filesystem::safe_open(fifo_path, O_WRONLY);
                    if (fifo_result.is_error()) {
                        exit_with_error(
                            ErrorType::FILE_NOT_FOUND,
                            "open: failed to open FIFO for writing: " + fifo_result.error());
                    }

                    auto dup_result =
                        cjsh_filesystem::safe_dup2(fifo_result.value(), STDOUT_FILENO);
                    if (dup_result.is_error()) {
                        exit_with_error(
                            ErrorType::RUNTIME_ERROR,
                            "dup2: failed to duplicate stdout descriptor: " + dup_result.error());
                    }

                    cjsh_filesystem::safe_close(fifo_result.value());
                } else {
                    auto fifo_result = cjsh_filesystem::safe_open(fifo_path, O_RDONLY);
                    if (fifo_result.is_error()) {
                        exit_with_error(
                            ErrorType::FILE_NOT_FOUND,
                            "open: failed to open FIFO for reading: " + fifo_result.error());
                    }

                    auto dup_result = cjsh_filesystem::safe_dup2(fifo_result.value(), STDIN_FILENO);
                    if (dup_result.is_error()) {
                        cjsh_filesystem::safe_close(fifo_result.value());
                        exit_with_error(
                            ErrorType::RUNTIME_ERROR,
                            "dup2: failed to duplicate stdin descriptor: " + dup_result.error());
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

enum class FdOperationErrorType : std::uint8_t {
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

        if ((info.flags & O_WRONLY) != 0 && (info.flags & O_TRUNC) != 0) {
            if (cjsh_filesystem::should_noclobber_prevent_overwrite(info.file)) {
                on_failure(FdOperationError{FdOperationErrorType::Redirect, fd_num, -1, spec,
                                            "cannot overwrite existing file (noclobber is set)"});
                return false;
            }
        }

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

enum class HereDocErrorKind : std::uint8_t {
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

    const auto duplicate_pipe_to_stdin = [&]() -> bool {
        auto dup_result = cjsh_filesystem::duplicate_pipe_read_end_to_fd(here_pipe, STDIN_FILENO);
        if (dup_result.is_error()) {
            on_error(HereDocErrorKind::Duplication, dup_result.error());
            return false;
        }
        return true;
    };

    auto write_result = cjsh_filesystem::write_all(here_pipe[1], std::string_view{here_doc});
    if (write_result.is_error()) {
        if (!cjsh_filesystem::error_indicates_broken_pipe(write_result.error())) {
            on_error(HereDocErrorKind::ContentWrite, write_result.error());
            cjsh_filesystem::close_pipe(here_pipe);
            return false;
        }

        if (!duplicate_pipe_to_stdin()) {
            return false;
        }
        return true;
    }

    auto newline_result = cjsh_filesystem::write_all(here_pipe[1], std::string_view("\n", 1));
    if (newline_result.is_error() &&
        !cjsh_filesystem::error_indicates_broken_pipe(newline_result.error())) {
        on_error(HereDocErrorKind::NewlineWrite, newline_result.error());
        cjsh_filesystem::close_pipe(here_pipe);
        return false;
    }

    if (!duplicate_pipe_to_stdin()) {
        return false;
    }

    return true;
}

enum class StreamRedirectErrorKind : std::uint8_t {
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

[[noreturn]] void handle_stream_redirect_error_and_exit(const StreamRedirectError& error) {
    ErrorInfo info;
    info.severity = ErrorSeverity::ERROR;

    switch (error.kind) {
        case StreamRedirectErrorKind::Noclobber:
            info.type = ErrorType::PERMISSION_DENIED;
            info.command_used = error.target.empty() ? "redirect" : error.target;
            info.message = error.detail;
            break;
        case StreamRedirectErrorKind::Redirect:
            info.type = ErrorType::RUNTIME_ERROR;
            info.command_used = error.target.empty() ? "redirect" : error.target;
            info.message = error.detail;
            break;
        case StreamRedirectErrorKind::Duplication: {
            info.type = ErrorType::RUNTIME_ERROR;
            info.command_used = "dup2";
            std::ostringstream oss;
            if (error.src_fd == STDOUT_FILENO && error.dst_fd == STDERR_FILENO) {
                oss << "2>&1 failed: " << error.detail;
            } else {
                oss << error.dst_fd << ">&" << error.src_fd << " failed: " << error.detail;
            }
            info.message = oss.str();
            break;
        }
    }

    print_error(info);
    _exit(EXIT_FAILURE);
}

[[noreturn]] void handle_fd_operation_error_and_exit(const FdOperationError& error) {
    ErrorInfo info;
    info.severity = ErrorSeverity::ERROR;
    info.command_used = error.type == FdOperationErrorType::Redirect ? error.spec : "dup2";

    if (error.type == FdOperationErrorType::Redirect) {
        info.type = ErrorType::FILE_NOT_FOUND;
        info.message = error.error;
    } else {
        info.type = ErrorType::RUNTIME_ERROR;
        info.message = "failed for " + std::to_string(error.fd_num) + ">&" +
                       std::to_string(error.src_fd) + ": " + error.error;
    }

    print_error(info);
    _exit(EXIT_FAILURE);
}

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

[[noreturn]] void exec_external_child(const std::vector<std::string>& args,
                                      const char* cached_path) {
    if (config::script_extension_interpreter_enabled) {
        auto interpreter_args =
            script_dispatch::build_extension_interpreter_args(args, cached_path);
        if (interpreter_args) {
            auto c_interp_args = cjsh_env::build_exec_argv(*interpreter_args);
            execvp((*interpreter_args)[0].c_str(), c_interp_args.data());
            int saved_errno = errno;
            report_exec_failure(*interpreter_args, saved_errno);
        }
    }
    auto c_args = cjsh_env::build_exec_argv(args);
    if (cached_path != nullptr && cached_path[0] != '\0') {
        execv(cached_path, c_args.data());
    }
    execvp(args[0].c_str(), c_args.data());
    int saved_errno = errno;
    report_exec_failure(args, saved_errno);
}

}  // namespace

Exec::Exec()
    : shell_pgid(getpid()),
      shell_terminal(STDIN_FILENO),
      shell_is_interactive(false),
      last_pipeline_statuses(1, 0) {
    bool requested_interactive = config::interactive_mode || config::force_interactive;
    shell_is_interactive = requested_interactive && (isatty(STDIN_FILENO) != 0);

#ifdef O_CLOEXEC
    int tty_fd = open("/dev/tty", O_RDWR | O_CLOEXEC);
#else
    int tty_fd = open("/dev/tty", O_RDWR);
#endif
    if (tty_fd >= 0) {
        shell_terminal = tty_fd;
        owns_shell_terminal = true;
    }

    if (shell_is_interactive && (isatty(shell_terminal) != 0)) {
        if (tcgetattr(shell_terminal, &shell_tmodes) < 0) {
            set_error(
                ErrorType::FATAL_ERROR, "tcgetattr",
                "failed to get terminal attributes in constructor: " + std::string(strerror(errno)),
                {"Try running cjsh from a terminal/TTY.",
                 "Avoid redirecting stdin when using interactive mode."});
        }
    }
}

Exec::~Exec() {
    int status = 0;
    int zombie_count = 0;
    const int max_cleanup_iterations = 50;
    while (zombie_count < max_cleanup_iterations) {
        pid_t reap_pid = waitpid(-1, &status, WNOHANG);
        if (reap_pid <= 0) {
            break;
        }
        zombie_count++;
    }

    if (zombie_count >= max_cleanup_iterations) {
        print_error({ErrorType::RUNTIME_ERROR,
                     ErrorSeverity::WARNING,
                     "exec",
                     "destructor hit maximum cleanup iterations, some zombies may remain",
                     {}});
    }

    if (owns_shell_terminal && shell_terminal >= 0) {
        close(shell_terminal);
        shell_terminal = STDIN_FILENO;
        owns_shell_terminal = false;
    }
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

bool Exec::initialize_env_assignments(const std::vector<std::string>& args,
                                      std::vector<std::pair<std::string, std::string>>& assignments,
                                      size_t& cmd_start_idx) {
    if (handle_empty_args(args)) {
        return false;
    }

    cmd_start_idx = cjsh_env::collect_env_assignments(args, assignments);
    return true;
}

std::optional<int> Exec::handle_assignments_prefix(
    const std::vector<std::string>& args,
    std::vector<std::pair<std::string, std::string>>& assignments, size_t& cmd_start_idx,
    const std::function<void()>& on_assignments_only) {
    if (!initialize_env_assignments(args, assignments, cmd_start_idx)) {
        set_last_pipeline_statuses({last_exit_code});
        return last_exit_code;
    }

    if (cmd_start_idx >= args.size()) {
        if (on_assignments_only) {
            on_assignments_only();
        }
        last_exit_code = 0;
        set_last_pipeline_statuses({0});
        return 0;
    }

    return std::nullopt;
}

std::optional<std::vector<std::string>> Exec::collect_command_args_with_assignments(
    const std::vector<std::string>& args,
    std::vector<std::pair<std::string, std::string>>& assignments,
    const std::function<void()>& on_assignments_only, int& early_exit_code) {
    size_t cmd_start_idx = 0;
    auto early_exit =
        handle_assignments_prefix(args, assignments, cmd_start_idx, on_assignments_only);
    if (early_exit.has_value()) {
        early_exit_code = early_exit.value();
        return std::nullopt;
    }

    std::vector<std::string> cmd_args(
        std::next(args.begin(), static_cast<std::ptrdiff_t>(cmd_start_idx)), args.end());
    return cmd_args;
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
        set_error(ErrorType::FATAL_ERROR, "builtin",
                  "no shell context available for builtin execution");
        last_exit_code = EX_SOFTWARE;
        return EX_SOFTWARE;
    }

    bool persist_fd_changes = (!cmd.args.empty() && cmd.args[0] == "exec" && cmd.args.size() == 1);
    bool is_exec_builtin = !cmd.args.empty() && cmd.args[0] == "exec";
    std::string command_name = cmd.args.empty() ? "builtin" : cmd.args[0];

    auto action = [&]() -> int {
        if (is_exec_builtin) {
            if (cmd.args.size() == 1) {
                return 0;
            }
            std::vector<std::string> exec_args(cmd.args.begin() + 1, cmd.args.end());
            return g_shell->execute_command(exec_args, false);
        }
        return g_shell->get_built_ins()->builtin_command(cmd.args);
    };

    bool action_invoked = false;
    int exit_code = run_with_command_redirections(cmd, action, command_name, persist_fd_changes,
                                                  &action_invoked);

    if (!action_invoked) {
        last_exit_code = exit_code;
        return exit_code;
    }

    auto exit_result = job_utils::make_exit_error_result(command_name, exit_code,
                                                         "builtin command completed successfully",
                                                         "builtin command failed with exit code ");
    set_error(exit_result.type, command_name, exit_result.message, exit_result.suggestions);
    last_exit_code = exit_code;
    return exit_code;
}

void Exec::warn_parent_setpgid_failure() {
    if (errno != EACCES && errno != ESRCH) {
        set_error(ErrorType::RUNTIME_ERROR, "setpgid",
                  "failed to set process group ID in parent: " + std::string(strerror(errno)));
    }
}

Job* Exec::find_job_locked(int job_id) {
    auto it = jobs.find(job_id);
    if (it == jobs.end()) {
        report_missing_job(job_id);
        return nullptr;
    }
    return &it->second;
}

Job* Exec::find_job_and_set_output_forwarding_locked(int job_id, bool forward) {
    Job* job = find_job_locked(job_id);
    if (job == nullptr) {
        return nullptr;
    }

    if (job->output_relay) {
        job->output_relay->forward.store(forward);
    }

    return job;
}

void Exec::report_missing_job(int job_id) {
    set_error(ErrorType::RUNTIME_ERROR, "job", "job [" + std::to_string(job_id) + "] not found");
    print_last_error();
}

void Exec::resume_job(Job& job, bool cont, std::string_view context) {
    if (!cont || !job.stopped) {
        return;
    }

    if (kill(-job.pgid, SIGCONT) < 0) {
        set_error(ErrorType::RUNTIME_ERROR, "kill",
                  "failed to send SIGCONT to " + std::string(context) + ": " +
                      std::string(strerror(errno)));
    }

    job.stopped = false;
}

void Exec::set_last_pipeline_statuses(std::vector<int> statuses) {
    last_pipeline_statuses = std::move(statuses);
}

int Exec::execute_command_sync(const std::vector<std::string>& args, bool auto_background_on_stop,
                               bool auto_background_on_stop_silent) {
    std::vector<std::pair<std::string, std::string>> env_assignments;
    int early_exit_code = 0;
    auto cmd_args = collect_command_args_with_assignments(
        args, env_assignments, [&]() { apply_assignments_to_shell_env(env_assignments); },
        early_exit_code);
    if (!cmd_args.has_value()) {
        return early_exit_code;
    }

    std::vector<std::string> cmd_args_value = std::move(cmd_args.value());

    Command proc_cmd;
    proc_cmd.args = cmd_args_value;
    for (const auto& arg : cmd_args_value) {
        if (arg.size() >= 4 && arg.back() == ')' &&
            (arg.rfind("<(", 0) == 0 || arg.rfind(">(", 0) == 0)) {
            proc_cmd.process_substitutions.push_back(arg);
        }
    }

    ProcessSubstitutionResources proc_resources;
    if (!proc_cmd.process_substitutions.empty()) {
        try {
            proc_resources = setup_process_substitutions(proc_cmd);
            cmd_args_value = proc_cmd.args;
        } catch (const std::exception& e) {
            set_error(ErrorType::RUNTIME_ERROR,
                      cmd_args_value.empty() ? "command" : cmd_args_value[0], e.what(), {});
            last_exit_code = EX_OSERR;
            set_last_pipeline_statuses({EX_OSERR});
            return EX_OSERR;
        }
    }

    auto exec_plan = resolve_command_exec_plan(cmd_args_value);
    bool is_builtin = exec_plan.is_builtin;
    std::string cached_exec_path = std::move(exec_plan.cached_exec_path);

    std::optional<PtyPair> output_pty;
    std::shared_ptr<OutputRelayState> output_relay;
    const bool wants_output_relay = shell_is_interactive && auto_background_on_stop_silent;
    if (wants_output_relay) {
        output_pty = create_output_pty(shell_terminal);
    }

    pid_t pid = fork();

    if (pid == -1) {
        set_error(ErrorType::RUNTIME_ERROR, cmd_args_value.empty() ? "unknown" : cmd_args_value[0],
                  "failed to fork process: " + std::string(strerror(errno)), {});
        if (output_pty.has_value()) {
            cjsh_filesystem::safe_close(output_pty->master_fd);
            cjsh_filesystem::safe_close(output_pty->slave_fd);
        }
        last_exit_code = EX_OSERR;
        cleanup_process_substitutions(proc_resources, true);
        set_last_pipeline_statuses({EX_OSERR});
        return EX_OSERR;
    }

    if (pid == 0) {
        cjsh_env::apply_env_assignments(env_assignments);

        pid_t child_pid = getpid();
        if (setpgid(child_pid, child_pid) < 0) {
            child_exit_with_error(
                ErrorType::RUNTIME_ERROR, cmd_args_value.empty() ? "command" : cmd_args_value[0],
                std::string("setpgid: failed to set process group ID in child: ") +
                    strerror(errno));
        }

        reset_child_signals();

        if (output_pty.has_value()) {
            if (dup2(output_pty->slave_fd, STDOUT_FILENO) == -1) {
                child_exit_with_error(
                    ErrorType::RUNTIME_ERROR,
                    cmd_args_value.empty() ? "command" : cmd_args_value[0],
                    std::string("dup2: failed to attach output relay stdout: ") + strerror(errno));
            }
            if (dup2(output_pty->slave_fd, STDERR_FILENO) == -1) {
                child_exit_with_error(
                    ErrorType::RUNTIME_ERROR,
                    cmd_args_value.empty() ? "command" : cmd_args_value[0],
                    std::string("dup2: failed to attach output relay stderr: ") + strerror(errno));
            }
            cjsh_filesystem::safe_close(output_pty->master_fd);
            cjsh_filesystem::safe_close(output_pty->slave_fd);
        }

        exec_builtin_or_external_child(cmd_args_value, is_builtin, cached_exec_path);
    }

    if (setpgid(pid, pid) < 0) {
        warn_parent_setpgid_failure();
    }

    Job job = make_single_process_job(pid, args[0], false, auto_background_on_stop,
                                      auto_background_on_stop_silent);
    attach_output_relay_to_job(job, output_pty, output_relay);

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
            // Best-effort parse; keep default reads_stdin on failure.
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
    set_last_pipeline_statuses({exit_code});

    cleanup_process_substitutions(proc_resources, false);

    return exit_code;
}

int Exec::execute_command_async(const std::vector<std::string>& args) {
    std::vector<std::pair<std::string, std::string>> env_assignments;
    int early_exit_code = 0;
    auto cmd_args = collect_command_args_with_assignments(
        args, env_assignments,
        [&]() {
            cjsh_env::apply_env_assignments(env_assignments);
            set_error(ErrorType::RUNTIME_ERROR, "", "Environment variables set", {});
        },
        early_exit_code);
    if (!cmd_args.has_value()) {
        return early_exit_code;
    }

    std::vector<std::string> cmd_args_value = std::move(cmd_args.value());

    auto exec_plan = resolve_command_exec_plan(cmd_args_value);
    bool is_builtin = exec_plan.is_builtin;
    std::string cached_exec_path = std::move(exec_plan.cached_exec_path);

    pid_t pid = fork();

    if (pid == -1) {
        std::string cmd_name = cmd_args_value.empty() ? "unknown" : cmd_args_value[0];
        set_error(ErrorType::RUNTIME_ERROR, cmd_name,
                  "failed to create background process: " + std::string(strerror(errno)), {});
        last_exit_code = EX_OSERR;
        set_last_pipeline_statuses({EX_OSERR});
        return EX_OSERR;
    }

    if (pid == 0) {
        cjsh_env::apply_env_assignments(env_assignments);

        if (setpgid(0, 0) < 0) {
            child_exit_with_error(
                ErrorType::RUNTIME_ERROR, cmd_args_value.empty() ? "command" : cmd_args_value[0],
                std::string("setpgid: failed to set process group ID in background child: ") +
                    strerror(errno));
        }

        reset_child_signals();

        exec_builtin_or_external_child(cmd_args_value, is_builtin, cached_exec_path);
    } else {
        if (setpgid(pid, pid) < 0 && errno != EACCES && errno != EPERM) {
            set_error(ErrorType::RUNTIME_ERROR, "setpgid",
                      "failed to set process group ID for background process: " +
                          std::string(strerror(errno)));
        }

        Job job = make_single_process_job(pid, args[0], true, false, false);

        int job_id = add_job(job);

        std::string full_command = join_arguments(args);
        JobManager::instance().add_job(pid, {pid}, full_command, true, false);
        JobManager::instance().set_last_background_pid(pid);

        std::cerr << "[" << job_id << "] " << pid << " " << job.command << '\n';
        last_exit_code = 0;
        return 0;
    }
}

int Exec::execute_pipeline(const std::vector<Command>& commands) {
    const bool pipeline_negated = (!commands.empty() && commands[0].negate_pipeline);

    auto apply_pipefail = [&](int exit_code, const std::vector<int>& statuses) -> int {
        if (!g_shell || !g_shell->get_shell_option(ShellOption::Pipefail)) {
            return exit_code;
        }
        if (statuses.empty()) {
            return exit_code;
        }
        int pipefail_exit = 0;
        for (int status : statuses) {
            if (status > 0) {
                pipefail_exit = status;
            }
        }
        return pipefail_exit;
    };

    auto finalize_exit = [&](int exit_code) -> int {
        int effective = exit_code;
        if (pipeline_negated) {
            effective = (exit_code == 0) ? 1 : 0;
        }
        last_exit_code = effective;
        return effective;
    };

    if (commands.empty()) {
        set_error(ErrorType::INVALID_ARGUMENT, "",
                  "cannot execute empty pipeline - no commands provided", {});
        set_last_pipeline_statuses({EX_USAGE});
        return finalize_exit(EX_USAGE);
    }

    if (g_shell && g_shell->get_shell_option(ShellOption::Noexec)) {
        const bool is_background = commands.back().background;
        if (!is_background) {
            set_last_pipeline_statuses(std::vector<int>(commands.size(), 0));
        }
        return finalize_exit(0);
    }

    if (commands.size() == 1) {
        Command cmd = commands[0];
        std::vector<std::pair<std::string, std::string>> env_assignments;
        size_t cmd_start_idx = cjsh_env::collect_env_assignments(cmd.args, env_assignments);
        const size_t original_arg_count = cmd.args.size();

        if (cmd_start_idx >= original_arg_count) {
            apply_assignments_to_shell_env(env_assignments);
            set_last_pipeline_statuses({0});
            return finalize_exit(0);
        }

        const bool has_temporary_env = strip_temporary_env_assignments(
            cmd.args, cmd_start_idx, original_arg_count, env_assignments);

        if (can_execute_in_process(cmd)) {
            TemporaryEnvAssignmentScope temp_scope(g_shell.get(), env_assignments);
            int exit_code = g_shell->get_built_ins()->builtin_command(cmd.args);
            set_last_pipeline_statuses({exit_code});
            return finalize_exit(exit_code);
        }

        if (cmd.background) {
            int async_result = execute_command_async(commands[0].args);
            if (async_result != 0) {
                set_last_pipeline_statuses({async_result});
            }
            return finalize_exit(async_result);
        }
        if (!cmd.args.empty() && g_shell && (g_shell->get_built_ins() != nullptr) &&
            (g_shell->get_built_ins()->is_builtin_command(cmd.args[0]) != 0)) {
            TemporaryEnvAssignmentScope temp_scope(g_shell.get(), env_assignments);
            int builtin_exit = execute_builtin_with_redirections(cmd);
            set_last_pipeline_statuses({builtin_exit});
            return finalize_exit(builtin_exit);
        }

        ProcessSubstitutionResources proc_resources;
        try {
            proc_resources = setup_process_substitutions(cmd);
        } catch (const std::exception& e) {
            set_error(ErrorType::RUNTIME_ERROR, cmd.args.empty() ? "command" : cmd.args[0],
                      std::string(e.what()));
            set_last_pipeline_statuses({EX_OSERR});
            return finalize_exit(EX_OSERR);
        }

        std::string cached_exec_path;
        if (!cmd.args.empty()) {
            cached_exec_path = cjsh_filesystem::resolve_executable_for_execution(cmd.args[0]);
        }

        std::optional<PtyPair> output_pty;
        std::shared_ptr<OutputRelayState> output_relay;
        const bool wants_output_relay = shell_is_interactive && cmd.auto_background_on_stop_silent;
        const bool can_capture_output = wants_output_relay &&
                                        !command_has_stdout_redirection(cmd) &&
                                        !command_has_stderr_redirection(cmd);
        if (can_capture_output) {
            output_pty = create_output_pty(shell_terminal);
        }

        pid_t pid = fork();

        if (pid == -1) {
            cleanup_process_substitutions(proc_resources, true);
            set_error(ErrorType::RUNTIME_ERROR, cmd.args.empty() ? "unknown" : cmd.args[0],
                      "failed to fork process: " + std::string(strerror(errno)));
            if (output_pty.has_value()) {
                cjsh_filesystem::safe_close(output_pty->master_fd);
                cjsh_filesystem::safe_close(output_pty->slave_fd);
            }
            set_last_pipeline_statuses({EX_OSERR});
            return finalize_exit(EX_OSERR);
        }

        if (pid == 0) {
            const std::string command_name = cmd.args.empty() ? "command" : cmd.args[0];

            if (has_temporary_env) {
                cjsh_env::apply_env_assignments(env_assignments);
            }
            pid_t child_pid = getpid();
            if (setpgid(child_pid, child_pid) < 0) {
                child_exit_with_error(
                    ErrorType::RUNTIME_ERROR, command_name,
                    std::string("setpgid: failed to set process group ID in child: ") +
                        strerror(errno));
            }

            maybe_set_foreground_terminal(
                shell_is_interactive, shell_terminal, child_pid, [&](int err) {
                    child_exit_with_error(ErrorType::RUNTIME_ERROR, command_name,
                                          std::string("tcsetpgrp: failed to set terminal "
                                                      "foreground process group in child: ") +
                                              strerror(err));
                });

            reset_child_signals();

            if (!cmd.here_doc.empty()) {
                auto here_doc_error = [&](HereDocErrorKind kind, const std::string& detail) {
                    std::string message;
                    switch (kind) {
                        case HereDocErrorKind::Pipe:
                            message = "pipe: failed to secure here document pipe: " + detail;
                            break;
                        case HereDocErrorKind::ContentWrite:
                            message = "write: failed to write here document content: " + detail;
                            break;
                        case HereDocErrorKind::NewlineWrite:
                            message = "write: failed to write here document newline: " + detail;
                            break;
                        case HereDocErrorKind::Duplication:
                            message =
                                "dup2: failed to duplicate here document descriptor: " + detail;
                            break;
                    }
                    child_exit_with_error(ErrorType::RUNTIME_ERROR, command_name, message);
                };

                if (!setup_here_document_stdin(cmd.here_doc, here_doc_error)) {
                    child_exit_with_error(ErrorType::RUNTIME_ERROR, command_name,
                                          "failed to configure here document for stdin");
                }
            } else if (!cmd.here_string.empty()) {
                auto here_error = cjsh_filesystem::setup_here_string_stdin(cmd.here_string);
                if (here_error.has_value()) {
                    std::string message;
                    switch (here_error->type) {
                        case cjsh_filesystem::HereStringErrorType::Pipe:
                            message = "pipe: failed to create pipe for here string: " +
                                      here_error->detail;
                            break;
                        case cjsh_filesystem::HereStringErrorType::Write:
                            message =
                                "write: failed to write here string content: " + here_error->detail;
                            break;
                        case cjsh_filesystem::HereStringErrorType::Dup:
                            message = "dup2: failed to duplicate here string descriptor: " +
                                      here_error->detail;
                            break;
                    }
                    child_exit_with_error(ErrorType::RUNTIME_ERROR, command_name, message);
                }
            } else if (!cmd.input_file.empty()) {
                auto redirect_result =
                    cjsh_filesystem::redirect_fd(cmd.input_file, STDIN_FILENO, O_RDONLY);
                if (redirect_result.is_error()) {
                    child_exit_with_error(ErrorType::FILE_NOT_FOUND, command_name,
                                          cmd.input_file + ": " + redirect_result.error());
                }
            }

            if (output_pty.has_value()) {
                if (dup2(output_pty->slave_fd, STDOUT_FILENO) == -1) {
                    child_exit_with_error(
                        ErrorType::RUNTIME_ERROR, command_name,
                        std::string("dup2: failed to attach output relay stdout: ") +
                            strerror(errno));
                }
                if (dup2(output_pty->slave_fd, STDERR_FILENO) == -1) {
                    child_exit_with_error(
                        ErrorType::RUNTIME_ERROR, command_name,
                        std::string("dup2: failed to attach output relay stderr: ") +
                            strerror(errno));
                }
                cjsh_filesystem::safe_close(output_pty->master_fd);
                cjsh_filesystem::safe_close(output_pty->slave_fd);
            }

            if (!cmd.output_file.empty()) {
                if (cjsh_filesystem::should_noclobber_prevent_overwrite(cmd.output_file,
                                                                        cmd.force_overwrite)) {
                    child_exit_with_error(
                        ErrorType::PERMISSION_DENIED, command_name,
                        cmd.output_file + ": cannot overwrite existing file (noclobber is set)");
                }

                auto redirect_result = cjsh_filesystem::redirect_fd(cmd.output_file, STDOUT_FILENO,
                                                                    O_WRONLY | O_CREAT | O_TRUNC);
                if (redirect_result.is_error()) {
                    child_exit_with_error(ErrorType::FILE_NOT_FOUND, command_name,
                                          cmd.output_file + ": " + redirect_result.error());
                }
            }

            if (cmd.both_output && !cmd.both_output_file.empty()) {
                if (cjsh_filesystem::should_noclobber_prevent_overwrite(cmd.both_output_file)) {
                    child_exit_with_error(
                        ErrorType::PERMISSION_DENIED, command_name,
                        cmd.both_output_file +
                            ": cannot overwrite existing file (noclobber is set)");
                }

                auto stdout_result = cjsh_filesystem::redirect_fd(
                    cmd.both_output_file, STDOUT_FILENO, O_WRONLY | O_CREAT | O_TRUNC);
                if (stdout_result.is_error()) {
                    child_exit_with_error(ErrorType::FILE_NOT_FOUND, command_name,
                                          cmd.both_output_file + ": " + stdout_result.error());
                }

                auto stderr_result = cjsh_filesystem::safe_dup2(STDOUT_FILENO, STDERR_FILENO);
                if (stderr_result.is_error()) {
                    child_exit_with_error(
                        ErrorType::RUNTIME_ERROR, command_name,
                        "dup2: failed for stderr in &> redirection: " + stderr_result.error());
                }
            }

            if (!cmd.append_file.empty()) {
                int fd = open(cmd.append_file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd == -1) {
                    child_exit_with_error(classify_filesystem_error(errno), command_name,
                                          cmd.append_file + ": " + std::string(strerror(errno)));
                }
                if (dup2(fd, STDOUT_FILENO) == -1) {
                    int saved_errno = errno;
                    close(fd);
                    child_exit_with_error(ErrorType::RUNTIME_ERROR, command_name,
                                          "dup2: failed for append redirection: " +
                                              std::string(strerror(saved_errno)));
                }
                close(fd);
            }

            if (!configure_stderr_redirects(cmd, handle_stream_redirect_error_and_exit)) {
                _exit(EXIT_FAILURE);
            }

            if (!apply_fd_operations(cmd, handle_fd_operation_error_and_exit)) {
                _exit(EXIT_FAILURE);
            }

            const char* exec_override =
                cached_exec_path.empty() ? nullptr : cached_exec_path.c_str();
            exec_external_child(cmd.args, exec_override);
        }

        if (!shell_is_interactive && !config::force_interactive) {
            int status = 0;
            pid_t wpid = waitpid(pid, &status, 0);
            while (wpid == -1 && errno == EINTR) {
                if (g_shell) {
                    g_shell->process_pending_signals();
                } else if (g_signal_handler) {
                    g_signal_handler->process_pending_signals(this);
                }
                wpid = waitpid(pid, &status, 0);
            }

            int exit_code = (wpid == -1) ? EX_OSERR : extract_exit_code(status);

            auto exit_result = job_utils::make_exit_error_result(cmd.args[0], exit_code,
                                                                 "command completed successfully",
                                                                 "command failed with exit code ");
            set_error(exit_result.type, cmd.args[0], exit_result.message, exit_result.suggestions);
            cleanup_process_substitutions(proc_resources, false);
            set_last_pipeline_statuses({exit_code});
            return finalize_exit(exit_code);
        }

        if (setpgid(pid, pid) < 0) {
            warn_parent_setpgid_failure();
        }
        Job job = make_single_process_job(pid, cmd.args[0], false, cmd.auto_background_on_stop,
                                          cmd.auto_background_on_stop_silent);
        attach_output_relay_to_job(job, output_pty, output_relay);

        int job_id = add_job(job);
        std::string full_command = join_arguments(cmd.args);
        bool reads_stdin = job_utils::command_consumes_terminal_stdin(cmd);
        int managed_job_id =
            JobManager::instance().add_job(pid, {pid}, full_command, job.background, reads_stdin);
        put_job_in_foreground(job_id, false);

        if (!cmd.output_file.empty() || !cmd.append_file.empty() || !cmd.stderr_file.empty()) {
            if (cjsh_env::shell_variable_is_set("CJSH_FORCE_SYNC")) {
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

        int raw_exit = last_exit_code;
        cleanup_process_substitutions(proc_resources, false);
        {
            std::lock_guard<std::mutex> lock(jobs_mutex);
            auto it = jobs.find(job_id);
            if (it != jobs.end()) {
                set_last_pipeline_statuses(it->second.pipeline_statuses);
            } else {
                set_last_pipeline_statuses({raw_exit});
            }
        }
        return finalize_exit(raw_exit);
    }
    std::vector<pid_t> pids;
    pid_t pgid = 0;

    std::vector<std::array<int, 2>> pipes(commands.size() - 1);

    std::optional<PtyPair> output_pty;
    std::shared_ptr<OutputRelayState> output_relay;
    auto close_output_pty = [&]() {
        if (output_pty.has_value()) {
            cjsh_filesystem::safe_close(output_pty->master_fd);
            cjsh_filesystem::safe_close(output_pty->slave_fd);
        }
    };
    const bool wants_output_relay =
        shell_is_interactive && commands.back().auto_background_on_stop_silent;
    if (wants_output_relay) {
        bool can_capture_output = !command_has_stdout_redirection(commands.back());
        if (!can_capture_output) {
            for (const auto& cmd : commands) {
                if (!command_has_stderr_redirection(cmd)) {
                    can_capture_output = true;
                    break;
                }
            }
        }
        if (can_capture_output) {
            output_pty = create_output_pty(shell_terminal);
        }
    }

    try {
        for (size_t i = 0; i < commands.size() - 1; i++) {
            if (pipe(pipes[i].data()) == -1) {
                set_error(ErrorType::RUNTIME_ERROR, "",
                          "failed to create pipe " + std::to_string(i + 1) +
                              " for pipeline: " + std::string(strerror(errno)));
                set_last_pipeline_statuses({EX_OSERR});
                close_output_pty();
                return finalize_exit(EX_OSERR);
            }
        }

        for (size_t i = 0; i < commands.size(); i++) {
            Command cmd = commands[i];
            std::vector<std::pair<std::string, std::string>> env_assignments;
            size_t cmd_start_idx = cjsh_env::collect_env_assignments(cmd.args, env_assignments);
            const size_t original_arg_count = cmd.args.size();
            const bool has_temporary_env = strip_temporary_env_assignments(
                cmd.args, cmd_start_idx, original_arg_count, env_assignments);

            std::string cached_exec_path;
            if (!cmd.args.empty()) {
                cached_exec_path = cjsh_filesystem::resolve_executable_for_execution(cmd.args[0]);
            }

            if (cmd.args.empty()) {
                set_error(ErrorType::INVALID_ARGUMENT, "",
                          "command " + std::to_string(i + 1) + " in pipeline is empty");
                print_last_error();

                for (size_t j = 0; j < commands.size() - 1; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }

                set_last_pipeline_statuses({1});
                close_output_pty();
                return finalize_exit(1);
            }

            pid_t pid = fork();

            if (pid == -1) {
                std::string cmd_name = cmd.args.empty() ? "unknown" : cmd.args[0];
                set_error(ErrorType::RUNTIME_ERROR, cmd_name,
                          "failed to create process (command " + std::to_string(i + 1) +
                              " in pipeline): " + std::string(strerror(errno)));
                set_last_pipeline_statuses({EX_OSERR});
                close_output_pty();
                return finalize_exit(EX_OSERR);
            }

            if (pid == 0) {
                const std::string command_name = cmd.args.empty() ? "exec" : cmd.args[0];
                const auto child_error = [&](ErrorType type, const std::string& message) {
                    child_exit_with_error(type, command_name, message);
                };

                if (i == 0) {
                    pgid = getpid();
                }

                if (setpgid(0, pgid) < 0) {
                    const int saved_errno = errno;
                    child_error(ErrorType::RUNTIME_ERROR, "failed to set process group in child: " +
                                                              std::string(strerror(saved_errno)));
                }

                maybe_set_foreground_terminal(
                    shell_is_interactive && i == 0, shell_terminal, pgid, [&](int err) {
                        ErrorInfo info{
                            ErrorType::RUNTIME_ERROR,
                            ErrorSeverity::WARNING,
                            command_name,
                            std::string("failed to set controlling terminal in child: ") +
                                strerror(err),
                            {}};
                        print_error(info);
                    });

                reset_child_signals();
                if (has_temporary_env) {
                    cjsh_env::apply_env_assignments(env_assignments);
                }
                if (i == 0) {
                    if (!cmd.here_doc.empty()) {
                        auto here_doc_error = [&](HereDocErrorKind kind,
                                                  const std::string& detail) {
                            std::string message;
                            switch (kind) {
                                case HereDocErrorKind::Pipe:
                                    message = "failed to create pipe for here document: " + detail;
                                    break;
                                case HereDocErrorKind::ContentWrite:
                                    message = "failed to write here document content: " + detail;
                                    break;
                                case HereDocErrorKind::NewlineWrite:
                                    message = "failed to write here document newline: " + detail;
                                    break;
                                case HereDocErrorKind::Duplication:
                                    message =
                                        "failed to duplicate here document descriptor: " + detail;
                                    break;
                            }
                            child_error(ErrorType::RUNTIME_ERROR, message);
                        };

                        if (!setup_here_document_stdin(cmd.here_doc, here_doc_error)) {
                            child_error(ErrorType::RUNTIME_ERROR,
                                        "failed to set up here document for pipeline input");
                        }
                    } else if (!cmd.input_file.empty()) {
                        int fd = open(cmd.input_file.c_str(), O_RDONLY);
                        if (fd == -1) {
                            const int saved_errno = errno;
                            child_error(classify_filesystem_error(saved_errno),
                                        cmd.input_file + ": " + std::string(strerror(saved_errno)));
                        }
                        if (dup2(fd, STDIN_FILENO) == -1) {
                            const int saved_errno = errno;
                            close(fd);
                            child_error(ErrorType::RUNTIME_ERROR,
                                        std::string("dup2 input failed: ") + strerror(saved_errno));
                        }
                        close(fd);
                    }
                } else {
                    if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
                        const int saved_errno = errno;
                        child_error(
                            ErrorType::RUNTIME_ERROR,
                            std::string("dup2 pipe input failed: ") + strerror(saved_errno));
                    }
                }

                if (output_pty.has_value() && i == commands.size() - 1 &&
                    !command_has_stdout_redirection(cmd)) {
                    if (dup2(output_pty->slave_fd, STDOUT_FILENO) == -1) {
                        const int saved_errno = errno;
                        child_error(ErrorType::RUNTIME_ERROR,
                                    std::string("dup2 output relay stdout failed: ") +
                                        strerror(saved_errno));
                    }
                }

                if (i == commands.size() - 1) {
                    if (!cmd.output_file.empty()) {
                        int fd = open(cmd.output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        if (fd == -1) {
                            const int saved_errno = errno;
                            child_error(
                                classify_filesystem_error(saved_errno),
                                cmd.output_file + ": " + std::string(strerror(saved_errno)));
                        }
                        if (dup2(fd, STDOUT_FILENO) == -1) {
                            const int saved_errno = errno;
                            close(fd);
                            child_error(
                                ErrorType::RUNTIME_ERROR,
                                std::string("dup2 output failed: ") + strerror(saved_errno));
                        }
                        close(fd);
                    } else if (!cmd.append_file.empty()) {
                        int fd = open(cmd.append_file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                        if (fd == -1) {
                            const int saved_errno = errno;
                            child_error(
                                classify_filesystem_error(saved_errno),
                                cmd.append_file + ": " + std::string(strerror(saved_errno)));
                        }
                        if (dup2(fd, STDOUT_FILENO) == -1) {
                            const int saved_errno = errno;
                            close(fd);
                            child_error(
                                ErrorType::RUNTIME_ERROR,
                                std::string("dup2 append failed: ") + strerror(saved_errno));
                        }
                        close(fd);
                    }
                } else {
                    if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                        const int saved_errno = errno;
                        child_error(
                            ErrorType::RUNTIME_ERROR,
                            std::string("dup2 pipe output failed: ") + strerror(saved_errno));
                    }
                }

                if (output_pty.has_value() && !command_has_stderr_redirection(cmd)) {
                    if (dup2(output_pty->slave_fd, STDERR_FILENO) == -1) {
                        const int saved_errno = errno;
                        child_error(ErrorType::RUNTIME_ERROR,
                                    std::string("dup2 output relay stderr failed: ") +
                                        strerror(saved_errno));
                    }
                }

                if (!configure_stderr_redirects(cmd, handle_stream_redirect_error_and_exit)) {
                    child_error(ErrorType::RUNTIME_ERROR,
                                "failed to configure stderr redirections for pipeline child");
                }

                if (!apply_fd_operations(cmd, handle_fd_operation_error_and_exit)) {
                    child_error(ErrorType::RUNTIME_ERROR,
                                "failed to apply file descriptor operations for pipeline child");
                }

                if (output_pty.has_value()) {
                    cjsh_filesystem::safe_close(output_pty->master_fd);
                    cjsh_filesystem::safe_close(output_pty->slave_fd);
                }

                for (size_t j = 0; j < commands.size() - 1; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }

                ShellScriptInterpreter* interpreter =
                    g_shell ? g_shell->get_shell_script_interpreter() : nullptr;

                if (is_shell_control_structure(cmd)) {
                    int exit_code = 1;
                    if (g_shell) {
                        exit_code = g_shell->execute(command_text_for_interpretation(cmd));
                    }
                    (void)fflush(stdout);
                    (void)fflush(stderr);
                    _exit(exit_code);
                } else if (interpreter && interpreter->has_function(cmd.args[0])) {
                    int exit_code = interpreter->invoke_function(cmd.args);

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
                    const char* exec_override =
                        cached_exec_path.empty() ? nullptr : cached_exec_path.c_str();
                    exec_external_child(cmd.args, exec_override);
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

        if (output_pty.has_value()) {
            cjsh_filesystem::safe_close(output_pty->slave_fd);
            output_relay = start_output_relay(output_pty->master_fd, !commands.back().background);
        }

    } catch (const std::exception& e) {
        set_error(ErrorType::RUNTIME_ERROR, "pipeline",
                  "Error executing pipeline: " + std::string(e.what()));
        print_last_error();
        for (pid_t pid : pids) {
            kill(pid, SIGTERM);
        }
        set_last_pipeline_statuses({1});
        close_output_pty();
        return finalize_exit(1);
    }

    Job job;
    job.pgid = pgid;
    job.command = commands[0].args[0] + " | ...";
    job.background = commands.back().background;
    job.auto_background_on_stop = commands.back().auto_background_on_stop;
    job.auto_background_on_stop_silent = commands.back().auto_background_on_stop_silent;
    job.completed = false;
    job.stopped = false;
    job.pids = pids;
    job.last_pid = pids.empty() ? -1 : pids.back();
    job.pid_order = pids;
    job.pipeline_statuses.assign(pids.size(), -1);
    job.output_relay = output_relay;

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

    int raw_exit = last_exit_code;

    if (job.background) {
        put_job_in_background(job_id, false);
        std::cerr << "[" << job_id << "] " << pgid << " " << job.command << '\n';
        raw_exit = 0;
    } else {
        put_job_in_foreground(job_id, false);

        std::lock_guard<std::mutex> lock(jobs_mutex);
        auto it = jobs.find(job_id);
        if (it != jobs.end()) {
            if (it->second.completed) {
                JobManager::instance().remove_job(new_job_id);
                raw_exit = extract_exit_code(it->second.last_status);
                set_last_pipeline_statuses(it->second.pipeline_statuses);
                raw_exit = apply_pipefail(raw_exit, it->second.pipeline_statuses);
            } else {
                raw_exit = last_exit_code;
            }
        } else {
            set_last_pipeline_statuses({raw_exit});
        }
    }

    if (job.background) {
        // Leave PIPESTATUS untouched for background pipelines to mirror bash behaviour.
    }

    return finalize_exit(raw_exit);
}

int Exec::run_with_command_redirections(Command cmd, const std::function<int()>& action,
                                        const std::string& command_name, bool persist_fd_changes,
                                        bool* action_invoked) {
    auto duplicate_fd = [](int fd) {
        int min_fd = std::max(fd + 1, 10);
        int dup_fd = -1;
#ifdef F_DUPFD_CLOEXEC
        dup_fd = fcntl(fd, F_DUPFD_CLOEXEC, min_fd);
#endif
        if (dup_fd == -1) {
            dup_fd = fcntl(fd, F_DUPFD, min_fd);
        }
        if (dup_fd == -1) {
            dup_fd = dup(fd);
        }
        if (dup_fd != -1) {
            int flags = fcntl(dup_fd, F_GETFD);
            if (flags != -1) {
                fcntl(dup_fd, F_SETFD, flags | FD_CLOEXEC);
            }
        }
        return dup_fd;
    };

    int orig_stdin = duplicate_fd(STDIN_FILENO);
    int orig_stdout = duplicate_fd(STDOUT_FILENO);
    int orig_stderr = duplicate_fd(STDERR_FILENO);

    if (orig_stdin == -1 || orig_stdout == -1 || orig_stderr == -1) {
        set_error(ErrorType::RUNTIME_ERROR, command_name,
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
        return EX_OSERR;
    }

    ProcessSubstitutionResources proc_resources;

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

    if (action_invoked) {
        *action_invoked = false;
    }

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
                if (write_result.error().find("Broken pipe") == std::string::npos &&
                    write_result.error().find("EPIPE") == std::string::npos) {
                    error = write_result.error();
                }

            } else {
                auto newline_result =
                    cjsh_filesystem::write_all(here_pipe[1], std::string_view("\n", 1));
                if (newline_result.is_error()) {
                    if (newline_result.error().find("Broken pipe") == std::string::npos &&
                        newline_result.error().find("EPIPE") == std::string::npos) {
                        error = newline_result.error();
                    }
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

        if (!cmd.fd_redirections.empty() || !cmd.fd_duplications.empty()) {
            auto fd_error_handler = [&](const FdOperationError& error) -> void {
                switch (error.type) {
                    case FdOperationErrorType::Redirect:
                        throw std::runtime_error(command_name + ": " + error.spec + ": " +
                                                 error.error);
                    case FdOperationErrorType::Duplication:
                        throw std::runtime_error(command_name + ": dup2 failed for " +
                                                 std::to_string(error.fd_num) + ">&" +
                                                 std::to_string(error.src_fd) + ": " + error.error);
                }
            };
            (void)apply_fd_operations(cmd, fd_error_handler);
        }

        std::cout.flush();
        std::cerr.flush();
        std::clog.flush();

        int exit_code = action();
        if (action_invoked) {
            *action_invoked = true;
        }

        std::cout.flush();
        std::cerr.flush();
        std::clog.flush();

        restore_descriptors(false);
        return exit_code;
    } catch (const std::exception& e) {
        restore_descriptors(true);
        set_error(ErrorType::RUNTIME_ERROR, command_name, std::string(e.what()));
        return EX_OSERR;
    }
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

void Exec::put_job_in_foreground(int job_id, bool cont) {
    std::lock_guard<std::mutex> lock(jobs_mutex);

    Job* job = find_job_and_set_output_forwarding_locked(job_id, true);
    if (job == nullptr) {
        return;
    }

    bool terminal_control_acquired = false;
    if (shell_is_interactive && (isatty(shell_terminal) != 0)) {
        if (tcsetpgrp(shell_terminal, job->pgid) == 0) {
            terminal_control_acquired = true;
        } else {
            if (errno != ENOTTY && errno != EINVAL && errno != EPERM) {
                set_error(ErrorType::RUNTIME_ERROR, "tcsetpgrp",
                          "warning: failed to set terminal control to job: " +
                              std::string(strerror(errno)));
            }
        }
    }

    resume_job(*job, cont, "job");

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

    Job* job = find_job_and_set_output_forwarding_locked(job_id, false);
    if (job == nullptr) {
        return;
    }

    resume_job(*job, cont, "background job");
}

void Exec::set_job_output_forwarding(pid_t pgid, bool forward) {
    std::lock_guard<std::mutex> lock(jobs_mutex);
    for (auto& pair : jobs) {
        Job& job = pair.second;
        if (job.pgid == pgid) {
            if (job.output_relay) {
                job.output_relay->forward.store(forward);
            }
            break;
        }
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
    std::vector<pid_t> pid_order = it->second.pid_order;
    std::vector<int> pipeline_statuses = it->second.pipeline_statuses;

    lock.unlock();

    int status = 0;
    pid_t pid = 0;

    bool job_stopped = false;
    bool saw_last = false;
    int last_status = 0;
    int stop_signal = 0;

    while (!remaining_pids.empty()) {
        pid = waitpid(-job_pgid, &status, WUNTRACED);

        if (pid == -1) {
            if (errno == EINTR) {
                if (g_shell) {
                    g_shell->process_pending_signals();
                } else if (g_signal_handler) {
                    g_signal_handler->process_pending_signals(this);
                }
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

        auto order_it = std::find(pid_order.begin(), pid_order.end(), pid);
        if (order_it != pid_order.end()) {
            size_t index = static_cast<size_t>(std::distance(pid_order.begin(), order_it));
            if (index < pipeline_statuses.size() && (WIFEXITED(status) || WIFSIGNALED(status))) {
                pipeline_statuses[index] = extract_exit_code(status);
            }
        }

        if (pid == last_pid) {
        }

        if (pid == last_pid) {
            saw_last = true;
            last_status = status;
        }

        if (WIFSTOPPED(status)) {
            job_stopped = true;
            stop_signal = WSTOPSIG(status);
            break;
        }
    }

    lock.lock();

    it = jobs.find(job_id);
    if (it != jobs.end()) {
        Job& job = it->second;
        if (!pipeline_statuses.empty()) {
            job.pipeline_statuses = pipeline_statuses;
        }

        if (job_stopped) {
            job.stopped = true;
            job.status = status;

            auto job_control = JobManager::instance().get_job_by_pgid(job_pgid);
            if (!job_control) {
                auto jobs_snapshot = JobManager::instance().get_all_jobs();
                for (const auto& candidate : jobs_snapshot) {
                    if (!candidate) {
                        continue;
                    }
                    if (candidate->pgid == job_pgid ||
                        std::find(candidate->pids.begin(), candidate->pids.end(), job_pgid) !=
                            candidate->pids.end()) {
                        job_control = candidate;
                        break;
                    }
                }
            }

            const bool should_auto_background =
                job.auto_background_on_stop && stop_signal == SIGTSTP && job.pgid > 0;

            if (should_auto_background) {
                if (job.auto_background_on_stop_silent && job.output_relay) {
                    job.output_relay->forward.store(false);
                }
                resume_job(job, true, "background job");
                job.background = true;
                job.completed = false;
                last_exit_code = 0;

                if (job_control) {
                    job_control->state.store(JobState::RUNNING, std::memory_order_relaxed);
                    job_control->background.store(true, std::memory_order_relaxed);
                    job_control->stop_notified.store(false, std::memory_order_relaxed);
                }

                JobManager::instance().set_last_background_pid(job.last_pid);

                const std::string& display_command =
                    job_control ? job_control->display_command() : job.command;
                std::cerr << "\n[" << job_id << "]+ " << display_command << " &" << '\n';
            } else {
                last_exit_code = 128 + SIGTSTP;

                if (job_control) {
                    JobManager::instance().notify_job_stopped(job_control);
                }
            }
        } else {
            job.completed = true;
            job.stopped = false;
            job.status = status;

            int final_status = saw_last ? last_status : status;
            const bool exited_or_signaled = WIFEXITED(final_status) || WIFSIGNALED(final_status);
            if (!exited_or_signaled) {
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
            auto order_it = std::find(job.pid_order.begin(), job.pid_order.end(), pid);
            if (order_it != job.pid_order.end()) {
                size_t index = static_cast<size_t>(std::distance(job.pid_order.begin(), order_it));
                int recorded = -1;
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    recorded = extract_exit_code(status);
                }
                if (recorded != -1 && index < job.pipeline_statuses.size()) {
                    job.pipeline_statuses[index] = recorded;
                }
            }
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
                        if (WIFSIGNALED(status)) {
                            std::cerr << "\n[" << job_id << "] Terminated\t" << job.command << '\n';
                        } else {
                            const int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 0;
                            if (exit_code == 0) {
                                std::cerr << "\n[" << job_id << "] Done\t" << job.command << '\n';
                            } else {
                                std::cerr << "\n[" << job_id << "] Exit " << exit_code << '\t'
                                          << job.command << '\n';
                            }
                        }
                    }
                }
            }
            break;
        }
    }
}

std::map<int, Job> Exec::get_jobs() {
    std::lock_guard<std::mutex> lock(jobs_mutex);
    return jobs;
}

void Exec::terminate_all_child_process() {
    struct JobRecord {
        int id;
        Job job;
    };

    std::vector<JobRecord> job_snapshot;
    {
        std::lock_guard<std::mutex> lock(jobs_mutex);
        job_snapshot.reserve(jobs.size());
        for (const auto& pair : jobs) {
            job_snapshot.push_back({pair.first, pair.second});
        }
    }

    const auto send_signal_to_job = [](const Job& job, int signal) -> bool {
        bool pid_signaled = false;

        for (pid_t pid : job.pids) {
            if (pid <= 0) {
                continue;
            }
            if (kill(pid, signal) == 0) {
                pid_signaled = true;
            }
        }

        if (job.pgid <= 0) {
            return pid_signaled;
        }

        if (killpg(job.pgid, signal) == 0) {
            return true;
        }

        const int group_errno = errno;
        if (group_errno == ESRCH) {
            return pid_signaled;
        }

        if ((group_errno == EPERM || group_errno == EACCES) && pid_signaled) {
            return true;
        }

        print_error({ErrorType::RUNTIME_ERROR,
                     ErrorSeverity::WARNING,
                     "killpg",
                     "failed to send signal " + std::to_string(signal) + " to pgid " +
                         std::to_string(job.pgid) + ": " + std::string(strerror(group_errno)),
                     {}});
        return pid_signaled;
    };

    bool signaled_any = false;
    for (const auto& entry : job_snapshot) {
        const Job& job = entry.job;
        if (job.completed) {
            continue;
        }

#ifdef SIGCONT
        if (job.stopped && job.pgid > 0) {
            (void)send_signal_to_job(job, SIGCONT);
        }
#endif

        if (send_signal_to_job(job, SIGTERM)) {
            signaled_any = true;
        }

        if (job.pgid > 0 || !job.pids.empty()) {
            std::cerr << "[" << entry.id << "] Terminated\t" << job.command << '\n';
        }
    }

    if (signaled_any) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    for (const auto& entry : job_snapshot) {
        const Job& job = entry.job;
        if (job.completed) {
            continue;
        }

        (void)send_signal_to_job(job, SIGKILL);
    }

    int status = 0;
    int zombie_count = 0;
    const int max_terminate_iterations = 100;
    while (zombie_count < max_terminate_iterations) {
        pid_t reap_pid = waitpid(-1, &status, WNOHANG);
        if (reap_pid <= 0) {
            break;
        }
        ++zombie_count;
    }

    if (zombie_count >= max_terminate_iterations) {
        print_error({ErrorType::RUNTIME_ERROR,
                     ErrorSeverity::WARNING,
                     "terminate_all_child_process",
                     "hit maximum cleanup iterations",
                     {}});
    }

    {
        std::lock_guard<std::mutex> lock(jobs_mutex);
        for (auto& pair : jobs) {
            pair.second.completed = true;
            pair.second.stopped = false;
            pair.second.pids.clear();
            pair.second.status = 0;
        }
    }

    if (!signaled_any) {
        set_error(ErrorType::RUNTIME_ERROR, "", "No child processes to terminate");
    } else {
        set_error(ErrorType::RUNTIME_ERROR, "", "All child processes terminated");
    }
}

void Exec::abandon_all_child_processes() {
    if (!jobs_mutex.try_lock()) {
        return;
    }
    jobs.clear();
    jobs_mutex.unlock();
}

void Exec::set_error(const ErrorInfo& error) {
    std::lock_guard<std::mutex> lock(error_mutex);
    last_error = error;
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

void Exec::print_last_error() {
    std::lock_guard<std::mutex> lock(error_mutex);
    print_error(last_error);
}

void Exec::print_error_if_needed(int exit_code) {
    if (exit_code == 0) {
        return;
    }

    ErrorInfo error = get_error();
    bool already_reported =
        (exit_code == 127 && error.type == ErrorType::COMMAND_NOT_FOUND && error.message.empty());
    if (!already_reported &&
        (error.type != ErrorType::RUNTIME_ERROR ||
         error.message.find("command failed with exit code") == std::string::npos)) {
        print_last_error();
    }
}

int Exec::get_exit_code() const {
    return last_exit_code;
}

const std::vector<int>& Exec::get_last_pipeline_statuses() const {
    return last_pipeline_statuses;
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

    std::string cached_exec_path;
    if (!args.empty()) {
        cached_exec_path = cjsh_filesystem::resolve_executable_for_execution(args[0]);
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

        const char* exec_override = cached_exec_path.empty() ? nullptr : cached_exec_path.c_str();
        exec_external_child(args, exec_override);
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

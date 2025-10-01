#include "exec.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <csignal>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "job_control.h"
#include "signal_handler.h"
#include "suggestion_utils.h"

namespace {

bool is_valid_env_name(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    unsigned char first = static_cast<unsigned char>(name[0]);
    if (!std::isalpha(first) && first != '_') {
        return false;
    }
    for (char c : name) {
        unsigned char ch = static_cast<unsigned char>(c);
        if (!std::isalnum(ch) && ch != '_') {
            return false;
        }
    }
    return true;
}

size_t collect_env_assignments(
    const std::vector<std::string>& args,
    std::vector<std::pair<std::string, std::string>>& env_assignments) {
    size_t cmd_start_idx = 0;
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& token = args[i];
        size_t pos = token.find('=');
        if (pos != std::string::npos && pos > 0) {
            std::string name = token.substr(0, pos);
            if (is_valid_env_name(name)) {
                env_assignments.push_back({name, token.substr(pos + 1)});
                cmd_start_idx = i + 1;
                continue;
            }
        }
        break;
    }
    return cmd_start_idx;
}

void apply_env_assignments(
    const std::vector<std::pair<std::string, std::string>>& env_assignments) {
    for (const auto& env : env_assignments) {
        setenv(env.first.c_str(), env.second.c_str(), 1);
    }
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

bool command_exists(const std::string& command_path) {
    if (command_path.empty()) {
        return false;
    }
    if (command_path.find('/') != std::string::npos) {
        return access(command_path.c_str(), F_OK) == 0;
    }
    const char* path_env = getenv("PATH");
    if (!path_env) {
        return false;
    }
    std::string path_str(path_env);
    std::istringstream path_stream(path_str);
    std::string path_dir;
    while (std::getline(path_stream, path_dir, ':')) {
        if (path_dir.empty()) {
            continue;
        }
        std::string full_path = path_dir + "/" + command_path;
        if (access(full_path.c_str(), F_OK) == 0) {
            return true;
        }
    }
    return false;
}

struct ExitErrorResult {
    ErrorType type;
    std::string message;
    std::vector<std::string> suggestions;
};

ExitErrorResult make_exit_error_result(const std::string& command,
                                       int exit_code,
                                       const std::string& success_message,
                                       const std::string& failure_prefix) {
    ExitErrorResult result{ErrorType::RUNTIME_ERROR, success_message, {}};
    if (exit_code == 0) {
        return result;
    }
    result.message = failure_prefix + std::to_string(exit_code);
    if (exit_code == 127) {
        if (!command_exists(command)) {
            result.type = ErrorType::COMMAND_NOT_FOUND;
            result.message.clear();
            result.suggestions =
                suggestion_utils::generate_command_suggestions(command);
            return result;
        }
    } else if (exit_code == 126) {
        result.type = ErrorType::PERMISSION_DENIED;
    }
    return result;
}

enum class HereStringErrorType {
    Pipe,
    Write,
    Dup
};

struct HereStringError {
    HereStringErrorType type;
    std::string detail;
};

std::optional<HereStringError> setup_here_string_stdin(
    const std::string& here_string) {
    int here_pipe[2];
    if (pipe(here_pipe) == -1) {
        return HereStringError{HereStringErrorType::Pipe,
                               std::string(strerror(errno))};
    }

    std::string content = here_string;
    if (g_shell && g_shell->get_parser()) {
        g_shell->get_parser()->expand_env_vars(content);
    }
    content += "\n";

    ssize_t bytes_written =
        write(here_pipe[1], content.c_str(), content.length());
    if (bytes_written == -1) {
        cjsh_filesystem::FileOperations::safe_close(here_pipe[1]);
        cjsh_filesystem::FileOperations::safe_close(here_pipe[0]);
        return HereStringError{HereStringErrorType::Write,
                               std::string(strerror(errno))};
    }

    cjsh_filesystem::FileOperations::safe_close(here_pipe[1]);
    auto dup_result =
        cjsh_filesystem::FileOperations::safe_dup2(here_pipe[0], STDIN_FILENO);
    cjsh_filesystem::FileOperations::safe_close(here_pipe[0]);
    if (dup_result.is_error()) {
        return HereStringError{HereStringErrorType::Dup, dup_result.error()};
    }

    return std::nullopt;
}

std::vector<char*> build_exec_argv(const std::vector<std::string>& args) {
    std::vector<char*> c_args;
    c_args.reserve(args.size() + 1);
    for (const auto& arg : args) {
        c_args.push_back(const_cast<char*>(arg.c_str()));
    }
    c_args.push_back(nullptr);
    return c_args;
}

bool command_consumes_terminal_stdin(const Command& cmd) {
    if (!cmd.input_file.empty() || !cmd.here_doc.empty() ||
        !cmd.here_string.empty()) {
        return false;
    }

    if (cmd.fd_redirections.count(0) > 0 || cmd.fd_duplications.count(0) > 0) {
        return false;
    }

    return true;
}

bool pipeline_consumes_terminal_stdin(const std::vector<Command>& commands) {
    if (commands.empty()) {
        return false;
    }

    if (commands.back().background) {
        return false;
    }

    return command_consumes_terminal_stdin(commands.front());
}

}  // namespace

static bool should_noclobber_prevent_overwrite(const std::string& filename,
                                               bool force_overwrite = false) {
    if (force_overwrite) {
        return false;
    }

    if (!g_shell || !g_shell->get_shell_option("noclobber")) {
        return false;
    }

    struct stat file_stat;
    if (stat(filename.c_str(), &file_stat) == 0) {
        return true;
    }

    return false;
}

Exec::Exec() {
    last_terminal_output_error = "";
    shell_terminal = STDIN_FILENO;
    shell_is_interactive = isatty(shell_terminal);

    shell_pgid = getpid();

    if (shell_is_interactive) {
        if (tcgetattr(shell_terminal, &shell_tmodes) < 0) {
            set_error(ErrorType::RUNTIME_ERROR, "tcgetattr",
                      "failed to get terminal attributes in constructor: " +
                          std::string(strerror(errno)));
        }
    }
}

Exec::~Exec() {
    if (g_debug_mode) {
        std::cerr << "DEBUG: Exec destructor called" << std::endl;
    }

    int status;
    pid_t pid;
    int zombie_count = 0;
    const int max_cleanup_iterations =
        50;  // Prevent infinite loops in destructor
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0 &&
           zombie_count < max_cleanup_iterations) {
        zombie_count++;
        if (g_debug_mode && zombie_count <= 3) {
            std::cerr << "DEBUG: Exec destructor reaped zombie " << pid
                      << std::endl;
        }
    }

    if (zombie_count >= max_cleanup_iterations) {
        std::cerr << "WARNING: Exec destructor hit maximum cleanup iterations, "
                     "some zombies may remain"
                  << std::endl;
    }

    if (g_debug_mode && zombie_count > 0) {
        std::cerr << "DEBUG: Exec destructor reaped " << zombie_count
                  << " zombies" << std::endl;
    }
}

void Exec::init_shell() {
    if (shell_is_interactive) {
        shell_pgid = getpid();
        if (setpgid(shell_pgid, shell_pgid) < 0) {
            if (errno != EPERM) {
                set_error(ErrorType::RUNTIME_ERROR, "setpgid",
                          "failed to set process group ID: " +
                              std::string(strerror(errno)));
                return;
            }
        }

        if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
            set_error(ErrorType::RUNTIME_ERROR, "tcsetpgrp",
                      "failed to set terminal foreground process group: " +
                          std::string(strerror(errno)));
        }

        if (tcgetattr(shell_terminal, &shell_tmodes) < 0) {
            set_error(ErrorType::RUNTIME_ERROR, "tcgetattr",
                      "failed to get terminal attributes: " +
                          std::string(strerror(errno)));
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
                        std::cerr << "\n[" << job_id << "] Done\t"
                                  << job.command << std::endl;
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

void Exec::set_error(ErrorType type, const std::string& command,
                     const std::string& message,
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
    return !cmd.input_file.empty() || !cmd.output_file.empty() ||
           !cmd.append_file.empty() || cmd.background ||
           !cmd.stderr_file.empty() || cmd.stderr_to_stdout ||
           cmd.stdout_to_stderr || !cmd.here_doc.empty() ||
           !cmd.here_string.empty() || cmd.both_output ||
           !cmd.process_substitutions.empty() || !cmd.fd_redirections.empty() ||
           !cmd.fd_duplications.empty();
}

bool Exec::can_execute_in_process(const Command& cmd) const {
    if (cmd.args.empty())
        return false;

    if (g_shell && g_shell->get_built_ins() &&
        g_shell->get_built_ins()->is_builtin_command(cmd.args[0])) {
        return !requires_fork(cmd);
    }

    return false;
}

int Exec::execute_command_sync(const std::vector<std::string>& args) {
    if (args.empty()) {
        set_error(ErrorType::INVALID_ARGUMENT, "",
                  "cannot execute empty command - no arguments provided");
        last_exit_code = EX_DATAERR;
        return EX_DATAERR;
    }
    std::vector<std::pair<std::string, std::string>> env_assignments;
    size_t cmd_start_idx = collect_env_assignments(args, env_assignments);

    if (cmd_start_idx >= args.size()) {
        // When there's no command after the assignments, set them as shell
        // variables (not exported to environment) - this matches bash behavior
        if (g_shell) {
            auto& env_vars = g_shell->get_env_vars();
            for (const auto& env : env_assignments) {
                env_vars[env.first] = env.second;

                // Only export special variables like PATH
                if (env.first == "PATH" || env.first == "PWD" ||
                    env.first == "HOME" || env.first == "USER" ||
                    env.first == "SHELL") {
                    setenv(env.first.c_str(), env.second.c_str(), 1);
                }
            }

            // Update parser cache
            if (g_shell->get_parser()) {
                g_shell->get_parser()->set_env_vars(env_vars);
            }
        }

        last_exit_code = 0;
        return 0;
    }

    std::vector<std::string> cmd_args(args.begin() + cmd_start_idx, args.end());

    pid_t pid = fork();

    if (pid == -1) {
        set_error(ErrorType::RUNTIME_ERROR,
                  cmd_args.empty() ? "unknown" : cmd_args[0],
                  "failed to fork process: " + std::string(strerror(errno)),
                  {});
        last_exit_code = EX_OSERR;
        return EX_OSERR;
    }

    if (pid == 0) {
        apply_env_assignments(env_assignments);

        pid_t child_pid = getpid();
        if (setpgid(child_pid, child_pid) < 0) {
            std::cerr
                << "cjsh: runtime error: setpgid: failed to set process group "
                   "ID in child: "
                << strerror(errno) << std::endl;
            _exit(EXIT_FAILURE);
        }

        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
        signal(SIGTERM, SIG_DFL);

        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGINT);
        sigaddset(&set, SIGQUIT);
        sigaddset(&set, SIGTSTP);
        sigaddset(&set, SIGCHLD);
        sigaddset(&set, SIGTERM);
        sigprocmask(SIG_UNBLOCK, &set, nullptr);

        auto c_args = build_exec_argv(cmd_args);
        execvp(cmd_args[0].c_str(), c_args.data());

        // execvp failed - save errno immediately and determine exit code inline
        int saved_errno = errno;
        int exit_code = (saved_errno == EACCES || saved_errno == EISDIR ||
                         saved_errno == ENOEXEC)
                            ? 126
                            : 127;

        if (saved_errno == ENOENT) {
            auto suggestions =
                suggestion_utils::generate_command_suggestions(cmd_args[0]);
            set_error(ErrorType::COMMAND_NOT_FOUND, cmd_args[0], "",
                      suggestions);
        } else if (saved_errno == EACCES || saved_errno == EISDIR) {
            set_error(ErrorType::PERMISSION_DENIED, cmd_args[0], "", {});
        } else if (saved_errno == ENOEXEC) {
            set_error(ErrorType::INVALID_ARGUMENT, cmd_args[0],
                      "invalid executable format", {});
        } else {
            set_error(ErrorType::RUNTIME_ERROR, cmd_args[0],
                      "execution failed: " + std::string(strerror(saved_errno)),
                      {});
        }
        _exit(exit_code);
    }

    if (setpgid(pid, pid) < 0) {
        if (errno != EACCES && errno != ESRCH) {
            set_error(ErrorType::RUNTIME_ERROR, "setpgid",
                      "failed to set process group ID in parent: " +
                          std::string(strerror(errno)));
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

    if (g_shell && g_shell->get_parser()) {
        try {
            auto command_pipeline =
                g_shell->get_parser()->parse_pipeline(full_command);
            if (!command_pipeline.empty()) {
                reads_stdin =
                    pipeline_consumes_terminal_stdin(command_pipeline);
            }
        } catch (const std::exception& e) {
            if (g_debug_mode) {
                std::cerr
                    << "DEBUG: Failed to parse command for stdin detection: "
                    << e.what() << std::endl;
            }
        }
    }

    int new_job_id = JobManager::instance().add_job(
        pid, {pid}, full_command, job.background, reads_stdin);

    put_job_in_foreground(job_id, false);

    std::lock_guard<std::mutex> lock(jobs_mutex);
    auto it = jobs.find(job_id);
    int exit_code = 0;

    if (it != jobs.end() && it->second.completed) {
        int status = it->second.status;
        if (WIFEXITED(status)) {
            exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            exit_code = 128 + WTERMSIG(status);
        }

        JobManager::instance().remove_job(new_job_id);
    }

    auto exit_result = make_exit_error_result(args[0], exit_code,
                                              "command completed successfully",
                                              "command failed with exit code ");
    set_error(exit_result.type, args[0], exit_result.message,
              exit_result.suggestions);
    last_exit_code = exit_code;

    // Auto-update executable cache for successful external commands
    if (exit_code == 0 && !cmd_args.empty()) {
        const std::string& command_name = cmd_args[0];

        // Extract basename if command_name is a full path
        std::string basename_command;
        size_t last_slash = command_name.find_last_of('/');
        if (last_slash != std::string::npos) {
            basename_command = command_name.substr(last_slash + 1);
        } else {
            basename_command = command_name;
        }

        if (g_debug_mode) {
            std::cerr << "DEBUG: Checking cache update for external command: "
                      << command_name;
            if (basename_command != command_name) {
                std::cerr << " (basename: " << basename_command << ")";
            }
            std::cerr << " with exit code: " << exit_code << std::endl;
        }

        // Only add to cache if it's not already there
        bool already_in_cache =
            cjsh_filesystem::is_executable_in_cache(basename_command);

        if (g_debug_mode) {
            std::cerr << "DEBUG: External command '" << basename_command
                      << "' already_in_cache: "
                      << (already_in_cache ? "true" : "false") << std::endl;
        }

        if (!already_in_cache) {
            // Find the full path of the command using the basename
            std::string full_path =
                cjsh_filesystem::find_executable_in_path(basename_command);

            if (g_debug_mode) {
                std::cerr << "DEBUG: Found full path for '" << basename_command
                          << "': " << (full_path.empty() ? "EMPTY" : full_path)
                          << std::endl;
            }

            if (!full_path.empty()) {
                cjsh_filesystem::add_executable_to_cache(basename_command,
                                                         full_path);
                if (g_debug_mode) {
                    std::cerr
                        << "DEBUG: Added new executable '" << basename_command
                        << "' to cache after successful execution" << std::endl;
                }
            }
        } else if (g_debug_mode) {
            std::cerr << "DEBUG: Skipping cache update - '" << basename_command
                      << "' already in cache" << std::endl;
        }
    } else if (exit_code == 127 && !cmd_args.empty()) {
        // Command not found - might be a stale cache entry
        const std::string& command_name = cmd_args[0];

        if (g_debug_mode) {
            std::cerr << "DEBUG: Command not found: " << command_name
                      << " - checking if it's a stale cache entry" << std::endl;
        }

        if (cjsh_filesystem::is_executable_in_cache(command_name)) {
            if (g_debug_mode) {
                std::cerr << "DEBUG: Removing stale cache entry for: "
                          << command_name << std::endl;
            }
            cjsh_filesystem::remove_executable_from_cache(command_name);
        }
    } else if (g_debug_mode) {
        std::cerr << "DEBUG: Skipping cache update - exit_code=" << exit_code
                  << ", cmd_args.empty()="
                  << (cmd_args.empty() ? "true" : "false") << std::endl;
    }

    return exit_code;
}

int Exec::execute_command_async(const std::vector<std::string>& args) {
    if (args.empty()) {
        set_error(ErrorType::INVALID_ARGUMENT, "",
                  "cannot execute empty command - no arguments provided", {});
        last_exit_code = EX_DATAERR;
        return EX_DATAERR;
    }

    std::vector<std::pair<std::string, std::string>> env_assignments;
    size_t cmd_start_idx = collect_env_assignments(args, env_assignments);

    if (cmd_start_idx >= args.size()) {
        apply_env_assignments(env_assignments);
        set_error(ErrorType::RUNTIME_ERROR, "", "Environment variables set",
                  {});
        last_exit_code = 0;
        return 0;
    }

    std::vector<std::string> cmd_args(args.begin() + cmd_start_idx, args.end());

    pid_t pid = fork();

    if (pid == -1) {
        std::string cmd_name = cmd_args.empty() ? "unknown" : cmd_args[0];
        set_error(ErrorType::RUNTIME_ERROR, cmd_name,
                  "failed to create background process: " +
                      std::string(strerror(errno)),
                  {});
        last_exit_code = EX_OSERR;
        return EX_OSERR;
    }

    if (pid == 0) {
        apply_env_assignments(env_assignments);

        if (setpgid(0, 0) < 0) {
            std::cerr
                << "cjsh: runtime error: setpgid: failed to set process group "
                   "ID in background child: "
                << strerror(errno) << std::endl;
            _exit(EXIT_FAILURE);
        }

        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);

        auto c_args = build_exec_argv(cmd_args);
        execvp(cmd_args[0].c_str(), c_args.data());
        int saved_errno = errno;
        _exit((saved_errno == EACCES || saved_errno == EISDIR ||
               saved_errno == ENOEXEC)
                  ? 126
                  : 127);
    } else {
        if (setpgid(pid, pid) < 0 && errno != EACCES && errno != EPERM) {
            set_error(
                ErrorType::RUNTIME_ERROR, "setpgid",
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
        JobManager::instance().add_job(pid, {pid}, full_command,
                                       /*background=*/true,
                                       /*reads_stdin=*/false);
        JobManager::instance().set_last_background_pid(pid);

        std::cerr << "[" << job_id << "] " << pid << std::endl;
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
        const Command& cmd = commands[0];

        if (can_execute_in_process(cmd)) {
            last_exit_code =
                g_shell->get_built_ins()->builtin_command(cmd.args);
            return last_exit_code;
        }

        if (cmd.background) {
            return execute_command_async(cmd.args);
        } else {
            if (!cmd.args.empty() && g_shell && g_shell->get_built_ins() &&
                g_shell->get_built_ins()->is_builtin_command(cmd.args[0])) {
                if (cmd.args[0] == "__INTERNAL_SUBSHELL__") {
                    int orig_stdin = dup(STDIN_FILENO);
                    int orig_stdout = dup(STDOUT_FILENO);
                    int orig_stderr = dup(STDERR_FILENO);

                    if (orig_stdin == -1 || orig_stdout == -1 ||
                        orig_stderr == -1) {
                        set_error(ErrorType::RUNTIME_ERROR, cmd.args[0],
                                  "failed to save original file descriptors",
                                  {});
                        last_exit_code = EX_OSERR;
                        return EX_OSERR;
                    }

                    int exit_code = 0;
                    std::vector<std::string> process_sub_files;
                    std::vector<pid_t> process_sub_pids;
                    Command modified_cmd = cmd;

                    try {
                        if (!cmd.process_substitutions.empty()) {
                            for (size_t i = 0;
                                 i < cmd.process_substitutions.size(); ++i) {
                                const std::string& proc_sub =
                                    cmd.process_substitutions[i];

                                if ((proc_sub.find("<(") == 0 ||
                                     proc_sub.find(">(") == 0) &&
                                    proc_sub.back() == ')') {
                                    bool is_input = proc_sub[0] == '<';
                                    std::string command = proc_sub.substr(
                                        2, proc_sub.length() - 3);

                                    std::string temp_file =
                                        "/tmp/cjsh_procsub_" +
                                        std::to_string(getpid()) + "_" +
                                        std::to_string(i);

                                    if (mkfifo(temp_file.c_str(), 0600) == -1) {
                                        throw std::runtime_error(
                                            "cjsh: Failed to create FIFO for "
                                            "process "
                                            "substitution: " +
                                            std::string(strerror(errno)));
                                    }

                                    process_sub_files.push_back(temp_file);

                                    pid_t pid = fork();
                                    if (pid == -1) {
                                        throw std::runtime_error(
                                            "cjsh: Failed to fork for process "
                                            "substitution: " +
                                            std::string(strerror(errno)));
                                    }

                                    if (pid == 0) {
                                        if (is_input) {
                                            auto fifo_result = cjsh_filesystem::
                                                FileOperations::safe_open(
                                                    temp_file.c_str(),
                                                    O_WRONLY);
                                            if (fifo_result.is_error()) {
                                                std::cerr
                                                    << "cjsh: file not found: "
                                                       "open: failed to "
                                                       "open FIFO for writing: "
                                                    << fifo_result.error()
                                                    << std::endl;
                                                _exit(1);
                                            }
                                            auto dup_result = cjsh_filesystem::
                                                FileOperations::safe_dup2(
                                                    fifo_result.value(),
                                                    STDOUT_FILENO);
                                            if (dup_result.is_error()) {
                                                std::cerr
                                                    << "cjsh: runtime error: "
                                                       "dup2: failed to "
                                                       "duplicate stdout "
                                                       "descriptor: "
                                                    << dup_result.error()
                                                    << std::endl;
                                                cjsh_filesystem::
                                                    FileOperations::safe_close(
                                                        fifo_result.value());
                                                _exit(1);
                                            }
                                            cjsh_filesystem::FileOperations::
                                                safe_close(fifo_result.value());
                                        } else {
                                            auto fifo_result = cjsh_filesystem::
                                                FileOperations::safe_open(
                                                    temp_file.c_str(),
                                                    O_RDONLY);
                                            if (fifo_result.is_error()) {
                                                std::cerr
                                                    << "cjsh: file not found: "
                                                       "open: failed to "
                                                       "open FIFO for reading: "
                                                    << fifo_result.error()
                                                    << std::endl;
                                                _exit(1);
                                            }
                                            auto dup_result = cjsh_filesystem::
                                                FileOperations::safe_dup2(
                                                    fifo_result.value(),
                                                    STDIN_FILENO);
                                            if (dup_result.is_error()) {
                                                std::cerr
                                                    << "cjsh: runtime error: "
                                                       "dup2: failed to "
                                                       "duplicate stdin "
                                                       "descriptor: "
                                                    << dup_result.error()
                                                    << std::endl;
                                                cjsh_filesystem::
                                                    FileOperations::safe_close(
                                                        fifo_result.value());
                                                _exit(1);
                                            }
                                            cjsh_filesystem::FileOperations::
                                                safe_close(fifo_result.value());
                                        }

                                        if (g_shell) {
                                            int result =
                                                g_shell->execute(command);
                                            _exit(result);
                                        } else {
                                            _exit(1);
                                        }
                                    }

                                    process_sub_pids.push_back(pid);

                                    for (auto& arg : modified_cmd.args) {
                                        size_t pos = 0;
                                        while (
                                            (pos = arg.find(proc_sub, pos)) !=
                                            std::string::npos) {
                                            arg.replace(pos, proc_sub.length(),
                                                        temp_file);
                                            pos += temp_file.length();
                                        }
                                    }
                                }
                            }
                        }

                        if (!modified_cmd.input_file.empty()) {
                            auto redirect_result =
                                cjsh_filesystem::FileOperations::redirect_fd(
                                    modified_cmd.input_file.c_str(),
                                    STDIN_FILENO, O_RDONLY);
                            if (redirect_result.is_error()) {
                                throw std::runtime_error(
                                    "cjsh: Failed to redirect stdin from "
                                    "file: " +
                                    modified_cmd.input_file + " - " +
                                    redirect_result.error());
                            }
                        }

                        if (!modified_cmd.here_string.empty()) {
                            auto here_error = setup_here_string_stdin(
                                modified_cmd.here_string);
                            if (here_error.has_value()) {
                                switch (here_error->type) {
                                    case HereStringErrorType::Pipe:
                                        throw std::runtime_error(
                                            "cjsh: Failed to create pipe for "
                                            "here string");
                                    case HereStringErrorType::Write:
                                        throw std::runtime_error(
                                            "cjsh: Failed to write here string "
                                            "content");
                                    case HereStringErrorType::Dup:
                                        throw std::runtime_error(
                                            "cjsh: Failed to redirect stdin "
                                            "for here string: " +
                                            here_error->detail);
                                }
                            }
                        }

                        if (!modified_cmd.output_file.empty()) {
                            if (should_noclobber_prevent_overwrite(
                                    modified_cmd.output_file,
                                    modified_cmd.force_overwrite)) {
                                throw std::runtime_error(
                                    "cjsh: Cannot overwrite existing file '" +
                                    modified_cmd.output_file +
                                    "' (noclobber is set)");
                            }

                            auto redirect_result =
                                cjsh_filesystem::FileOperations::redirect_fd(
                                    modified_cmd.output_file.c_str(),
                                    STDOUT_FILENO,
                                    O_WRONLY | O_CREAT | O_TRUNC);
                            if (redirect_result.is_error()) {
                                throw std::runtime_error(
                                    "cjsh: Failed to redirect stdout to "
                                    "file: " +
                                    modified_cmd.output_file + " - " +
                                    redirect_result.error());
                            }
                        }

                        if (modified_cmd.both_output &&
                            !modified_cmd.both_output_file.empty()) {
                            if (should_noclobber_prevent_overwrite(
                                    modified_cmd.both_output_file)) {
                                throw std::runtime_error(
                                    "cjsh: Cannot overwrite existing file '" +
                                    modified_cmd.both_output_file +
                                    "' (noclobber is set)");
                            }

                            auto stdout_result =
                                cjsh_filesystem::FileOperations::redirect_fd(
                                    modified_cmd.both_output_file.c_str(),
                                    STDOUT_FILENO,
                                    O_WRONLY | O_CREAT | O_TRUNC);
                            if (stdout_result.is_error()) {
                                throw std::runtime_error(
                                    "cjsh: Failed to redirect stdout for &>: " +
                                    modified_cmd.both_output_file + " - " +
                                    stdout_result.error());
                            }

                            auto stderr_result =
                                cjsh_filesystem::FileOperations::safe_dup2(
                                    STDOUT_FILENO, STDERR_FILENO);
                            if (stderr_result.is_error()) {
                                throw std::runtime_error(
                                    "cjsh: Failed to redirect stderr for &>: " +
                                    stderr_result.error());
                            }
                        }

                        if (!cmd.append_file.empty()) {
                            auto redirect_result =
                                cjsh_filesystem::FileOperations::redirect_fd(
                                    cmd.append_file.c_str(), STDOUT_FILENO,
                                    O_WRONLY | O_CREAT | O_APPEND);
                            if (redirect_result.is_error()) {
                                throw std::runtime_error(
                                    "cjsh: Failed to redirect stdout for "
                                    "append: " +
                                    cmd.append_file + " - " +
                                    redirect_result.error());
                            }
                        }

                        if (!cmd.stderr_file.empty()) {
                            if (!cmd.stderr_append &&
                                should_noclobber_prevent_overwrite(
                                    cmd.stderr_file)) {
                                throw std::runtime_error(
                                    "cjsh: Cannot overwrite existing file '" +
                                    cmd.stderr_file + "' (noclobber is set)");
                            }

                            int flags =
                                O_WRONLY | O_CREAT |
                                (cmd.stderr_append ? O_APPEND : O_TRUNC);
                            auto redirect_result =
                                cjsh_filesystem::FileOperations::redirect_fd(
                                    cmd.stderr_file.c_str(), STDERR_FILENO,
                                    flags);
                            if (redirect_result.is_error()) {
                                throw std::runtime_error(
                                    "cjsh: Failed to redirect stderr: " +
                                    cmd.stderr_file + " - " +
                                    redirect_result.error());
                            }
                        }

                        if (cmd.stderr_to_stdout) {
                            auto dup_result =
                                cjsh_filesystem::FileOperations::safe_dup2(
                                    STDOUT_FILENO, STDERR_FILENO);
                            if (dup_result.is_error()) {
                                throw std::runtime_error(
                                    "cjsh: Failed to redirect stderr to "
                                    "stdout: " +
                                    dup_result.error());
                            }
                        }

                        if (cmd.stdout_to_stderr) {
                            auto dup_result =
                                cjsh_filesystem::FileOperations::safe_dup2(
                                    STDERR_FILENO, STDOUT_FILENO);
                            if (dup_result.is_error()) {
                                throw std::runtime_error(
                                    "cjsh: Failed to redirect stdout to "
                                    "stderr: " +
                                    dup_result.error());
                            }
                        }

                        if (!cmd.args.empty() && cmd.args[0] == "exec" &&
                            (!cmd.fd_redirections.empty() ||
                             !cmd.fd_duplications.empty())) {
                            for (const auto& fd_redir : cmd.fd_redirections) {
                                int fd_num = fd_redir.first;
                                const std::string& spec = fd_redir.second;

                                std::string file;
                                int flags;

                                if (spec.find("input:") == 0) {
                                    file = spec.substr(6);
                                    flags = O_RDONLY;
                                } else if (spec.find("output:") == 0) {
                                    file = spec.substr(7);
                                    flags = O_WRONLY | O_CREAT | O_TRUNC;
                                } else {
                                    file = spec;
                                    if (fd_num == 0) {
                                        flags = O_RDONLY;
                                    } else {
                                        flags = O_WRONLY | O_CREAT | O_TRUNC;
                                    }
                                }

                                auto redirect_result =
                                    cjsh_filesystem::FileOperations::
                                        redirect_fd(file, fd_num, flags);
                                if (redirect_result.is_error()) {
                                    throw std::runtime_error(
                                        "cjsh: exec: " + spec + ": " +
                                        redirect_result.error());
                                }
                            }

                            for (const auto& fd_dup : cmd.fd_duplications) {
                                int dst_fd = fd_dup.first;
                                int src_fd = fd_dup.second;

                                auto dup_result =
                                    cjsh_filesystem::FileOperations::safe_dup2(
                                        src_fd, dst_fd);
                                if (dup_result.is_error()) {
                                    throw std::runtime_error(
                                        "cjsh: exec: dup2 failed for " +
                                        std::to_string(dst_fd) + ">&" +
                                        std::to_string(src_fd) + ": " +
                                        dup_result.error());
                                }
                            }

                            if (cmd.args.size() == 1) {
                                exit_code = 0;
                            } else {
                                std::vector<std::string> exec_args(
                                    cmd.args.begin() + 1, cmd.args.end());
                                exit_code =
                                    g_shell->execute_command(exec_args, false);
                            }
                        } else {
                            exit_code =
                                g_shell->get_built_ins()->builtin_command(
                                    modified_cmd.args);
                        }

                    } catch (const std::exception& e) {
                        set_error(ErrorType::RUNTIME_ERROR, cmd.args[0],
                                  std::string(e.what()));
                        exit_code = EX_OSERR;
                    }

                    for (size_t i = 0; i < process_sub_pids.size(); ++i) {
                        int status;
                        waitpid(process_sub_pids[i], &status, 0);
                    }

                    for (const std::string& temp_file : process_sub_files) {
                        unlink(temp_file.c_str());
                    }

                    cjsh_filesystem::FileOperations::safe_dup2(orig_stdin,
                                                               STDIN_FILENO);
                    cjsh_filesystem::FileOperations::safe_dup2(orig_stdout,
                                                               STDOUT_FILENO);
                    cjsh_filesystem::FileOperations::safe_dup2(orig_stderr,
                                                               STDERR_FILENO);
                    cjsh_filesystem::FileOperations::safe_close(orig_stdin);
                    cjsh_filesystem::FileOperations::safe_close(orig_stdout);
                    cjsh_filesystem::FileOperations::safe_close(orig_stderr);

                    auto exit_result = make_exit_error_result(
                        cmd.args[0], exit_code,
                        "builtin command completed successfully",
                        "builtin command failed with exit code ");
                    set_error(exit_result.type, cmd.args[0],
                              exit_result.message, exit_result.suggestions);
                    last_exit_code = exit_code;
                    return exit_code;
                }
            }

            pid_t pid = fork();

            if (pid == -1) {
                set_error(
                    ErrorType::RUNTIME_ERROR,
                    cmd.args.empty() ? "unknown" : cmd.args[0],
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
                              << strerror(errno) << std::endl;
                    _exit(EXIT_FAILURE);
                }

                if (shell_is_interactive) {
                    if (tcsetpgrp(shell_terminal, child_pid) < 0) {
                        std::cerr
                            << "cjsh: runtime error: tcsetpgrp: failed to set "
                               "terminal foreground process group in child: "
                            << strerror(errno) << std::endl;
                    }
                }

                signal(SIGINT, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGTTIN, SIG_DFL);
                signal(SIGTTOU, SIG_DFL);
                signal(SIGCHLD, SIG_DFL);
                signal(SIGTERM, SIG_DFL);

                sigset_t set;
                sigemptyset(&set);
                sigaddset(&set, SIGINT);
                sigaddset(&set, SIGQUIT);
                sigaddset(&set, SIGTSTP);
                sigaddset(&set, SIGCHLD);
                sigaddset(&set, SIGTERM);
                sigprocmask(SIG_UNBLOCK, &set, nullptr);

                if (!cmd.here_doc.empty()) {
                    int here_pipe[2];
                    if (pipe(here_pipe) == -1) {
                        std::cerr << "cjsh: runtime error: pipe: failed to "
                                     "create pipe for "
                                     "here document: "
                                  << strerror(errno) << std::endl;
                        _exit(EXIT_FAILURE);
                    }

                    ssize_t bytes_written =
                        write(here_pipe[1], cmd.here_doc.c_str(),
                              cmd.here_doc.length());
                    if (bytes_written == -1) {
                        std::cerr << "cjsh: runtime error: write: failed to "
                                     "write here "
                                     "document content: "
                                  << strerror(errno) << std::endl;
                        close(here_pipe[1]);
                        _exit(EXIT_FAILURE);
                    }
                    bytes_written = write(here_pipe[1], "\n", 1);
                    if (bytes_written == -1) {
                        std::cerr << "cjsh: runtime error: write: failed to "
                                     "write here "
                                     "document newline: "
                                  << strerror(errno) << std::endl;
                        close(here_pipe[1]);
                        _exit(EXIT_FAILURE);
                    }
                    close(here_pipe[1]);

                    auto dup_result =
                        cjsh_filesystem::FileOperations::safe_dup2(
                            here_pipe[0], STDIN_FILENO);
                    if (dup_result.is_error()) {
                        std::cerr << "cjsh: runtime error: dup2: failed to "
                                     "duplicate here "
                                     "document descriptor: "
                                  << dup_result.error() << std::endl;
                        cjsh_filesystem::FileOperations::safe_close(
                            here_pipe[0]);
                        _exit(EXIT_FAILURE);
                    }
                    cjsh_filesystem::FileOperations::safe_close(here_pipe[0]);
                }

                else if (!cmd.here_string.empty()) {
                    auto here_error = setup_here_string_stdin(cmd.here_string);
                    if (here_error.has_value()) {
                        switch (here_error->type) {
                            case HereStringErrorType::Pipe:
                                std::cerr << "cjsh: runtime error: pipe: "
                                             "failed to create pipe for "
                                             "here string: "
                                          << here_error->detail << std::endl;
                                break;
                            case HereStringErrorType::Write:
                                std::cerr << "cjsh: runtime error: write: "
                                             "failed to write here "
                                             "string content: "
                                          << here_error->detail << std::endl;
                                break;
                            case HereStringErrorType::Dup:
                                std::cerr << "cjsh: runtime error: dup2: "
                                             "failed to duplicate here "
                                             "string descriptor: "
                                          << here_error->detail << std::endl;
                                break;
                        }
                        _exit(EXIT_FAILURE);
                    }
                } else if (!cmd.input_file.empty()) {
                    auto redirect_result =
                        cjsh_filesystem::FileOperations::redirect_fd(
                            cmd.input_file, STDIN_FILENO, O_RDONLY);
                    if (redirect_result.is_error()) {
                        std::cerr << "cjsh: file not found: " << cmd.input_file
                                  << ": " << redirect_result.error()
                                  << std::endl;
                        _exit(EXIT_FAILURE);
                    }
                }

                if (!cmd.output_file.empty()) {
                    if (should_noclobber_prevent_overwrite(
                            cmd.output_file, cmd.force_overwrite)) {
                        std::cerr
                            << "cjsh: permission denied: " << cmd.output_file
                            << ": cannot overwrite existing file (noclobber is "
                               "set)"
                            << std::endl;
                        _exit(EXIT_FAILURE);
                    }

                    auto redirect_result =
                        cjsh_filesystem::FileOperations::redirect_fd(
                            cmd.output_file, STDOUT_FILENO,
                            O_WRONLY | O_CREAT | O_TRUNC);
                    if (redirect_result.is_error()) {
                        std::cerr << "cjsh: file not found: " << cmd.output_file
                                  << ": " << redirect_result.error()
                                  << std::endl;
                        _exit(EXIT_FAILURE);
                    }
                }

                if (cmd.both_output && !cmd.both_output_file.empty()) {
                    if (should_noclobber_prevent_overwrite(
                            cmd.both_output_file)) {
                        std::cerr << "cjsh: permission denied: "
                                  << cmd.both_output_file
                                  << ": cannot overwrite existing file "
                                     "(noclobber is set)"
                                  << std::endl;
                        _exit(EXIT_FAILURE);
                    }

                    // Redirect stdout first
                    auto stdout_result =
                        cjsh_filesystem::FileOperations::redirect_fd(
                            cmd.both_output_file, STDOUT_FILENO,
                            O_WRONLY | O_CREAT | O_TRUNC);
                    if (stdout_result.is_error()) {
                        std::cerr
                            << "cjsh: file not found: " << cmd.both_output_file
                            << ": " << stdout_result.error() << std::endl;
                        _exit(EXIT_FAILURE);
                    }

                    // Duplicate stdout to stderr
                    auto stderr_result =
                        cjsh_filesystem::FileOperations::safe_dup2(
                            STDOUT_FILENO, STDERR_FILENO);
                    if (stderr_result.is_error()) {
                        std::cerr << "cjsh: runtime error: dup2: failed for "
                                     "stderr in &> "
                                     "redirection: "
                                  << stderr_result.error() << std::endl;
                        _exit(EXIT_FAILURE);
                    }
                }

                if (!cmd.append_file.empty()) {
                    int fd = open(cmd.append_file.c_str(),
                                  O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (fd == -1) {
                        std::cerr << "cjsh: file not found: " << cmd.append_file
                                  << ": " << strerror(errno) << std::endl;
                        _exit(EXIT_FAILURE);
                    }
                    if (dup2(fd, STDOUT_FILENO) == -1) {
                        std::cerr << "cjsh: runtime error: dup2: failed for "
                                     "append redirection: "
                                  << strerror(errno) << std::endl;
                        close(fd);
                        _exit(EXIT_FAILURE);
                    }
                    close(fd);
                }

                if (!cmd.stderr_file.empty()) {
                    if (!cmd.stderr_append &&
                        should_noclobber_prevent_overwrite(cmd.stderr_file)) {
                        std::cerr
                            << "cjsh: permission denied: " << cmd.stderr_file
                            << ": cannot overwrite existing file (noclobber is "
                               "set)"
                            << std::endl;
                        _exit(EXIT_FAILURE);
                    }

                    int flags = O_WRONLY | O_CREAT |
                                (cmd.stderr_append ? O_APPEND : O_TRUNC);
                    int fd = open(cmd.stderr_file.c_str(), flags, 0644);
                    if (fd == -1) {
                        std::cerr << "cjsh: " << cmd.stderr_file << ": "
                                  << strerror(errno) << std::endl;
                        _exit(EXIT_FAILURE);
                    }
                    if (dup2(fd, STDERR_FILENO) == -1) {
                        perror("cjsh: dup2 stderr");
                        close(fd);
                        _exit(EXIT_FAILURE);
                    }
                    close(fd);
                } else if (cmd.stderr_to_stdout) {
                    if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1) {
                        perror("cjsh: dup2 2>&1");
                        _exit(EXIT_FAILURE);
                    }
                }

                if (cmd.stdout_to_stderr) {
                    if (dup2(STDERR_FILENO, STDOUT_FILENO) == -1) {
                        perror("cjsh: dup2 >&2");
                        _exit(EXIT_FAILURE);
                    }
                }

                for (const auto& fd_redir : cmd.fd_redirections) {
                    int fd_num = fd_redir.first;
                    const std::string& spec = fd_redir.second;

                    std::string file;
                    int flags;

                    if (spec.find("input:") == 0) {
                        file = spec.substr(6);
                        flags = O_RDONLY;
                    } else if (spec.find("output:") == 0) {
                        file = spec.substr(7);
                        flags = O_WRONLY | O_CREAT | O_TRUNC;
                    } else {
                        file = spec;
                        if (fd_num == 0) {
                            flags = O_RDONLY;
                        } else {
                            flags = O_WRONLY | O_CREAT | O_TRUNC;
                        }
                    }

                    auto redirect_result =
                        cjsh_filesystem::FileOperations::redirect_fd(
                            file, fd_num, flags);
                    if (redirect_result.is_error()) {
                        std::cerr << "cjsh: file not found: " << spec << ": "
                                  << redirect_result.error() << std::endl;
                        _exit(EXIT_FAILURE);
                    }
                }

                for (const auto& fd_dup : cmd.fd_duplications) {
                    int src_fd = fd_dup.first;
                    int dst_fd = fd_dup.second;

                    auto dup_result =
                        cjsh_filesystem::FileOperations::safe_dup2(dst_fd,
                                                                   src_fd);
                    if (dup_result.is_error()) {
                        std::cerr << "cjsh: runtime error: dup2: failed for "
                                  << src_fd << ">&" << dst_fd << ": "
                                  << dup_result.error() << std::endl;
                        _exit(EXIT_FAILURE);
                    }
                }

                auto c_args = build_exec_argv(cmd.args);
                execvp(cmd.args[0].c_str(), c_args.data());
                int saved_errno = errno;
                _exit((saved_errno == EACCES || saved_errno == EISDIR ||
                       saved_errno == ENOEXEC)
                          ? 126
                          : 127);
            }

            if (g_shell && !g_shell->get_interactive_mode()) {
                int status = 0;
                pid_t wpid;
                do {
                    wpid = waitpid(pid, &status, 0);
                } while (wpid == -1 && errno == EINTR);

                int exit_code = 0;
                if (wpid == -1) {
                    exit_code = EX_OSERR;
                } else if (WIFEXITED(status)) {
                    exit_code = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    exit_code = 128 + WTERMSIG(status);
                }

                auto exit_result = make_exit_error_result(
                    cmd.args[0], exit_code, "command completed successfully",
                    "command failed with exit code ");
                set_error(exit_result.type, cmd.args[0], exit_result.message,
                          exit_result.suggestions);
                last_exit_code = exit_code;
                if (g_debug_mode) {
                    std::cerr << "DEBUG: execute_pipeline single-command "
                                 "(non-interactive) exit="
                              << last_exit_code << std::endl;
                }
                return last_exit_code;
            } else {
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
                bool reads_stdin = command_consumes_terminal_stdin(cmd);
                int managed_job_id = JobManager::instance().add_job(
                    pid, {pid}, full_command, job.background, reads_stdin);
                put_job_in_foreground(job_id, false);

                if (!cmd.output_file.empty() || !cmd.append_file.empty() ||
                    !cmd.stderr_file.empty()) {
                    sync();
                }

                {
                    std::lock_guard<std::mutex> lock(jobs_mutex);
                    auto it = jobs.find(job_id);
                    if (it != jobs.end() && it->second.completed) {
                        JobManager::instance().remove_job(managed_job_id);
                    }
                }

                if (g_debug_mode) {
                    std::cerr
                        << "DEBUG: execute_pipeline single-command returning "
                           "last_exit_code="
                        << last_exit_code << std::endl;
                }
                return last_exit_code;
            }
        }

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
                          "command " + std::to_string(i + 1) +
                              " in pipeline is empty");
                print_last_error();

                for (size_t j = 0; j < commands.size() - 1; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }

                return 1;
            }

            pid_t pid = fork();

            if (pid == -1) {
                std::string cmd_name =
                    cmd.args.empty() ? "unknown" : cmd.args[0];
                set_error(ErrorType::RUNTIME_ERROR, cmd_name,
                          "failed to create process (command " +
                              std::to_string(i + 1) +
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

                if (shell_is_interactive && i == 0) {
                    if (tcsetpgrp(shell_terminal, pgid) < 0) {
                        perror("cjsh: tcsetpgrp failed in child");
                    }
                }

                signal(SIGINT, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGTTIN, SIG_DFL);
                signal(SIGTTOU, SIG_DFL);
                signal(SIGCHLD, SIG_DFL);
                signal(SIGTERM, SIG_DFL);
                if (i == 0) {
                    if (!cmd.here_doc.empty()) {
                        int here_pipe[2];
                        if (pipe(here_pipe) == -1) {
                            perror("cjsh: pipe for here document");
                            _exit(EXIT_FAILURE);
                        }

                        ssize_t bytes_written =
                            write(here_pipe[1], cmd.here_doc.c_str(),
                                  cmd.here_doc.length());
                        if (bytes_written == -1) {
                            perror("cjsh: write here document content");
                            close(here_pipe[1]);
                            _exit(EXIT_FAILURE);
                        }
                        bytes_written = write(here_pipe[1], "\n", 1);
                        if (bytes_written == -1) {
                            perror("cjsh: write here document newline");
                            close(here_pipe[1]);
                            _exit(EXIT_FAILURE);
                        }
                        close(here_pipe[1]);

                        if (dup2(here_pipe[0], STDIN_FILENO) == -1) {
                            perror("cjsh: dup2 here document");
                            close(here_pipe[0]);
                            _exit(EXIT_FAILURE);
                        }
                        close(here_pipe[0]);
                    } else if (!cmd.input_file.empty()) {
                        int fd = open(cmd.input_file.c_str(), O_RDONLY);
                        if (fd == -1) {
                            std::cerr << "cjsh: " << cmd.input_file << ": "
                                      << strerror(errno) << std::endl;
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
                        int fd = open(cmd.output_file.c_str(),
                                      O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        if (fd == -1) {
                            std::cerr << "cjsh: " << cmd.output_file << ": "
                                      << strerror(errno) << std::endl;
                            _exit(EXIT_FAILURE);
                        }
                        if (dup2(fd, STDOUT_FILENO) == -1) {
                            perror("cjsh: dup2 output");
                            close(fd);
                            _exit(EXIT_FAILURE);
                        }
                        close(fd);
                    } else if (!cmd.append_file.empty()) {
                        int fd = open(cmd.append_file.c_str(),
                                      O_WRONLY | O_CREAT | O_APPEND, 0644);
                        if (fd == -1) {
                            std::cerr << "cjsh: " << cmd.append_file << ": "
                                      << strerror(errno) << std::endl;
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

                if (!cmd.stderr_file.empty()) {
                    int flags = O_WRONLY | O_CREAT |
                                (cmd.stderr_append ? O_APPEND : O_TRUNC);
                    int fd = open(cmd.stderr_file.c_str(), flags, 0644);
                    if (fd == -1) {
                        std::cerr << "cjsh: " << cmd.stderr_file << ": "
                                  << strerror(errno) << std::endl;
                        _exit(EXIT_FAILURE);
                    }
                    if (dup2(fd, STDERR_FILENO) == -1) {
                        perror("cjsh: dup2 stderr");
                        close(fd);
                        _exit(EXIT_FAILURE);
                    }
                    close(fd);
                } else if (cmd.stderr_to_stdout) {
                    if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1) {
                        perror("cjsh: dup2 2>&1");
                        _exit(EXIT_FAILURE);
                    }
                }

                if (cmd.stdout_to_stderr) {
                    if (dup2(STDERR_FILENO, STDOUT_FILENO) == -1) {
                        perror("cjsh: dup2 >&2");
                        _exit(EXIT_FAILURE);
                    }
                }

                for (size_t j = 0; j < commands.size() - 1; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }

                if (g_shell && g_shell->get_built_ins() &&
                    g_shell->get_built_ins()->is_builtin_command(cmd.args[0])) {
                    int exit_code =
                        g_shell->get_built_ins()->builtin_command(cmd.args);

                    fflush(stdout);
                    fflush(stderr);

                    _exit(exit_code);
                } else {
                    auto c_args = build_exec_argv(cmd.args);
                    execvp(cmd.args[0].c_str(), c_args.data());
                    int saved_errno = errno;
                    _exit((saved_errno == EACCES || saved_errno == EISDIR ||
                           saved_errno == ENOEXEC)
                              ? 126
                              : 127);
                }
            }

            if (i == 0) {
                pgid = pid;
            }

            if (setpgid(pid, pgid) < 0) {
                if (errno != EACCES && errno != EPERM) {
                    set_error(
                        ErrorType::RUNTIME_ERROR, "setpgid",
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
    int new_job_id = JobManager::instance().add_job(
        pgid, pids, pipeline_command, job.background,
        pipeline_consumes_terminal_stdin(commands));

    if (job.background) {
        JobManager::instance().set_last_background_pid(
            pids.empty() ? -1 : pids.back());
    }

    if (job.background) {
        put_job_in_background(job_id, false);
        std::cerr << "[" << job_id << "] " << pgid << std::endl;
        last_exit_code = 0;
    } else {
        put_job_in_foreground(job_id, false);

        std::lock_guard<std::mutex> lock(jobs_mutex);
        auto it = jobs.find(job_id);
        if (it != jobs.end()) {
            JobManager::instance().remove_job(new_job_id);

            int st = it->second.last_status;
            if (WIFEXITED(st))
                last_exit_code = WEXITSTATUS(st);
            else if (WIFSIGNALED(st))
                last_exit_code = 128 + WTERMSIG(st);
        }
    }

    return last_exit_code;
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

void Exec::update_job_status(int job_id, bool completed, bool stopped,
                             int status) {
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
        set_error(ErrorType::RUNTIME_ERROR, "job",
                  "job [" + std::to_string(job_id) + "] not found");
        print_last_error();
        return;
    }

    Job& job = it->second;

    bool terminal_control_acquired = false;
    if (shell_is_interactive && isatty(shell_terminal)) {
        if (tcsetpgrp(shell_terminal, job.pgid) == 0) {
            terminal_control_acquired = true;
            if (g_debug_mode) {
                std::cerr << "DEBUG: Gave terminal control to job " << job_id
                          << " (pgid: " << job.pgid << ")" << std::endl;
            }
        } else {
            if (errno != ENOTTY && errno != EINVAL && errno != EPERM) {
                if (g_debug_mode) {
                    std::cerr
                        << "DEBUG: Could not give terminal control to job "
                        << job_id << ": " << strerror(errno) << std::endl;
                }
            }
        }
    }

    if (cont && job.stopped) {
        if (kill(-job.pgid, SIGCONT) < 0) {
            set_error(ErrorType::RUNTIME_ERROR, "kill",
                      "failed to send SIGCONT to job: " +
                          std::string(strerror(errno)));
        }
        job.stopped = false;
    }

    jobs_mutex.unlock();

    wait_for_job(job_id);

    jobs_mutex.lock();

    if (terminal_control_acquired && shell_is_interactive &&
        isatty(shell_terminal)) {
        if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
            if (errno != ENOTTY && errno != EINVAL) {
                set_error(ErrorType::RUNTIME_ERROR, "tcsetpgrp",
                          "warning: failed to restore terminal control: " +
                              std::string(strerror(errno)));
            }
        }

        if (tcgetattr(shell_terminal, &shell_tmodes) == 0) {
            if (tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes) < 0) {
                set_error(ErrorType::RUNTIME_ERROR, "tcsetattr",
                          "failed to restore terminal attributes: " +
                              std::string(strerror(errno)));
            }
        }
    }
}

void Exec::put_job_in_background(int job_id, bool cont) {
    std::lock_guard<std::mutex> lock(jobs_mutex);

    auto it = jobs.find(job_id);
    if (it == jobs.end()) {
        set_error(ErrorType::RUNTIME_ERROR, "job",
                  "job [" + std::to_string(job_id) + "] not found");
        print_last_error();
        return;
    }

    Job& job = it->second;

    if (cont && job.stopped) {
        if (kill(-job.pgid, SIGCONT) < 0) {
            set_error(ErrorType::RUNTIME_ERROR, "kill",
                      "failed to send SIGCONT to background job: " +
                          std::string(strerror(errno)));
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
    pid_t pid;

    bool job_stopped = false;
    bool saw_last = false;
    int last_status = 0;

    while (!remaining_pids.empty()) {
        pid = waitpid(-job_pgid, &status, WUNTRACED);

        if (pid == -1) {
            if (errno == EINTR) {
                continue;
            } else if (errno == ECHILD) {
                remaining_pids.clear();
                break;
            } else {
                set_error(ErrorType::RUNTIME_ERROR, "waitpid",
                          "failed to wait for child process: " +
                              std::string(strerror(errno)));
                break;
            }
        }

        auto pid_it =
            std::find(remaining_pids.begin(), remaining_pids.end(), pid);
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
                final_status =
                    (job.last_status != 0) ? job.last_status : job.status;
            }
            job.last_status = final_status;

            if (WIFEXITED(final_status)) {
                int exit_status = WEXITSTATUS(final_status);
                last_exit_code = exit_status;
                job.completed = true;
                if (g_debug_mode) {
                    std::cerr << "DEBUG: wait_for_job setting last_exit_code="
                              << exit_status
                              << " from final_status=" << final_status
                              << std::endl;
                }
                auto exit_result = make_exit_error_result(
                    job.command, exit_status, "command completed successfully",
                    "command failed with exit code ");
                set_error(exit_result.type, job.command, exit_result.message,
                          exit_result.suggestions);
            } else if (WIFSIGNALED(final_status)) {
                last_exit_code = 128 + WTERMSIG(final_status);
                set_error(ErrorType::RUNTIME_ERROR, job.command,
                          "command terminated by signal " +
                              std::to_string(WTERMSIG(final_status)));
            }
        }
    }
}

void Exec::terminate_all_child_process() {
    std::lock_guard<std::mutex> lock(jobs_mutex);

    if (g_debug_mode && !jobs.empty()) {
        std::cerr << "DEBUG: Starting graceful termination of " << jobs.size()
                  << " jobs" << std::endl;
    }

    bool any_jobs_terminated = false;
    for (auto& job_pair : jobs) {
        Job& job = job_pair.second;
        if (!job.completed) {
            if (killpg(job.pgid, 0) == 0) {
                if (killpg(job.pgid, SIGTERM) == 0) {
                    any_jobs_terminated = true;
                    if (g_debug_mode) {
                        std::cerr << "DEBUG: Sent SIGTERM to job "
                                  << job_pair.first << " (pgid " << job.pgid
                                  << ")" << std::endl;
                    }
                } else {
                    if (errno != ESRCH) {
                        std::cerr << "cjsh: warning: failed to terminate job ["
                                  << job_pair.first << "] '" << job.command
                                  << "': " << strerror(errno) << std::endl;
                    }
                }
                std::cerr << "[" << job_pair.first << "] Terminated\t"
                          << job.command << std::endl;
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
                    if (g_debug_mode) {
                        std::cerr << "DEBUG: Sent SIGKILL to stubborn job "
                                  << job_pair.first << " (pgid " << job.pgid
                                  << ")" << std::endl;
                    }
                } else {
                    if (errno != ESRCH) {
                        set_error(ErrorType::RUNTIME_ERROR, "killpg",
                                  "failed to send SIGKILL in "
                                  "terminate_all_child_process: " +
                                      std::string(strerror(errno)));
                    }
                }

                // Also kill individual PIDs in case process group kill failed
                for (pid_t pid : job.pids) {
                    if (kill(pid, SIGKILL) == 0 && g_debug_mode) {
                        std::cerr << "DEBUG: Sent SIGKILL to individual PID "
                                  << pid << std::endl;
                    }
                }
            }

            job.completed = true;
            job.stopped = false;
            job.status = 0;
        }
    }

    int status;
    pid_t pid;
    int zombie_count = 0;
    const int max_terminate_iterations = 50;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0 &&
           zombie_count < max_terminate_iterations) {
        zombie_count++;
        if (g_debug_mode && zombie_count <= 3) {
            std::cerr << "DEBUG: Reaped zombie process " << pid << std::endl;
        }
    }

    if (zombie_count >= max_terminate_iterations) {
        std::cerr << "WARNING: terminate_all_child_process hit maximum cleanup "
                     "iterations"
                  << std::endl;
    }

    set_error(ErrorType::RUNTIME_ERROR, "", "All child processes terminated");
}

std::map<int, Job> Exec::get_jobs() {
    std::lock_guard<std::mutex> lock(jobs_mutex);
    return jobs;
}
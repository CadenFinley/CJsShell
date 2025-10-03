#include "git_info.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "cjsh.h"
#include "utils/cjsh_filesystem.h"

namespace {

bool set_close_on_exec(int fd) {
    int flags = fcntl(fd, F_GETFD);
    if (flags == -1) {
        return false;
    }
    if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
        return false;
    }
    return true;
}

std::vector<char*> build_exec_argv(const std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    return argv;
}

}  // namespace

static int safe_execute_git_command(const std::vector<std::string>& args, std::string& result,
                                    int& exit_code) {
    result.clear();
    exit_code = -1;

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        return -1;
    }

    if (!set_close_on_exec(pipefd[0]) || !set_close_on_exec(pipefd[1])) {
        cjsh_filesystem::FileOperations::safe_close(pipefd[0]);
        cjsh_filesystem::FileOperations::safe_close(pipefd[1]);
        return -1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        cjsh_filesystem::FileOperations::safe_close(pipefd[0]);
        cjsh_filesystem::FileOperations::safe_close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        cjsh_filesystem::FileOperations::safe_close(pipefd[0]);

        auto dup_result = cjsh_filesystem::FileOperations::safe_dup2(pipefd[1], STDOUT_FILENO);
        if (dup_result.is_error()) {
            _exit(127);
        }

        auto devnull_result = cjsh_filesystem::FileOperations::safe_open("/dev/null", O_WRONLY);
        if (devnull_result.is_ok()) {
            cjsh_filesystem::FileOperations::safe_dup2(devnull_result.value(), STDERR_FILENO);
            cjsh_filesystem::FileOperations::safe_close(devnull_result.value());
        }

        cjsh_filesystem::FileOperations::safe_close(pipefd[1]);

        auto argv = build_exec_argv(args);
        execvp(argv[0], argv.data());
        _exit(127);
    }

    cjsh_filesystem::FileOperations::safe_close(pipefd[1]);

    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        result += buffer;
    }

    cjsh_filesystem::FileOperations::safe_close(pipefd[0]);

    int status;
    if (waitpid(pid, &status, 0) == -1) {
        return -1;
    }

    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        exit_code = 128 + WTERMSIG(status);
    } else {
        exit_code = 1;
    }

    return 0;
}

GitInfo::GitInfo() {
    last_git_status_check = std::chrono::steady_clock::now() - std::chrono::seconds(30);
    is_git_status_check_running = false;
    cached_is_clean_repo = true;
}

GitInfo::~GitInfo() {
}

std::string GitInfo::get_git_remote(const std::filesystem::path& repo_root) {
    std::vector<std::string> cmd = {"git", "-C", repo_root.string(), "remote", "get-url",
                                    "origin"};
    std::string result;
    int exit_code;

    if (safe_execute_git_command(cmd, result, exit_code) != 0 || exit_code != 0) {
        return "";
    }

    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}

std::string GitInfo::get_git_tag(const std::filesystem::path& repo_root) {
    std::vector<std::string> cmd = {"git", "-C", repo_root.string(), "describe", "--tags",
                                    "--abbrev=0"};
    std::string result;
    int exit_code;

    if (safe_execute_git_command(cmd, result, exit_code) != 0 || exit_code != 0) {
        return "";
    }

    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}

std::string GitInfo::get_git_last_commit(const std::filesystem::path& repo_root) {
    std::vector<std::string> cmd = {"git", "-C", repo_root.string(), "log", "-1",
                                    "--pretty=format:%h:%s"};
    std::string result;
    int exit_code;

    if (safe_execute_git_command(cmd, result, exit_code) != 0 || exit_code != 0) {
        return "";
    }

    return result;
}

std::string GitInfo::get_git_author(const std::filesystem::path& repo_root) {
    std::vector<std::string> cmd = {"git", "-C", repo_root.string(), "log", "-1",
                                    "--pretty=format:%an"};
    std::string result;
    int exit_code;

    if (safe_execute_git_command(cmd, result, exit_code) != 0 || exit_code != 0) {
        return "";
    }

    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}

std::string GitInfo::get_git_branch(const std::filesystem::path& git_head_path) {
    try {
        auto read_result =
            cjsh_filesystem::FileOperations::read_file_content(git_head_path.string());
        if (read_result.is_error()) {
            return "";
        }

        std::string head_contents = read_result.value();

        if (!head_contents.empty() && head_contents.back() == '\n') {
            head_contents.pop_back();
        }

        const std::string ref_prefix = "ref: refs/heads/";
        if (head_contents.substr(0, ref_prefix.length()) == ref_prefix) {
            std::string branch = head_contents.substr(ref_prefix.length());
            return branch;
        } else {
            return head_contents.substr(0, 7);
        }
    } catch (const std::exception& e) {
        return "";
    }
}

std::string GitInfo::get_git_status(const std::filesystem::path& repo_root) {
    std::string status_symbols = "";
    std::string git_dir = repo_root.string();
    bool is_clean_repo = true;

    auto now = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(now - last_git_status_check).count();

    if ((elapsed > 60 || cached_git_dir != git_dir) && !is_git_status_check_running) {
        std::lock_guard<std::mutex> lock(git_status_mutex);
        if (!is_git_status_check_running) {
            is_git_status_check_running = true;

            std::vector<std::string> cmd = {"git", "-C", git_dir, "status", "--porcelain"};
            std::string result;
            int exit_code;

            if (safe_execute_git_command(cmd, result, exit_code) == 0 && exit_code == 0) {
                if (!result.empty()) {
                    is_clean_repo = false;
                    status_symbols = "✘";
                } else {
                    is_clean_repo = true;
                    status_symbols = "✓";
                }
            } else {
                is_clean_repo = true;
                status_symbols = "✓";
            }

            cached_git_dir = git_dir;
            cached_status_symbols = status_symbols;
            cached_is_clean_repo = is_clean_repo;
            last_git_status_check = now;
            is_git_status_check_running = false;
        }
    } else {
        status_symbols = cached_status_symbols;
        is_clean_repo = cached_is_clean_repo;
    }

    if (is_clean_repo) {
        return "✓";
    } else {
        return "✘";
    }
}

std::string GitInfo::get_local_path(const std::filesystem::path& repo_root) {
    std::filesystem::path cwd = std::filesystem::current_path();
    std::string repo_root_path = repo_root.string();
    std::string repo_root_name = repo_root.filename().string();
    std::string current_path_str = cwd.string();

    std::string result;
    if (current_path_str == repo_root_path) {
        result = repo_root_name;
    } else {
        try {
            std::filesystem::path relative_path = std::filesystem::relative(cwd, repo_root);
            result = repo_root_name + "/" + relative_path.string();
        } catch (const std::exception& e) {
            result = cwd.filename().string();
        }
    }
    return result;
}

int GitInfo::get_git_ahead_behind(const std::filesystem::path& repo_root, int& ahead, int& behind) {
    ahead = 0;
    behind = 0;

    std::filesystem::path git_head_path = repo_root / ".git" / "HEAD";
    std::string branch = get_git_branch(git_head_path);
    if (branch.empty()) {
        return -1;
    }

    std::vector<std::string> upstream_cmd = {"git", "-C", repo_root.string(), "rev-parse",
                                             "--abbrev-ref", branch + "@{upstream}"};
    std::string upstream;
    int exit_code;

    if (safe_execute_git_command(upstream_cmd, upstream, exit_code) != 0 || exit_code != 0) {
        return -1;
    }

    if (!upstream.empty() && upstream.back() == '\n') {
        upstream.pop_back();
    }

    if (upstream.empty()) {
        return -1;
    }

    std::vector<std::string> count_cmd = {"git", "-C", repo_root.string(), "rev-list",
                                          "--left-right", "--count",
                                          branch + "..." + upstream};
    std::string count_result;

    if (safe_execute_git_command(count_cmd, count_result, exit_code) != 0 || exit_code != 0) {
        return -1;
    }

    size_t tab_pos = count_result.find('\t');
    if (tab_pos != std::string::npos) {
        try {
            ahead = std::stoi(count_result.substr(0, tab_pos));
            behind = std::stoi(count_result.substr(tab_pos + 1));
        } catch (const std::exception& e) {
            return -1;
        }
    }

    return 0;
}

int GitInfo::get_git_stash_count(const std::filesystem::path& repo_root) {
    std::vector<std::string> cmd = {"git", "-C", repo_root.string(), "stash", "list",
                                    "--pretty=oneline"};
    std::string result;
    int exit_code;

    if (safe_execute_git_command(cmd, result, exit_code) != 0 || exit_code != 0) {
        return 0;
    }

    int count = 0;
    std::istringstream stream(result);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty()) {
            ++count;
        }
    }
    return count;
}

bool GitInfo::get_git_has_staged_changes(const std::filesystem::path& repo_root) {
    std::vector<std::string> cmd = {"git", "-C", repo_root.string(), "diff", "--cached",
                                    "--quiet"};
    std::string result;
    int exit_code;

    if (safe_execute_git_command(cmd, result, exit_code) != 0) {
        return false;
    }

    if (exit_code == 0) {
        return false;
    } else if (exit_code == 1) {
        return true;
    } else {
        return false;
    }
}

int GitInfo::get_git_uncommitted_changes(const std::filesystem::path& repo_root) {
    std::vector<std::string> cmd = {"git", "-C", repo_root.string(), "status",
                                    "--porcelain"};
    std::string result;
    int exit_code;

    if (safe_execute_git_command(cmd, result, exit_code) != 0 || exit_code != 0) {
        return 0;
    }

    int count = 0;
    std::istringstream stream(result);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty()) {
            ++count;
        }
    }
    return count;
}
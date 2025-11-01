#include "git_info.h"

#include <sstream>
#include <unordered_map>
#include <vector>

#include "cjsh_filesystem.h"
#include "command_utils.h"

using prompt_modules::detail::command_execute;
using prompt_modules::detail::command_output_or;

namespace {

std::string trim_trailing_newline(std::string value) {
    if (!value.empty() && value.back() == '\n') {
        value.pop_back();
    }
    return value;
}

std::unordered_map<std::string, std::pair<std::string, std::chrono::steady_clock::time_point>>
    git_info_cache;
std::mutex git_info_cache_mutex;

}  // namespace

std::chrono::steady_clock::time_point last_git_status_check =
    std::chrono::steady_clock::now() - std::chrono::seconds(30);
std::string cached_git_dir;
std::string cached_status_symbols;
bool cached_is_clean_repo = true;
std::mutex git_status_mutex;
bool is_git_status_check_running = false;

std::string get_git_remote(const std::filesystem::path& repo_root) {
    std::vector<std::string> cmd = {"git", "-C", repo_root.string(), "remote", "get-url", "origin"};
    return command_output_or(cmd, "");
}

std::string get_git_tag(const std::filesystem::path& repo_root) {
    std::vector<std::string> cmd = {"git",      "-C",     repo_root.string(),
                                    "describe", "--tags", "--abbrev=0"};
    return command_output_or(cmd, "");
}

std::string get_git_last_commit(const std::filesystem::path& repo_root) {
    std::vector<std::string> cmd = {"git", "-C", repo_root.string(),
                                    "log", "-1", "--pretty=format:%h:%s"};
    return command_output_or(cmd, "");
}

std::string get_git_author(const std::filesystem::path& repo_root) {
    std::vector<std::string> cmd = {"git", "-C", repo_root.string(),
                                    "log", "-1", "--pretty=format:%an"};
    return command_output_or(cmd, "");
}

std::string get_git_branch(const std::filesystem::path& git_head_path) {
    try {
        auto read_result = cjsh_filesystem::read_file_content(git_head_path.string());
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

std::string get_git_status(const std::filesystem::path& repo_root) {
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
            auto command = command_execute(cmd);

            if (command.exit_code == 0) {
                if (!command.output.empty()) {
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

std::string get_local_path(const std::filesystem::path& repo_root) {
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

int get_git_ahead_behind(const std::filesystem::path& repo_root, int& ahead, int& behind) {
    ahead = 0;
    behind = 0;

    std::filesystem::path git_head_path = repo_root / ".git" / "HEAD";
    std::string branch = get_git_branch(git_head_path);
    if (branch.empty()) {
        return -1;
    }

    std::vector<std::string> upstream_cmd = {"git",       "-C",           repo_root.string(),
                                             "rev-parse", "--abbrev-ref", branch + "@{upstream}"};
    auto upstream = command_execute(upstream_cmd);
    if (upstream.exit_code != 0) {
        return -1;
    }

    std::string upstream_branch = trim_trailing_newline(upstream.output);

    if (upstream_branch.empty()) {
        return -1;
    }

    std::vector<std::string> count_cmd = {"git",
                                          "-C",
                                          repo_root.string(),
                                          "rev-list",
                                          "--left-right",
                                          "--count",
                                          branch + "..." + upstream_branch};
    auto count_result = command_execute(count_cmd);

    if (count_result.exit_code != 0) {
        return -1;
    }

    size_t tab_pos = count_result.output.find('\t');
    if (tab_pos != std::string::npos) {
        try {
            ahead = std::stoi(count_result.output.substr(0, tab_pos));
            behind = std::stoi(count_result.output.substr(tab_pos + 1));
        } catch (const std::exception& e) {
            return -1;
        }
    }

    return 0;
}

int get_git_stash_count(const std::filesystem::path& repo_root) {
    std::vector<std::string> cmd = {"git",   "-C",   repo_root.string(),
                                    "stash", "list", "--pretty=oneline"};
    auto command = command_execute(cmd);
    if (command.exit_code != 0) {
        return 0;
    }

    int count = 0;
    std::istringstream stream(command.output);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty()) {
            ++count;
        }
    }
    return count;
}

bool get_git_has_staged_changes(const std::filesystem::path& repo_root) {
    std::vector<std::string> cmd = {"git", "-C", repo_root.string(), "diff", "--cached", "--quiet"};
    auto command = command_execute(cmd);

    if (command.exit_code == 0) {
        return false;
    } else if (command.exit_code == 1) {
        return true;
    } else {
        return false;
    }
}

int get_git_uncommitted_changes(const std::filesystem::path& repo_root) {
    std::vector<std::string> cmd = {"git", "-C", repo_root.string(), "status", "--porcelain"};
    auto command = command_execute(cmd);
    if (command.exit_code != 0) {
        return 0;
    }

    int count = 0;
    std::istringstream stream(command.output);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty()) {
            ++count;
        }
    }
    return count;
}

void clear_git_info_cache() {
    {
        std::lock_guard<std::mutex> cache_lock(git_info_cache_mutex);
        git_info_cache.clear();
    }

    {
        std::lock_guard<std::mutex> status_lock(git_status_mutex);
        cached_git_dir.clear();
        cached_status_symbols.clear();
        cached_is_clean_repo = true;
        last_git_status_check = std::chrono::steady_clock::now() - std::chrono::seconds(60);
        is_git_status_check_running = false;
    }
}

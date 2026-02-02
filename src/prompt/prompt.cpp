/*
  prompt.cpp

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

#include "prompt.h"

#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "cjsh.h"
#include "flags.h"
#include "isocline.h"
#include "isocline/keycodes.h"
#include "job_control.h"
#include "shell.h"
#include "shell_env.h"
#include "token_constants.h"
#include "version_command.h"

namespace prompt {
namespace {

enum class PromptContext {
    Primary,
    Right,
    Secondary,
};

struct GitDirInfo {
    std::filesystem::path root;
    std::filesystem::path git_dir;
};

struct GitRepositoryContext {
    std::filesystem::path workdir;
    std::filesystem::path root;
    std::filesystem::path git_dir;
};

std::string get_env(const char* name, const char* fallback = nullptr) {
    const char* value = std::getenv(name);
    if (value != nullptr && value[0] != '\0') {
        return value;
    }
    if (fallback != nullptr) {
        return fallback;
    }
    return {};
}

std::string get_exit_status();

std::string get_shell_name() {
    std::string shell_name = get_env("0");
    if (shell_name.empty() && !flags::startup_args().empty()) {
        shell_name = flags::startup_args().front();
    }
    if (shell_name.empty()) {
        shell_name = "cjsh";
    }
    auto pos = shell_name.find_last_of('/');
    if (pos != std::string::npos) {
        shell_name = shell_name.substr(pos + 1);
    }
    return shell_name;
}

std::string get_username() {
    std::string username = get_env("USER");
    if (!username.empty()) {
        return username;
    }
    uid_t uid = geteuid();
    passwd* pw = getpwuid(uid);
    if (pw != nullptr && pw->pw_name != nullptr) {
        return pw->pw_name;
    }
    return {};
}

std::string get_hostname(bool full) {
    char buffer[256] = {0};
    if (gethostname(buffer, sizeof(buffer)) != 0) {
        return {};
    }
    std::string host(buffer);
    if (!full) {
        auto pos = host.find('.');
        if (pos != std::string::npos) {
            host.resize(pos);
        }
    }
    return host;
}

bool terminal_supports_color() {
    if (isatty(STDOUT_FILENO) == 0) {
        return false;
    }

    if (std::getenv("NO_COLOR") != nullptr) {
        return false;
    }

    const char* colorterm = std::getenv("COLORTERM");
    if (colorterm != nullptr) {
        std::string colorterm_lower;
        for (const char* p = colorterm; *p != '\0'; ++p) {
            colorterm_lower.push_back(
                static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
        }
        if (colorterm_lower.find("nocolor") != std::string::npos ||
            colorterm_lower.find("monochrome") != std::string::npos) {
            return false;
        }
        if (!colorterm_lower.empty()) {
            return true;
        }
    }

    const char* term = std::getenv("TERM");
    if (term == nullptr) {
        return true;
    }

    std::string term_lower;
    for (const char* p = term; *p != '\0'; ++p) {
        term_lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
    }

    const bool unsupported = term_lower.find("dumb") != std::string::npos ||
                             term_lower.find("cons25") != std::string::npos ||
                             term_lower.find("emacs") != std::string::npos ||
                             term_lower.find("nocolor") != std::string::npos ||
                             term_lower.find("monochrome") != std::string::npos;

    return !unsupported;
}

std::string build_default_terminal_title() {
    std::string shell_value = "cjsh";

    std::string pwd_value = get_env("PWD");
    if (pwd_value.empty()) {
        std::error_code ec;
        auto cwd = std::filesystem::current_path(ec);
        if (!ec) {
            pwd_value = cwd.string();
        }
    }

    if (shell_value.empty() && pwd_value.empty()) {
        return {};
    }

    if (shell_value.empty()) {
        return pwd_value;
    }

    if (pwd_value.empty()) {
        return shell_value;
    }

    std::string title;
    title.reserve(shell_value.size() + 3 + pwd_value.size());
    title.append(shell_value);
    title.append(" - ");
    title.append(pwd_value);
    return title;
}

std::string format_time(const char* fmt) {
    std::time_t now = std::time(nullptr);
    std::tm tm_now{};
    (void)localtime_r(&now, &tm_now);

    char buffer[256];
    size_t written = std::strftime(buffer, sizeof(buffer), fmt, &tm_now);
    if (written == 0) {
        return {};
    }
    return std::string(buffer, written);
}

std::string get_cwd(bool abbreviate_home, bool basename_only) {
    std::error_code ec;
    std::filesystem::path cwd = std::filesystem::current_path(ec);
    if (ec) {
        return {};
    }
    std::string path = cwd.string();

    std::string home = get_env("HOME");
    if (abbreviate_home && !home.empty()) {
        if (path == home) {
            path = "~";
        } else if (path.rfind(home + "/", 0) == 0) {
            path = "~" + path.substr(home.size());
        }
    }

    if (!basename_only) {
        return path;
    }

    if (path == "~") {
        return path;
    }

    std::filesystem::path path_obj(path);
    std::string base = path_obj.filename().string();
    if (base.empty()) {
        base = path_obj.root_path().string();
    }
    if (abbreviate_home && !home.empty() && path.rfind(home + "/", 0) == 0) {
        if (path == home) {
            return "~";
        }
        base = path.substr(path.find_last_of('/') + 1);
    }
    return base;
}

std::string get_terminal_name() {
    const char* tty = ttyname(STDIN_FILENO);
    if (tty == nullptr) {
        return {};
    }
    std::string name(tty);
    auto pos = name.find_last_of('/');
    if (pos != std::string::npos) {
        name = name.substr(pos + 1);
    }
    return name;
}

std::string trim_copy(const std::string& input) {
    const auto first = input.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = input.find_last_not_of(" \t\r\n");
    return input.substr(first, (last - first) + 1);
}

std::optional<std::filesystem::path> git_dir_from_worktree_file(
    const std::filesystem::path& candidate, const std::filesystem::path& base) {
    std::ifstream file(candidate);
    if (!file.is_open()) {
        return std::nullopt;
    }
    std::string line;
    std::getline(file, line);
    line = trim_copy(line);
    constexpr std::string_view kPrefix = "gitdir:";
    if (line.rfind(kPrefix, 0) != 0) {
        return std::nullopt;
    }
    std::string path_str = trim_copy(line.substr(kPrefix.size()));
    if (path_str.empty()) {
        return std::nullopt;
    }
    std::filesystem::path gitdir(path_str);
    if (gitdir.is_relative()) {
        return (base / gitdir).lexically_normal();
    }
    return gitdir;
}

std::optional<GitDirInfo> locate_git_directory(const std::filesystem::path& dir) {
    std::error_code ec;
    std::filesystem::path candidate = dir / ".git";
    if (!std::filesystem::exists(candidate, ec)) {
        return std::nullopt;
    }
    if (std::filesystem::is_directory(candidate, ec)) {
        return GitDirInfo{dir, candidate};
    }
    if (!std::filesystem::is_regular_file(candidate, ec)) {
        return std::nullopt;
    }
    if (auto gitdir = git_dir_from_worktree_file(candidate, dir)) {
        return GitDirInfo{dir, *gitdir};
    }
    return std::nullopt;
}

std::optional<GitDirInfo> find_git_directory(const std::filesystem::path& start) {
    std::filesystem::path current = start;
    while (true) {
        if (auto git_info = locate_git_directory(current)) {
            return git_info;
        }
        if (!current.has_parent_path()) {
            break;
        }
        auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return std::nullopt;
}

std::string read_git_head(const std::filesystem::path& git_dir) {
    std::ifstream head_file(git_dir / "HEAD");
    if (!head_file.is_open()) {
        return {};
    }
    std::string line;
    std::getline(head_file, line);
    line = trim_copy(line);
    if (line.empty()) {
        return {};
    }
    constexpr std::string_view kRefPrefix = "ref:";
    if (line.rfind(kRefPrefix, 0) == 0) {
        std::string ref = trim_copy(line.substr(kRefPrefix.size()));
        auto slash = ref.find_last_of('/');
        if (slash != std::string::npos) {
            return ref.substr(slash + 1);
        }
        return ref;
    }
    if (line.size() > 7) {
        return line.substr(0, 7);
    }
    return line;
}

bool git_has_changes(const std::filesystem::path& workdir) {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        return false;
    }

    std::string workdir_str = workdir.empty() ? std::string(".") : workdir.string();
    const char* workdir_cstr = workdir_str.c_str();

    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    if (pid == 0) {
        // Child: redirect stdout/stderr to the pipe and exec git directly.
        if (dup2(pipefd[1], STDOUT_FILENO) == -1 || dup2(pipefd[1], STDERR_FILENO) == -1) {
            _exit(127);
        }
        close(pipefd[0]);
        close(pipefd[1]);

        const char* const argv[] = {"git",    "-C",          workdir_cstr,
                                    "status", "--porcelain", "--untracked-files=normal",
                                    nullptr};
        execvp("git", const_cast<char* const*>(argv));
        _exit(127);
    }

    close(pipefd[1]);
    char ch;
    ssize_t bytes_read = read(pipefd[0], &ch, 1);
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    return bytes_read > 0;
}

std::optional<GitRepositoryContext> detect_git_context() {
    std::error_code ec;
    std::filesystem::path cwd = std::filesystem::current_path(ec);
    if (ec) {
        return std::nullopt;
    }
    auto git_info = find_git_directory(cwd);
    if (!git_info) {
        return std::nullopt;
    }
    GitRepositoryContext context;
    context.workdir = cwd;
    context.root = git_info->root;
    context.git_dir = git_info->git_dir;
    return context;
}

std::string build_git_segment(const GitRepositoryContext& ctx) {
    std::string branch = read_git_head(ctx.git_dir);
    if (branch.empty()) {
        return {};
    }
    std::string segment;
    segment.reserve(branch.size() + 32);
    segment += "[b color=ansi-blue]git:([/]";
    segment += "[color=#ff6b6b]" + branch + "[/]";
    segment += "[color=ansi-blue])[/]";
    if (git_has_changes(ctx.workdir)) {
        segment += " [color=#ffd166]✗[/color]";
    }
    segment.push_back(' ');
    return segment;
}

std::string render_git_segment_now() {
    auto context = detect_git_context();
    if (!context) {
        return {};
    }
    return build_git_segment(*context);
}

class AsyncGitPromptManager {
   public:
    void begin_render();
    void finalize_render(const std::string& prompt_snapshot);
    void append_segment(std::string& builder);
    std::optional<std::string> consume_ready_prompt();

   private:
    void start_worker_locked(size_t request_id, const GitRepositoryContext& context);
    void handle_worker_result(size_t request_id, std::string segment);

    std::mutex mutex_;
    std::condition_variable cv_;
    bool async_enabled_cached_ = false;
    bool worker_running_ = false;
    size_t worker_request_id_ = 0;
    size_t request_counter_ = 0;
    size_t active_request_id_ = 0;
    size_t pending_prompt_request_id_ = 0;
    size_t result_request_id_ = 0;
    bool refresh_pending_ = false;
    bool awaiting_async_result_ = false;
    bool prompt_snapshot_ready_ = false;
    bool notify_when_snapshot_ready_ = false;
    bool result_ready_ = false;
    std::string pending_prompt_;
    std::vector<size_t> placeholder_offsets_;
    std::string pending_segment_;
};

constexpr std::chrono::milliseconds kGitInlineBudget(35);

void AsyncGitPromptManager::begin_render() {
    std::lock_guard<std::mutex> lock(mutex_);
    async_enabled_cached_ = config::interactive_mode;
    if (!async_enabled_cached_) {
        active_request_id_ = 0;
        placeholder_offsets_.clear();
        pending_prompt_.clear();
        refresh_pending_ = false;
        awaiting_async_result_ = false;
        prompt_snapshot_ready_ = false;
        notify_when_snapshot_ready_ = false;
        result_ready_ = false;
        return;
    }
    ++request_counter_;
    active_request_id_ = request_counter_;
    placeholder_offsets_.clear();
    pending_prompt_.clear();
    refresh_pending_ = false;
    awaiting_async_result_ = false;
    prompt_snapshot_ready_ = false;
    notify_when_snapshot_ready_ = false;
    result_ready_ = false;
}

void AsyncGitPromptManager::append_segment(std::string& builder) {
    if (!async_enabled_cached_) {
        builder += render_git_segment_now();
        return;
    }

    auto context = detect_git_context();
    if (!context) {
        std::lock_guard<std::mutex> lock(mutex_);
        refresh_pending_ = false;
        awaiting_async_result_ = false;
        placeholder_offsets_.clear();
        pending_prompt_.clear();
        result_ready_ = false;
        pending_segment_.clear();
        prompt_snapshot_ready_ = false;
        notify_when_snapshot_ready_ = false;
        return;
    }

    std::unique_lock<std::mutex> lock(mutex_);

    if (!worker_running_ || worker_request_id_ != active_request_id_) {
        start_worker_locked(active_request_id_, *context);
    }

    if (cv_.wait_for(lock, kGitInlineBudget,
                     [&] { return result_ready_ && result_request_id_ == active_request_id_; })) {
        builder += pending_segment_;
        refresh_pending_ = false;
        awaiting_async_result_ = false;
        return;
    }

    awaiting_async_result_ = true;
    refresh_pending_ = true;
    placeholder_offsets_.push_back(builder.size());
}

void AsyncGitPromptManager::finalize_render(const std::string& prompt_snapshot) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!async_enabled_cached_ || !awaiting_async_result_ || placeholder_offsets_.empty()) {
        pending_prompt_.clear();
        prompt_snapshot_ready_ = false;
        notify_when_snapshot_ready_ = false;
        return;
    }
    pending_prompt_ = prompt_snapshot;
    prompt_snapshot_ready_ = true;
    pending_prompt_request_id_ = active_request_id_;
    bool should_notify = notify_when_snapshot_ready_ && result_ready_ &&
                         result_request_id_ == pending_prompt_request_id_;
    if (should_notify) {
        notify_when_snapshot_ready_ = false;
    }
    lock.unlock();
    if (should_notify) {
        ic_push_key_event(IC_KEY_EVENT_PROMPT_REFRESH);
    }
}

std::optional<std::string> AsyncGitPromptManager::consume_ready_prompt() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!async_enabled_cached_ || !refresh_pending_ || !result_ready_ || !prompt_snapshot_ready_ ||
        pending_prompt_request_id_ != result_request_id_ || placeholder_offsets_.empty()) {
        return std::nullopt;
    }
    std::string updated = pending_prompt_;
    size_t inserted = 0;
    for (size_t offset : placeholder_offsets_) {
        updated.insert(offset + inserted, pending_segment_);
        inserted += pending_segment_.size();
    }
    refresh_pending_ = false;
    awaiting_async_result_ = false;
    prompt_snapshot_ready_ = false;
    placeholder_offsets_.clear();
    pending_prompt_.clear();
    result_ready_ = false;
    notify_when_snapshot_ready_ = false;
    lock.unlock();
    return updated;
}

void AsyncGitPromptManager::start_worker_locked(size_t request_id,
                                                const GitRepositoryContext& context) {
    worker_running_ = true;
    worker_request_id_ = request_id;
    std::thread worker([this, request_id, context]() {
        std::string segment = build_git_segment(context);
        handle_worker_result(request_id, std::move(segment));
    });
    worker.detach();
}

void AsyncGitPromptManager::handle_worker_result(size_t request_id, std::string segment) {
    bool should_notify = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        worker_running_ = false;
        if (!async_enabled_cached_ || request_id != active_request_id_) {
            return;
        }
        pending_segment_ = std::move(segment);
        result_ready_ = true;
        result_request_id_ = request_id;
        if (refresh_pending_) {
            if (prompt_snapshot_ready_) {
                should_notify = true;
            } else {
                notify_when_snapshot_ready_ = true;
            }
        }
    }
    cv_.notify_all();
    if (should_notify) {
        ic_push_key_event(IC_KEY_EVENT_PROMPT_REFRESH);
    }
}

AsyncGitPromptManager& git_prompt_async_manager() {
    static AsyncGitPromptManager manager;
    return manager;
}

void append_git_segment(std::string& builder, PromptContext context) {
    if (context == PromptContext::Primary) {
        git_prompt_async_manager().append_segment(builder);
    } else {
        builder += render_git_segment_now();
    }
}

int exit_status_value() {
    std::string status = get_exit_status();
    try {
        return std::stoi(status);
    } catch (...) {
        return 0;
    }
}

std::string status_symbol() {
    static const char* arrow = u8"\u279C";
    if (exit_status_value() == 0) {
        return std::string("[b color=#2dd881]") + arrow + "[/]";
    }
    return std::string("[b color=#ff6b6b]") + arrow + "[/]";
}

std::string get_version_short() {
    std::string version = get_version();
    auto pos = version.find(' ');
    if (pos != std::string::npos) {
        version = version.substr(0, pos);
    }
    pos = version.find_last_of('.');
    if (pos != std::string::npos) {
        return version.substr(0, pos);
    }
    return version;
}

std::string get_version_full() {
    return get_version();
}

std::string get_exit_status() {
    return get_env("?");
}

std::string get_job_count() {
    size_t count = JobManager::instance().get_all_jobs().size();
    return std::to_string(count);
}

char prompt_dollar() {
    if (geteuid() == 0) {
        return '#';
    }
    return '$';
}

char to_ascii(std::uint8_t value) {
    return static_cast<char>(value);
}

std::string expand_prompt_string(const std::string& templ, PromptContext context) {
    std::string result;
    result.reserve(templ.size() + 16);

    for (size_t i = 0; i < templ.size(); ++i) {
        char ch = templ[i];
        if (ch != '\\') {
            result.push_back(ch);
            continue;
        }

        ++i;
        if (i >= templ.size()) {
            result.push_back('\\');
            break;
        }

        char code = templ[i];
        switch (code) {
            case 'a':
                result.push_back('\a');
                break;
            case 'd':
                result += format_time("%a %b %e");
                break;
            case 'D': {
                if (i + 1 < templ.size() && templ[i + 1] == '{') {
                    size_t closing = templ.find('}', i + 2);
                    if (closing != std::string::npos) {
                        std::string fmt = templ.substr(i + 2, closing - (i + 2));
                        result += format_time(fmt.c_str());
                        i = closing;
                        break;
                    }
                }
                result.push_back('D');
                break;
            }
            case 'e':
            case 'E':
                result.push_back('\033');
                break;
            case 'h':
                result += get_hostname(false);
                break;
            case 'H':
                result += get_hostname(true);
                break;
            case 'j':
                result += get_job_count();
                break;
            case 'l':
                result += get_terminal_name();
                break;
            case 'n':
                result.push_back('\n');
                break;
            case 'r':
                result.push_back('\r');
                break;
            case 's':
                result += get_shell_name();
                break;
            case 't':
                result += format_time("%H:%M:%S");
                break;
            case 'T':
                result += format_time("%I:%M:%S");
                break;
            case '@':
                result += format_time("%I:%M %p");
                break;
            case 'A':
                result += format_time("%H:%M");
                break;
            case 'u':
                result += get_username();
                break;
            case 'v':
                result += get_version_short();
                break;
            case 'V':
                result += get_version_full();
                break;
            case 'w':
                result += get_cwd(true, false);
                break;
            case 'W':
                result += get_cwd(true, true);
                break;
            case 'S':
                result += status_symbol();
                break;
            case 'g':
                append_git_segment(result, context);
                break;
            case '$':
                result.push_back(prompt_dollar());
                break;
            case '?':
                result += get_exit_status();
                break;
            case '\\':
                result.push_back('\\');
                break;
            case '[':
            case ']':
                break;
            default: {
                if (code >= '0' && code <= '7') {
                    int value = code - '0';
                    int digits = 1;
                    while (digits < 3 && (i + 1) < templ.size()) {
                        char next = templ[i + 1];
                        if (next < '0' || next > '7') {
                            break;
                        }
                        value = (value * 8) + (next - '0');
                        ++i;
                        ++digits;
                    }
                    result.push_back(to_ascii(static_cast<std::uint8_t>(value & 0xFF)));
                } else {
                    result.push_back(code);
                }
                break;
            }
        }
    }

    return result;
}

std::string get_ps(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    if (value != nullptr) {
        return value;
    }
    return fallback;
}

}  // namespace

std::string default_primary_prompt_template() {
    return "\\S  [color=#5fd7ff]\\W[/color] \\g";
}

std::string default_right_prompt_template() {
    return "[ic-hint]\\A[/ic-hint]";
}

std::string render_primary_prompt() {
    std::string ps1 = get_ps("PS1", default_primary_prompt_template());
    git_prompt_async_manager().begin_render();
    std::string prompt_text = expand_prompt_string(ps1, PromptContext::Primary);
    git_prompt_async_manager().finalize_render(prompt_text);
    return prompt_text;
}

std::string render_right_prompt() {
    if (const char* rprompt = std::getenv("RPROMPT"); rprompt != nullptr) {
        return expand_prompt_string(rprompt, PromptContext::Right);
    }

    std::string rps1 = get_ps("RPS1", default_right_prompt_template());
    return expand_prompt_string(rps1, PromptContext::Right);
}

std::string render_secondary_prompt() {
    const char* ps2 = std::getenv("PS2");
    if (ps2 == nullptr || ps2[0] == '\0') {
        return {};
    }
    return expand_prompt_string(ps2, PromptContext::Secondary);
}

void execute_prompt_command() {
    if (!g_shell) {
        return;
    }
    std::string command = get_env("PROMPT_COMMAND");
    if (command.empty()) {
        return;
    }
    g_shell->execute(command);
}

void initialize_colors() {
    if (!config::colors_enabled) {
        ic_enable_color(false);
        return;
    }

    if (!terminal_supports_color()) {
        config::colors_enabled = false;
        ic_enable_color(false);
        return;
    }

    ic_enable_color(true);

    if (!config::syntax_highlighting_enabled) {
        return;
    }

    for (const auto& pair : token_constants::default_styles()) {
        std::string style_name = pair.first;
        if (style_name.rfind("ic-", 0) != 0) {
            style_name = "cjsh-";
            style_name += pair.first;
        }
        ic_style_def(style_name.c_str(), pair.second.c_str());
        ic_style_def("ic-prompt", "white");
    }
}

void apply_terminal_window_title() {
    if (!config::interactive_mode) {
        return;
    }

    std::string title;
    if (const char* twinprompt = std::getenv("TWINPROMPT");
        twinprompt != nullptr && twinprompt[0] != '\0') {
        title.assign(twinprompt);
    } else {
        title = build_default_terminal_title();
        if (!title.empty()) {
            setenv("TWINPROMPT", title.c_str(), 1);
        }
    }

    if (title.empty()) {
        return;
    }

    std::printf("\033]0;%s\007", title.c_str());
    (void)std::fflush(stdout);
}

bool handle_async_prompt_refresh() {
    auto updated_prompt = git_prompt_async_manager().consume_ready_prompt();
    if (!updated_prompt) {
        return false;
    }
    return ic_current_loop_reset(nullptr, updated_prompt->c_str(), nullptr);
}
}  // namespace prompt

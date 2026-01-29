#include "cjsh_filesystem.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cjsh.h"
#include "error_out.h"
#include "parser/parser.h"
#include "shell.h"

#ifdef __linux__
#include <linux/limits.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace cjsh_filesystem {

namespace {
enum class CacheUsage : std::uint8_t;
std::string resolve_command_with_cache(const std::string& name, CacheUsage usage);
}  // namespace

const std::filesystem::path& g_user_home_path() {
    static const std::filesystem::path path = []() {
        const char* home = std::getenv("HOME");
        if (!home || home[0] == '\0') {
            print_error({ErrorType::UNKNOWN_ERROR,
                         ErrorSeverity::WARNING,
                         "filesystem",
                         "HOME environment variable not set or empty. Using /tmp as fallback.",
                         {}});
            return std::filesystem::path("/tmp");
        }
        return std::filesystem::path(home);
    }();
    return path;
}

const std::filesystem::path& g_cjsh_config_path() {
    static const std::filesystem::path path = g_user_home_path() / ".config" / "cjsh";
    return path;
}

const std::filesystem::path& g_cjsh_cache_path() {
    static const std::filesystem::path path = g_user_home_path() / ".cache" / "cjsh";
    return path;
}

const std::filesystem::path& g_cjsh_profile_path() {
    static const std::filesystem::path path = g_user_home_path() / ".cjprofile";
    return path;
}

const std::filesystem::path& g_cjsh_source_path() {
    static const std::filesystem::path path = g_user_home_path() / ".cjshrc";
    return path;
}

const std::filesystem::path& g_cjsh_logout_path() {
    static const std::filesystem::path path = g_user_home_path() / ".cjsh_logout";
    return path;
}

const std::filesystem::path& g_cjsh_profile_alt_path() {
    static const std::filesystem::path path = g_cjsh_config_path() / ".cjprofile";
    return path;
}

const std::filesystem::path& g_cjsh_source_alt_path() {
    static const std::filesystem::path path = g_cjsh_config_path() / ".cjshrc";
    return path;
}

const std::filesystem::path& g_cjsh_logout_alt_path() {
    static const std::filesystem::path path = g_cjsh_config_path() / ".cjsh_logout";
    return path;
}

const std::filesystem::path& g_cjsh_history_path() {
    static const std::filesystem::path path = g_cjsh_cache_path() / "history.txt";
    return path;
}

const std::filesystem::path& g_cjsh_first_boot_path() {
    static const std::filesystem::path path = g_cjsh_cache_path() / ".first_boot";
    return path;
}

const std::filesystem::path& g_cjsh_generated_completions_path() {
    static const std::filesystem::path path = g_cjsh_cache_path() / "generated_completions";
    return path;
}

namespace {
std::string describe_errno(int err) {
    return std::system_category().message(err);
}

bool path_is_executable(const std::filesystem::path& candidate) {
    std::error_code ec;

    if (!std::filesystem::exists(candidate, ec) || ec) {
        return false;
    }

    if (std::filesystem::is_directory(candidate, ec) || ec) {
        return false;
    }

    return ::access(candidate.c_str(), X_OK) == 0;
}
}  // namespace

namespace {
template <typename Callback>
bool for_each_path_segment(std::string_view path_str, Callback&& callback) {
    size_t start = 0;
    while (start < path_str.size()) {
        size_t pos = path_str.find(':', start);
        size_t end = (pos != std::string::npos) ? pos : path_str.size();
        if (callback(path_str.substr(start, end - start))) {
            return true;
        }
        start = (pos != std::string::npos) ? pos + 1 : path_str.size();
    }
    return false;
}
}  // namespace

namespace {

enum class CacheUsage : std::uint8_t {
    Query,
    Execution,
    Manual
};

struct CachedExecutable {
    std::string path;
    std::uint64_t hits{0};
    std::time_t last_used{0};
    bool manually_added{false};
};

std::mutex g_path_hash_mutex;
std::unordered_map<std::string, CachedExecutable> g_path_hash_entries;
std::string g_path_snapshot;
bool g_path_hash_seeded = false;

void seed_path_hash_locked(const std::string& path_value) {
    if (g_path_hash_seeded || path_value.empty()) {
        return;
    }

    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    for_each_path_segment(path_value, [&](std::string_view raw_segment) {
        if (raw_segment.empty()) {
            return false;
        }

        std::filesystem::path directory_path(raw_segment);
        std::error_code ec;

        if (!std::filesystem::exists(directory_path, ec) || ec) {
            ec.clear();
            return false;
        }

        if (!std::filesystem::is_directory(directory_path, ec) || ec) {
            ec.clear();
            return false;
        }

        std::filesystem::directory_iterator it(
            directory_path, std::filesystem::directory_options::skip_permission_denied, ec);
        if (ec) {
            ec.clear();
            return false;
        }

        for (; it != std::filesystem::directory_iterator(); it.increment(ec)) {
            if (ec) {
                ec.clear();
                break;
            }

            const auto& entry = *it;
            auto status = entry.status(ec);
            if (ec) {
                ec.clear();
                continue;
            }

            if (!std::filesystem::is_regular_file(status)) {
                continue;
            }

            auto perms = status.permissions();
            constexpr auto exec_mask = std::filesystem::perms::owner_exec |
                                       std::filesystem::perms::group_exec |
                                       std::filesystem::perms::others_exec;

            if ((perms & exec_mask) == std::filesystem::perms::none) {
                continue;
            }

            std::string exec_name = entry.path().filename().string();
            if (g_path_hash_entries.find(exec_name) != g_path_hash_entries.end()) {
                continue;
            }

            CachedExecutable cache_entry;
            cache_entry.path = entry.path().string();
            cache_entry.hits = 0;
            cache_entry.last_used = now;
            cache_entry.manually_added = false;
            g_path_hash_entries.emplace(std::move(exec_name), std::move(cache_entry));
        }

        return false;
    });

    g_path_hash_seeded = true;
}

std::string current_path_env_value() {
    const char* path_env = std::getenv("PATH");
    if (!path_env) {
        return {};
    }
    return path_env;
}

void ensure_path_snapshot_locked(const std::string& current_path) {
    if (current_path != g_path_snapshot) {
        g_path_hash_entries.clear();
        g_path_snapshot = current_path;
        g_path_hash_seeded = false;
    }
}

bool entry_is_valid(const CachedExecutable& entry) {
    return path_is_executable(entry.path);
}

bool is_cacheable_command(const std::string& name) {
    return !name.empty() && name.find('/') == std::string::npos;
}

std::string resolve_explicit_command(const std::string& name) {
    if (name.empty()) {
        return {};
    }

    std::filesystem::path candidate(name);
    if (!candidate.is_absolute()) {
        candidate = std::filesystem::path(safe_current_directory()) / candidate;
    }
    candidate = candidate.lexically_normal();

    return path_is_executable(candidate) ? candidate.string() : std::string{};
}

std::string scan_path_for_command(const std::string& name, std::string_view path_value) {
    std::string resolved;
    for_each_path_segment(path_value, [&](std::string_view raw_segment) {
        if (raw_segment.empty()) {
            return false;
        }
        std::filesystem::path directory_path(raw_segment);
        std::filesystem::path candidate = directory_path / name;
        if (path_is_executable(candidate)) {
            resolved = candidate.string();
            return true;
        }
        return false;
    });
    return resolved;
}

std::string resolve_command_with_cache(const std::string& name, CacheUsage usage) {
    if (!is_cacheable_command(name)) {
        return resolve_explicit_command(name);
    }

    std::string current_path = current_path_env_value();
    std::lock_guard<std::mutex> lock(g_path_hash_mutex);
    ensure_path_snapshot_locked(current_path);

    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    auto it = g_path_hash_entries.find(name);
    if (it != g_path_hash_entries.end()) {
        if (entry_is_valid(it->second)) {
            if (usage == CacheUsage::Execution) {
                it->second.hits++;
            }
            if (usage == CacheUsage::Manual) {
                it->second.manually_added = true;
            }
            it->second.last_used = now;
            return it->second.path;
        }
        g_path_hash_entries.erase(it);
    }

    if (current_path.empty()) {
        return {};
    }

    std::string resolved = scan_path_for_command(name, current_path);
    if (!resolved.empty()) {
        CachedExecutable entry;
        entry.path = resolved;
        entry.hits = (usage == CacheUsage::Execution) ? 1 : 0;
        entry.manually_added = (usage == CacheUsage::Manual);
        entry.last_used = now;
        g_path_hash_entries[name] = std::move(entry);
    }

    return resolved;
}

}  // namespace

std::filesystem::path g_cjsh_path;

std::string safe_current_directory() {
    char* cwd = ::getcwd(nullptr, 0);
    if (cwd != nullptr) {
        std::string result(cwd);
        std::free(cwd);
        return result;
    }

    if (const char* env_pwd = std::getenv("PWD"); env_pwd != nullptr && env_pwd[0] != '\0') {
        return env_pwd;
    }

    const auto& home_path = g_user_home_path();
    if (!home_path.empty()) {
        return home_path.string();
    }

    return "/";
}

Result<int> safe_open(const std::string& path, int flags, mode_t mode) {
    int fd = ::open(path.c_str(), flags, mode);
    if (fd == -1) {
        return Result<int>::error("Failed to open file '" + path + "': " + describe_errno(errno));
    }
    return Result<int>::ok(fd);
}

Result<void> safe_dup2(int oldfd, int newfd) {
    if (::dup2(oldfd, newfd) == -1) {
        return Result<void>::error("Failed to duplicate file descriptor " + std::to_string(oldfd) +
                                   " to " + std::to_string(newfd) + ": " + describe_errno(errno));
    }
    return Result<void>::ok();
}

void safe_close(int fd) {
    if (fd >= 0) {
        ::close(fd);
    }
}

Result<void> redirect_fd(const std::string& file, int target_fd, int flags) {
    auto open_result = safe_open(file, flags, 0644);
    if (open_result.is_error()) {
        return Result<void>::error(open_result.error());
    }

    int file_fd = open_result.value();

    if (file_fd != target_fd) {
        auto dup_result = safe_dup2(file_fd, target_fd);
        safe_close(file_fd);
        if (dup_result.is_error()) {
            return dup_result;
        }
    }

    return Result<void>::ok();
}

Result<void> set_close_on_exec(int fd) {
    int flags = ::fcntl(fd, F_GETFD);
    if (flags == -1) {
        return Result<void>::error("failed to get file descriptor flags for fd " +
                                   std::to_string(fd) + ": " + describe_errno(errno));
    }

    if (::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
        return Result<void>::error("failed to set close-on-exec on fd " + std::to_string(fd) +
                                   ": " + describe_errno(errno));
    }

    return Result<void>::ok();
}

Result<void> create_pipe_cloexec(int pipe_fds[2]) {
    if (::pipe(pipe_fds) == -1) {
        return Result<void>::error("failed to create pipe: " + describe_errno(errno));
    }

    auto read_result = set_close_on_exec(pipe_fds[0]);
    if (read_result.is_error()) {
        safe_close(pipe_fds[0]);
        safe_close(pipe_fds[1]);
        return Result<void>::error("failed to secure pipe read end: " + read_result.error());
    }

    auto write_result = set_close_on_exec(pipe_fds[1]);
    if (write_result.is_error()) {
        safe_close(pipe_fds[0]);
        safe_close(pipe_fds[1]);
        return Result<void>::error("failed to secure pipe write end: " + write_result.error());
    }

    return Result<void>::ok();
}

Result<void> duplicate_pipe_read_end_to_fd(int (&pipe_fds)[2], int target_fd) {
    safe_close(pipe_fds[1]);

    auto dup_result = safe_dup2(pipe_fds[0], target_fd);
    safe_close(pipe_fds[0]);
    if (dup_result.is_error()) {
        return Result<void>::error(dup_result.error());
    }

    return Result<void>::ok();
}

void close_pipe(int pipe_fds[2]) {
    safe_close(pipe_fds[0]);
    safe_close(pipe_fds[1]);
}

namespace {
Result<void> write_content_with_permissions(const std::string& path, std::string_view content,
                                            bool enforce_secure_permissions) {
    auto open_result = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (open_result.is_error()) {
        return Result<void>::error(open_result.error());
    }

    int fd = open_result.value();

    if (enforce_secure_permissions) {
        if (::fchmod(fd, S_IRUSR | S_IWUSR) == -1) {
            std::string error_message =
                "Failed to set secure permissions on '" + path + "': " + describe_errno(errno);
            safe_close(fd);
            return Result<void>::error(error_message);
        }
    }

    auto write_result = write_all(fd, content);
    safe_close(fd);

    if (write_result.is_error()) {
        return Result<void>::error(write_result.error());
    }

    return Result<void>::ok();
}
}  // namespace

Result<void> write_file_content(const std::string& path, const std::string& content) {
    return write_content_with_permissions(path, std::string_view{content}, true);
}

Result<void> write_all(int fd, std::string_view data) {
    size_t total_written = 0;
    while (total_written < data.size()) {
        size_t remaining = data.size() - total_written;
#ifdef SSIZE_MAX
        remaining = std::min(remaining, static_cast<size_t>(SSIZE_MAX));
#endif
        ssize_t written = ::write(fd, data.data() + total_written, remaining);
        if (written == -1) {
            if (errno == EINTR
#ifdef EAGAIN
                || errno == EAGAIN
#endif
#ifdef EWOULDBLOCK
                || errno == EWOULDBLOCK
#endif
            ) {
                continue;
            }

            if (errno == EPIPE) {
                return Result<void>::error("Broken pipe (EPIPE)");
            }
            return Result<void>::error("Failed to write to file descriptor " + std::to_string(fd) +
                                       ": " + describe_errno(errno));
        }
        if (written == 0) {
            return Result<void>::error("Write to file descriptor " + std::to_string(fd) +
                                       " returned zero bytes");
        }
        total_written += static_cast<size_t>(written);
    }
    return Result<void>::ok();
}

bool error_indicates_broken_pipe(std::string_view message) {
    return (message.find("Broken pipe") != std::string::npos) ||
           (message.find("EPIPE") != std::string::npos);
}

std::optional<HereStringError> setup_here_string_stdin(const std::string& here_string) {
    int here_pipe[2];
    auto pipe_result = create_pipe_cloexec(here_pipe);
    if (pipe_result.is_error()) {
        return HereStringError{HereStringErrorType::Pipe, pipe_result.error()};
    }

    std::string content = here_string;
    if (g_shell && (g_shell->get_parser() != nullptr)) {
        g_shell->get_parser()->expand_env_vars(content);
    }
    content.push_back('\n');

    auto write_result = write_all(here_pipe[1], std::string_view{content});
    std::fill(content.begin(), content.end(), '\0');
    if (write_result.is_error() && !error_indicates_broken_pipe(write_result.error())) {
        close_pipe(here_pipe);
        return HereStringError{HereStringErrorType::Write, write_result.error()};
    }

    auto dup_result = duplicate_pipe_read_end_to_fd(here_pipe, STDIN_FILENO);
    if (dup_result.is_error()) {
        return HereStringError{HereStringErrorType::Dup, dup_result.error()};
    }

    return std::nullopt;
}

bool should_noclobber_prevent_overwrite(const std::string& filename, bool force_overwrite) {
    if (force_overwrite) {
        return false;
    }

    if (!g_shell || !g_shell->get_shell_option("noclobber")) {
        return false;
    }

    struct stat file_stat{};
    return stat(filename.c_str(), &file_stat) == 0;
}

bool command_exists(const std::string& command_path) {
    if (command_path.empty()) {
        return false;
    }
    if (command_path.find('/') != std::string::npos) {
        return ::access(command_path.c_str(), F_OK) == 0;
    }
    return !resolve_command_with_cache(command_path, CacheUsage::Query).empty();
}

bool resolves_to_executable(const std::string& name, const std::string& cwd) {
    if (name.empty()) {
        return false;
    }

    std::filesystem::path candidate(name);

    if (name.find('/') != std::string::npos) {
        if (!candidate.is_absolute()) {
            candidate = std::filesystem::path(cwd) / candidate;
        }

        return path_is_executable(candidate);
    }

    return !resolve_command_with_cache(name, CacheUsage::Query).empty();
}

bool path_is_directory_candidate(const std::string& value, const std::string& cwd) {
    if (value.empty()) {
        return false;
    }

    std::filesystem::path candidate(value);
    std::error_code ec;

    if (!candidate.is_absolute()) {
        candidate = std::filesystem::path(cwd) / candidate;
    }

    return std::filesystem::exists(candidate, ec) && !ec &&
           std::filesystem::is_directory(candidate, ec) && !ec;
}

Result<std::string> read_file_content(const std::string& path) {
    auto open_result = safe_open(path, O_RDONLY);
    if (open_result.is_error()) {
        return Result<std::string>::error(open_result.error());
    }

    int fd = open_result.value();
    std::string content;
    char buffer[4096];
    ssize_t bytes_read = 0;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        content.append(buffer, bytes_read);
    }

    safe_close(fd);

    if (bytes_read < 0) {
        return Result<std::string>::error("Failed to read from file '" + path +
                                          "': " + describe_errno(errno));
    }

    return Result<std::string>::ok(content);
}

std::vector<std::string> get_executables_in_path() {
    std::vector<std::string> executables;

    std::lock_guard<std::mutex> lock(g_path_hash_mutex);
    std::string current_path = current_path_env_value();
    ensure_path_snapshot_locked(current_path);
    seed_path_hash_locked(current_path);

    executables.reserve(g_path_hash_entries.size());
    for (const auto& [command, entry] : g_path_hash_entries) {
        (void)entry;
        executables.push_back(command);
    }

    return executables;
}

bool file_exists(const std::filesystem::path& path) {
    return std::filesystem::exists(path);
}

bool initialize_cjsh_directories() {
    std::string current_path = safe_current_directory();
    setenv("PWD", current_path.c_str(), 1);

    try {
        bool home_exists = file_exists(g_user_home_path());

        if (!home_exists) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "",
                         "User home path not found",
                         {"Check user account configuration"}});
            return false;
        }
        std::filesystem::create_directories(g_cjsh_config_path());
        std::filesystem::create_directories(g_cjsh_cache_path());
        std::filesystem::create_directories(g_cjsh_generated_completions_path());
        bool history_exists = file_exists(g_cjsh_history_path());

        if (!history_exists) {
            auto write_result = write_file_content(g_cjsh_history_path().string(), "");
            if (!write_result.is_ok()) {
                print_error({ErrorType::RUNTIME_ERROR,
                             g_cjsh_history_path().c_str(),
                             write_result.error(),
                             {"Check file permissions"}});
                return false;
            }
        }

    } catch (const std::exception& e) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "",
                     "Failed to initialize interactive filesystem",
                     {"Check file permissions", "Reinstall cjsh"}});
        return false;
    }
    return true;
}

std::string find_executable_in_path(const std::string& name) {
    return resolve_command_with_cache(name, CacheUsage::Query);
}

std::string resolve_executable_for_execution(const std::string& name) {
    return resolve_command_with_cache(name, CacheUsage::Execution);
}

bool hash_executable(const std::string& name, std::string* resolved_path) {
    if (name.empty() || name.find('/') != std::string::npos) {
        if (resolved_path) {
            resolved_path->clear();
        }
        return false;
    }

    std::string resolved = resolve_command_with_cache(name, CacheUsage::Manual);
    if (resolved_path) {
        *resolved_path = resolved;
    }
    return !resolved.empty();
}

std::vector<PathHashEntry> get_path_hash_entries() {
    std::string current_path = current_path_env_value();
    std::lock_guard<std::mutex> lock(g_path_hash_mutex);
    ensure_path_snapshot_locked(current_path);
    std::vector<PathHashEntry> entries;
    entries.reserve(g_path_hash_entries.size());
    for (const auto& [command, entry] : g_path_hash_entries) {
        entries.push_back({command, entry.path, entry.hits, entry.last_used, entry.manually_added});
    }
    std::sort(entries.begin(), entries.end(),
              [](const PathHashEntry& lhs, const PathHashEntry& rhs) {
                  if (lhs.hits == rhs.hits) {
                      return lhs.command < rhs.command;
                  }
                  return lhs.hits > rhs.hits;
              });
    return entries;
}

void reset_path_hash() {
    std::lock_guard<std::mutex> lock(g_path_hash_mutex);
    g_path_hash_entries.clear();
    g_path_snapshot = current_path_env_value();
    g_path_hash_seeded = false;
}

namespace {
bool write_configuration_file(const std::filesystem::path& target_path,
                              const std::string& content) {
    if (!target_path.parent_path().empty()) {
        std::error_code dir_error;
        std::filesystem::create_directories(target_path.parent_path(), dir_error);
        if (dir_error) {
            print_error({ErrorType::RUNTIME_ERROR,
                         target_path.parent_path().string(),
                         "Failed to prepare configuration directory: " + dir_error.message(),
                         {"Check file permissions"}});
            return false;
        }
    }

    auto write_result = write_file_content(target_path.string(), content);

    if (!write_result.is_ok()) {
        print_error(
            {ErrorType::RUNTIME_ERROR, "", write_result.error(), {"Check file permissions"}});
        return false;
    }

    return true;
}
}  // namespace

bool create_profile_file(const std::filesystem::path& target_path) {
    std::string profile_content =
        "#!/usr/bin/env cjsh\n"
        "# cjsh Configuration File\n"
        "# this file is sourced when the shell starts in login "
        "mode and is sourced after ~/.profile\n"
        "# this file supports full shell scripting capabilities\n"
        "# Use the 'cjshopt login-startup-arg' command to set "
        "startup flags conditionally\n"
        "# Note: login-startup-arg can ONLY be used in configuration files (not at runtime)\n"
        "\n"
        "# Example: Conditional startup flags based on environment\n"
        "# if test -n \"$TMUX\"; then\n"
        "#     echo \"In tmux session, no flags required\"\n"
        "# else\n"
        "#     cjshopt login-startup-arg --no-colors\n"
        "#     cjshopt login-startup-arg --no-titleline\n"
        "# fi\n"
        "\n"
        "# Available startup flags:\n"
        "# cjshopt login-startup-arg --login               # Set login mode\n"
        "# cjshopt login-startup-arg --interactive         # Force interactive "
        "mode\n"
        "# cjshopt login-startup-arg --minimal             # Disable all "
        "unique cjsh "
        "features (colors, completions, syntax "
        "highlighting, smart cd, sourcing, startup time display)\n"
        "# cjshopt login-startup-arg --no-colors           # Disable colors\n"
        "# cjshopt login-startup-arg --no-titleline        # Disable title "
        "line\n"
        "# cjshopt login-startup-arg --show-startup-time   # Display shell "
        "startup time\n"
        "# cjshopt login-startup-arg --no-source           # Don't source the "
        ".cjshrc "
        "file\n"
        "# cjshopt login-startup-arg --no-completions      # Disable tab "
        "completions\n"
        "# cjshopt login-startup-arg --no-syntax-highlighting # Disable syntax "
        "highlighting\n"
        "# cjshopt login-startup-arg --no-smart-cd        # Disable smart cd shortcuts\n"
        "# cjshopt login-startup-arg --no-history-expansion # Disable history !! shortcuts\n"
        "# cjshopt login-startup-arg --no-sh-warning      # Suppress the sh invocation reminder\n"
        "# cjshopt login-startup-arg --secure             # Skip sourcing profile/rc/logout files\n"
        "# cjshopt login-startup-arg --startup-test        # Enable startup "
        "test "
        "mode\n";

    return write_configuration_file(target_path, profile_content);
}

bool create_source_file(const std::filesystem::path& target_path) {
    std::string source_content =
        "#!/usr/bin/env cjsh\n"
        "# cjsh Source File\n"
        "# this file is sourced when the shell starts in interactive mode\n"
        "# this is where your aliases and theme setup will be stored by default.\n"
        "\n"
        "# Alias examples\n"
        "alias ll='ls -la'\n"
        "\n"
        "# Syntax highlighting customization examples\n"
        "# Use 'cjshopt style_def' to customize syntax highlighting colors\n"
        "# cjshopt style_def builtin \"bold color=#FFB86C\"\n"
        "# cjshopt style_def system \"color=#50FA7B\"\n"
        "# cjshopt style_def installed \"color=#8BE9FD\"\n"
        "# cjshopt style_def comment \"italic color=green\"\n"
        "# cjshopt style_def string \"color=#F1FA8C\"\n"
        "# Run 'cjshopt style_def' for more information\n"
        "\n"
        "# Key binding customization\n"
        "# Note: Key bindings can ONLY be modified in configuration files (not at runtime)\n"
        "# Use 'cjshopt keybind list' to view current bindings\n"
        "# Available keybind subcommands (for use in config files only):\n"
        "#   set <action> <keys...>    - Replace bindings for an action\n"
        "#   add <action> <keys...>    - Add bindings without removing existing ones\n"
        "#   clear <keys...>           - Remove bindings for specific keys\n"
        "#   clear-action <action>     - Remove all custom bindings for an action\n"
        "#   reset                     - Clear all custom bindings and restore defaults\n"
        "#   profile list              - Show available key binding profiles\n"
        "#   profile set <name>        - Activate a key binding profile\n"
        "#\n"
        "# Examples:\n"
        "# Map Vim-style cursor movement to Alt+H/J/K/L\n"
        "# cjshopt keybind set cursor-left alt+h\n"
        "# cjshopt keybind set cursor-down alt+j\n"
        "# cjshopt keybind set cursor-up alt+k\n"
        "# cjshopt keybind set cursor-right alt+l\n"
        "#\n"
        "# Switch to the built-in Vim key binding profile\n"
        "# cjshopt keybind profile set vim\n"
        "#\n"
        "# Add additional delete word binding\n"
        "# cjshopt keybind add delete-word-end ctrl+delete\n"
        "#\n"
        "# Run 'cjshopt keybind --help' for more information\n"
        "\n";

    return write_configuration_file(target_path, source_content);
}

bool create_logout_file(const std::filesystem::path& target_path) {
    std::string logout_content =
        "#!/usr/bin/env cjsh\n"
        "# cjsh Logout File\n"
        "# this file is sourced when the shell exits from a login "
        "session\n"
        "# you can place any cleanup commands or messages here\n"
        "\n"
        "# Example: Display a goodbye message\n"
        "# echo \"Thank you for using cjsh! Goodbye!\"\n";

    return write_configuration_file(target_path, logout_content);
}

bool is_first_boot() {
    return !std::filesystem::exists(g_cjsh_first_boot_path());
}

void process_profile_files() {
    if (config::secure_mode) {
        return;
    }
    std::filesystem::path user_profile = g_user_home_path() / ".profile";
    if (std::filesystem::exists(user_profile)) {
        g_shell->execute_script_file(user_profile, true);
    }

    if (std::filesystem::exists(g_cjsh_profile_path())) {
        g_shell->execute_script_file(g_cjsh_profile_path(), true);
    } else if (std::filesystem::exists(g_cjsh_profile_alt_path())) {
        g_shell->execute_script_file(g_cjsh_profile_alt_path(), true);
    }
}

void process_logout_file() {
    if (!config::secure_mode && (config::interactive_mode || config::force_interactive)) {
        const auto& logout_path = g_cjsh_logout_path();
        std::error_code logout_status_ec;
        auto logout_status = std::filesystem::status(logout_path, logout_status_ec);

        if (!logout_status_ec && std::filesystem::is_regular_file(logout_status)) {
            g_shell->execute_script_file(logout_path, true);
        }
    }
}

void process_source_files() {
    if (!config::source_enabled || config::secure_mode) {
        return;
    }

    if (file_exists(g_cjsh_source_path())) {
        g_shell->execute_script_file(g_cjsh_source_path());
    } else if (file_exists(g_cjsh_source_alt_path())) {
        g_shell->execute_script_file(g_cjsh_source_alt_path());
    }
}

}  // namespace cjsh_filesystem

#include "cjsh_filesystem.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_set>
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
std::string describe_errno(int err) {
    return std::system_category().message(err);
}

template <typename Functor>
void launch_async_once(std::atomic<bool>& guard, Functor&& fn) {
    bool expected = false;
    if (!guard.compare_exchange_strong(expected, true)) {
        return;
    }

    std::thread([task = std::forward<Functor>(fn), &guard]() mutable {
        try {
            task();
        } catch (...) {
        }

        guard.store(false);
    }).detach();
}
}  // namespace

fs::path g_cjsh_path;

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

void close_pipe(int pipe_fds[2]) {
    safe_close(pipe_fds[0]);
    safe_close(pipe_fds[1]);
}

Result<FILE*> safe_fopen(const std::string& path, const std::string& mode) {
    FILE* file = std::fopen(path.c_str(), mode.c_str());
    if (file == nullptr) {
        return Result<FILE*>::error("Failed to open file '" + path + "' with mode '" + mode +
                                    "': " + describe_errno(errno));
    }
    return Result<FILE*>::ok(file);
}

void safe_fclose(FILE* file) {
    if (file != nullptr) {
        (void)std::fclose(file);
    }
}

Result<std::string> create_temp_file(const std::string& prefix) {
    std::string temp_path =
        "/tmp/" + prefix + "_" + std::to_string(getpid()) + "_" + std::to_string(time(nullptr));
    auto open_result = safe_open(temp_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (open_result.is_error()) {
        return Result<std::string>::error(open_result.error());
    }
    safe_close(open_result.value());
    return Result<std::string>::ok(temp_path);
}

Result<void> write_temp_file(const std::string& path, const std::string& content) {
    auto open_result = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (open_result.is_error()) {
        return Result<void>::error(open_result.error());
    }

    int fd = open_result.value();
    auto write_result = write_all(fd, std::string_view{content});
    safe_close(fd);

    if (write_result.is_error()) {
        return Result<void>::error(write_result.error());
    }

    return Result<void>::ok();
}

void cleanup_temp_file(const std::string& path) {
    (void)std::remove(path.c_str());
}

Result<void> write_file_content(const std::string& path, const std::string& content) {
    auto open_result = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (open_result.is_error()) {
        return Result<void>::error(open_result.error());
    }

    int fd = open_result.value();
    if (::fchmod(fd, S_IRUSR | S_IWUSR) == -1) {
        std::string error_message =
            "Failed to set secure permissions on '" + path + "': " + describe_errno(errno);
        safe_close(fd);
        return Result<void>::error(error_message);
    }
    auto write_result = write_all(fd, std::string_view{content});
    safe_close(fd);

    if (write_result.is_error()) {
        return Result<void>::error(write_result.error());
    }

    return Result<void>::ok();
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
    if (write_result.is_error()) {
        bool is_broken_pipe = (write_result.error().find("Broken pipe") != std::string::npos ||
                               write_result.error().find("EPIPE") != std::string::npos);
        if (!is_broken_pipe) {
            close_pipe(here_pipe);
            return HereStringError{HereStringErrorType::Write, write_result.error()};
        }
    }

    safe_close(here_pipe[1]);
    auto dup_result = safe_dup2(here_pipe[0], STDIN_FILENO);
    safe_close(here_pipe[0]);
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
        return access(command_path.c_str(), F_OK) == 0;
    }
    const char* path_env = getenv("PATH");
    if (path_env == nullptr) {
        return false;
    }
    std::string path_str(path_env);
    std::istringstream path_stream(path_str);
    std::string path_dir;
    while (std::getline(path_stream, path_dir, ':')) {
        if (path_dir.empty()) {
            continue;
        }
        std::string full_path = path_dir;
        full_path.append("/").append(command_path);
        if (access(full_path.c_str(), F_OK) == 0) {
            return true;
        }
    }
    return false;
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

    const char* path_env = std::getenv("PATH");
    if (path_env == nullptr) {
        return executables;
    }

    std::string path_str(path_env);
    std::unordered_set<std::string> seen_executables;

    size_t start = 0;
    while (start < path_str.size()) {
        size_t pos = path_str.find(':', start);
        size_t end = (pos != std::string::npos) ? pos : path_str.size();

        if (end > start) {
            std::string dir(path_str, start, end - start);

            if (!dir.empty()) {
                fs::path directory_path(dir);
                std::error_code ec;

                if (!fs::exists(directory_path, ec) || ec) {
                    start = (pos != std::string::npos) ? pos + 1 : path_str.size();
                    continue;
                }

                ec.clear();
                if (!fs::is_directory(directory_path, ec) || ec) {
                    start = (pos != std::string::npos) ? pos + 1 : path_str.size();
                    continue;
                }

                fs::directory_iterator it(directory_path,
                                          fs::directory_options::skip_permission_denied, ec);
                if (ec) {
                    start = (pos != std::string::npos) ? pos + 1 : path_str.size();
                    continue;
                }

                for (; it != fs::directory_iterator(); it.increment(ec)) {
                    if (ec) {
                        break;
                    }

                    const auto& entry = *it;
                    auto status = entry.status(ec);
                    if (ec) {
                        ec.clear();
                        continue;
                    }

                    if (!fs::is_regular_file(status)) {
                        continue;
                    }

                    auto perms = status.permissions();
                    constexpr auto exec_mask =
                        fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec;

                    if ((perms & exec_mask) != fs::perms::none) {
                        std::string exec_name = entry.path().filename().string();
                        if (seen_executables.insert(exec_name).second) {
                            executables.push_back(exec_name);
                        }
                    }
                }
            }
        }

        start = (pos != std::string::npos) ? pos + 1 : path_str.size();
    }

    return executables;
}

bool file_exists(const fs::path& path) {
    return fs::exists(path);
}

bool initialize_cjsh_path() {
    char path[PATH_MAX];
#ifdef __linux__
    ssize_t len = readlink("/proc/self/exe", path, PATH_MAX - 1);
    if (len != -1) {
        path[len] = '\0';
        g_cjsh_path = path;
        return true;
    }
#endif

#ifdef __APPLE__
    uint32_t size = PATH_MAX;
    if (_NSGetExecutablePath(path, &size) == 0) {
        std::unique_ptr<char, decltype(&std::free)> resolved_path(realpath(path, nullptr),
                                                                  std::free);
        if (resolved_path) {
            g_cjsh_path = resolved_path.get();
            return true;
        }
        g_cjsh_path = path;
        return true;
    }
#endif

    if (g_cjsh_path.empty()) {
        g_cjsh_path = "cjsh";
        return true;
    }

    return true;
}

bool initialize_cjsh_directories() {
    try {
        fs::create_directories(g_cache_path);
        fs::create_directories(g_cjsh_cache_path);
        fs::create_directories(g_cjsh_generated_completions_path);

        return true;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error creating cjsh directories: " << e.what() << '\n';
        return false;
    }
}

std::filesystem::path get_cjsh_path() {
    if (g_cjsh_path.empty() || g_cjsh_path == ".") {
        initialize_cjsh_path();
    }
    return g_cjsh_path;
}

std::string find_executable_in_path(const std::string& name) {
    const char* path_env = std::getenv("PATH");
    if (path_env == nullptr) {
        return "";
    }

    std::stringstream ss(path_env);
    std::string dir;

    while (std::getline(ss, dir, ':')) {
        if (dir.empty())
            continue;

        fs::path executable_path = fs::path(dir) / name;

        try {
            if (fs::exists(executable_path) && fs::is_regular_file(executable_path)) {
                auto perms = fs::status(executable_path).permissions();
                if ((perms & fs::perms::owner_exec) != fs::perms::none ||
                    (perms & fs::perms::group_exec) != fs::perms::none ||
                    (perms & fs::perms::others_exec) != fs::perms::none) {
                    return executable_path.string();
                }
            }
        } catch (const fs::filesystem_error&) {
            continue;
        }
    }

    return "";
}

bool create_profile_file() {
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
        "#     cjshopt login-startup-arg --no-themes\n"
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
        "features (themes, colors, completions, syntax "
        "highlighting, smart cd, sourcing, startup time display)\n"
        "# cjshopt login-startup-arg --no-themes           # Disable themes\n"
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
        "# cjshopt login-startup-arg --no-smart-cd         # Disable smart cd "
        "functionality\n"
        "# cjshopt login-startup-arg --startup-test        # Enable startup "
        "test "
        "mode\n";

    auto write_result = write_file_content(g_cjsh_profile_path.string(), profile_content);

    if (!write_result.is_ok()) {
        print_error(
            {ErrorType::RUNTIME_ERROR, "", write_result.error(), {"Check file permissions"}});
        return false;
    }

    return true;
}

bool create_source_file() {
    std::string source_content =
        "#!/usr/bin/env cjsh\n"
        "# cjsh Source File\n"
        "# this file is sourced when the shell starts in interactive mode\n"
        "# this is where your aliases and theme setup will be stored by default.\n"
        "\n"
        "# Alias examples\n"
        "alias ll='ls -la'\n"
        "\n"
        "# Theme configuration\n"
        "# This is the default cjsh theme\n"
        "# Theme definitions placed in the cjshrc file are always\n"
        "# activated when starting an interactive session\n"
        "# You can also load external theme files with: source path/to/theme.cjsh\n"
        "\n"
        "theme_definition {\n"
        "  terminal_title \"{PATH}\"\n"
        "\n"
        "  fill {\n"
        "    char \"\"\n"
        "    fg RESET\n"
        "    bg RESET\n"
        "  }\n"
        "\n"
        "  ps1 {\n"
        "    segment \"left_bracket\" {\n"
        "      content \"[\"\n"
        "      fg \"#FF5555\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "    segment \"username\" {\n"
        "      content \"{USERNAME}\"\n"
        "      fg \"#FFFF55\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "    segment \"at_sign\" {\n"
        "      content \"@\"\n"
        "      fg \"#55FF55\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "    segment \"hostname\" {\n"
        "      content \"{HOSTNAME}\"\n"
        "      fg \"#5555FF\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "    segment \"directory\" {\n"
        "      content \" {DIRECTORY} \"\n"
        "      fg \"#FF55FF\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "    segment \"right_bracket\" {\n"
        "      content \"]\"\n"
        "      fg \"#FF5555\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "    segment \"exit_status\" {\n"
        "      content \"{if = {STATUS} == 0 ?  : {STATUS}}\"\n"
        "      fg \"{if = {STATUS} == 0 ? RESET : #FF5555}\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "    segment \"prompt\" {\n"
        "      content \" $ \"\n"
        "      fg \"#FFFFFF\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "  }\n"
        "\n"
        "  git_segments {\n"
        "    segment \"left_bracket\" {\n"
        "      content \"[\"\n"
        "      fg \"#FF5555\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "    segment \"username\" {\n"
        "      content \"{USERNAME}\"\n"
        "      fg \"#FFFF55\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "    segment \"at_sign\" {\n"
        "      content \"@\"\n"
        "      fg \"#55FF55\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "    segment \"directory\" {\n"
        "      content \" {LOCAL_PATH} \"\n"
        "      fg \"#FF55FF\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "    segment \"branch\" {\n"
        "      content \"in {GIT_BRANCH}\"\n"
        "      fg \"#5555FF\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "    segment \"status\" {\n"
        "      content \"{GIT_STATUS} \"\n"
        "      fg \"{if = {GIT_STATUS} == ✓ ? #55FF55 : #FF5555}\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "    segment \"right_bracket\" {\n"
        "      content \"]\"\n"
        "      fg \"#FF5555\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "    segment \"exit_status\" {\n"
        "      content \"{if = {STATUS} == 0 ?  : {STATUS}}\"\n"
        "      fg \"{if = {STATUS} == 0 ? RESET : #FF5555}\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "    segment \"prompt\" {\n"
        "      content \" $ \"\n"
        "      fg \"#FFFFFF\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "  }\n"
        "\n"
        "  inline_right {\n"
        "    segment \"time\" {\n"
        "      content \"[{TIME}]\"\n"
        "      fg \"#888888\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "  }\n"
        "\n"
        "  behavior {\n"
        "    cleanup false\n"
        "    cleanup_empty_line false\n"
        "    cleanup_truncate_multiline false\n"
        "    newline_after_execution false\n"
        "  }\n"
        "}\n"
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

    auto write_result = write_file_content(g_cjsh_source_path.string(), source_content);

    if (!write_result.is_ok()) {
        print_error(
            {ErrorType::RUNTIME_ERROR, "", write_result.error(), {"Check file permissions"}});
        return false;
    }

    return true;
}

bool create_logout_file() {
    std::string logout_content =
        "#!/usr/bin/env cjsh\n"
        "# cjsh Logout File\n"
        "# this file is sourced when the shell exits from a login "
        "session\n"
        "# you can place any cleanup commands or messages here\n"
        "\n"
        "# Example: Display a goodbye message\n"
        "# echo \"Thank you for using cjsh! Goodbye!\"\n";

    auto write_result = write_file_content(g_cjsh_logout_path.string(), logout_content);

    if (!write_result.is_ok()) {
        print_error(
            {ErrorType::RUNTIME_ERROR, "", write_result.error(), {"Check file permissions"}});
        return false;
    }

    return true;
}

bool init_interactive_filesystem() {
    std::string current_path = std::filesystem::current_path().string();
    setenv("PWD", current_path.c_str(), 1);

    try {
        bool home_exists = std::filesystem::exists(g_user_home_path);
        bool history_exists = std::filesystem::exists(g_cjsh_history_path);

        if (!home_exists) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "",
                         "User home path not found",
                         {"Check user account configuration"}});
            return false;
        }

        if (!history_exists) {
            auto write_result = write_file_content(g_cjsh_history_path.string(), "");
            if (!write_result.is_ok()) {
                print_error({ErrorType::RUNTIME_ERROR,
                             g_cjsh_history_path.c_str(),
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

bool is_first_boot() {
    return !fs::exists(g_cjsh_first_boot_path);
}

}  // namespace cjsh_filesystem

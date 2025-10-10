#include "cjsh_filesystem.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <sstream>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "cjsh_syntax_highlighter.h"
#include "error_out.h"

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

std::atomic<bool> g_cache_refresh_in_progress{false};
std::atomic<bool> g_cache_cleanup_in_progress{false};

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

Result<FILE*> safe_popen(const std::string& command, const std::string& mode) {
    FILE* pipe = ::popen(command.c_str(), mode.c_str());
    if (pipe == nullptr) {
        return Result<FILE*>::error("Failed to execute command '" + command +
                                    "': " + describe_errno(errno));
    }
    return Result<FILE*>::ok(pipe);
}

int safe_pclose(FILE* file) {
    if (file == nullptr) {
        return -1;
    }
    return ::pclose(file);
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

Result<std::string> read_command_output(const std::string& command) {
    auto pipe_result = safe_popen(command, "r");
    if (pipe_result.is_error()) {
        return Result<std::string>::error(pipe_result.error());
    }

    FILE* pipe = pipe_result.value();
    std::string output;
    char buffer[256];

    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

    int exit_code = safe_pclose(pipe);
    if (exit_code != 0) {
        return Result<std::string>::error("Command '" + command + "' failed with exit code " +
                                          std::to_string(exit_code));
    }

    return Result<std::string>::ok(output);
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

bool should_refresh_executable_cache() {
    try {
        if (has_path_changed()) {
            return true;
        }

        if (!fs::exists(g_cjsh_found_executables_path)) {
            return true;
        }

        auto last = fs::last_write_time(g_cjsh_found_executables_path);
        auto now = decltype(last)::clock::now();
        bool is_old = (now - last) > std::chrono::hours(24);
        return is_old;
    } catch (...) {
        return true;
    }
}

bool build_executable_cache() {
    const char* path_env = std::getenv("PATH");
    if (path_env == nullptr) {
        return false;
    }

    std::stringstream ss(path_env);
    std::string dir;
    std::vector<fs::path> executables;

    while (std::getline(ss, dir, ':')) {
        if (dir.empty()) {
            continue;
        }

        fs::path directory_path(dir);
        std::error_code ec;

        if (!fs::exists(directory_path, ec)) {
            continue;
        }

        ec.clear();
        if (!fs::is_directory(directory_path, ec) || ec) {
            continue;
        }

        fs::directory_iterator it(directory_path, fs::directory_options::skip_permission_denied,
                                  ec);
        if (ec) {
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
                executables.push_back(entry.path());
            }
        }
    }

    std::string content;
    content.reserve(executables.size() * 16);
    for (const auto& executable : executables) {
        content += executable.filename().string();
        content.push_back('\n');
    }

    auto write_result = write_file_content(g_cjsh_found_executables_path.string(), content);

    if (write_result.is_ok()) {
        notify_cache_systems_of_update();
    }

    return write_result.is_ok();
}

std::vector<fs::path> read_cached_executables() {
    std::vector<fs::path> executables;

    auto read_result = read_file_content(g_cjsh_found_executables_path.string());
    if (read_result.is_error()) {
        return executables;
    }

    std::stringstream ss(read_result.value());
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty()) {
            executables.emplace_back(line);
        }
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
        "highlighting, smart cd, sourcing, custom ls, startup time display)\n"
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
        "# cjshopt login-startup-arg --disable-custom-ls   # Use system ls "
        "command "
        "instead of builtin ls\n"
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
        "    segment \"username\" {\n"
        "      content \"{USERNAME}@{HOSTNAME}:\"\n"
        "      fg \"#5555FF\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "    segment \"directory\" {\n"
        "      content \" {DIRECTORY} \"\n"
        "      fg \"#55FF55\"\n"
        "      bg \"RESET\"\n"
        "      separator \" \"\n"
        "      separator_fg \"#FFFFFF\"\n"
        "      separator_bg \"RESET\"\n"
        "    }\n"
        "    segment \"prompt\" {\n"
        "      content \"$ \"\n"
        "      fg \"#FFFFFF\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "  }\n"
        "\n"
        "  git_segments {\n"
        "    segment \"path\" {\n"
        "      content \" {LOCAL_PATH} \"\n"
        "      fg \"#55FF55\"\n"
        "      bg \"RESET\"\n"
        "      separator \" \"\n"
        "      separator_fg \"#FFFFFF\"\n"
        "      separator_bg \"RESET\"\n"
        "    }\n"
        "    segment \"branch\" {\n"
        "      content \"{GIT_BRANCH}\"\n"
        "      fg \"#FFFF55\"\n"
        "      bg \"RESET\"\n"
        "    }\n"
        "    segment \"status\" {\n"
        "      content \"{GIT_STATUS}\"\n"
        "      fg \"#FF5555\"\n"
        "      bg \"RESET\"\n"
        "      separator \" $ \"\n"
        "      separator_fg \"#FFFFFF\"\n"
        "      separator_bg \"RESET\"\n"
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
        bool should_refresh_cache = should_refresh_executable_cache();
        std::error_code cache_ec;
        bool cache_exists = std::filesystem::exists(g_cjsh_found_executables_path, cache_ec);
        if (cache_ec) {
            cache_exists = false;
        }

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

        if (should_refresh_cache) {
            if (!cache_exists) {
                build_executable_cache();
            } else {
                launch_async_once(g_cache_refresh_in_progress, []() { build_executable_cache(); });
            }
        } else {
            static int cleanup_counter = 0;
            if (++cleanup_counter % 10 == 0) {
                launch_async_once(g_cache_cleanup_in_progress,
                                  []() { cleanup_stale_cache_entries(); });
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

void add_executable_to_cache(const std::string& executable_name, const std::string& full_path) {
    if (executable_name.empty() || full_path.empty()) {
        return;
    }

    if (is_executable_in_cache(executable_name)) {
        return;
    }

    auto cached_executables = read_cached_executables();

    cached_executables.emplace_back(executable_name);

    std::sort(cached_executables.begin(), cached_executables.end());
    cached_executables.erase(std::unique(cached_executables.begin(), cached_executables.end()),
                             cached_executables.end());

    std::string content;
    for (const auto& exec : cached_executables) {
        content += exec.filename().string() + "\n";
    }

    auto write_result = write_file_content(g_cjsh_found_executables_path.string(), content);

    if (write_result.is_ok()) {
        notify_cache_systems_of_update();
    }
}

void invalidate_executable_cache() {
    try {
        if (fs::exists(g_cjsh_found_executables_path)) {
            fs::remove(g_cjsh_found_executables_path);
        }
        if (fs::exists(g_cjsh_path_hash_cache_path)) {
            fs::remove(g_cjsh_path_hash_cache_path);
        }
    } catch (const fs::filesystem_error& e) {
    }
}

bool is_executable_in_cache(const std::string& executable_name) {
    if (executable_name.empty()) {
        return false;
    }

    auto cached_executables = read_cached_executables();
    bool found = std::any_of(cached_executables.begin(), cached_executables.end(),
                             [&executable_name](const fs::path& exec_path) {
                                 return exec_path.filename().string() == executable_name;
                             });

    return found;
}

std::string get_current_path_hash() {
    const char* path_env = std::getenv("PATH");
    if (path_env == nullptr) {
        return "";
    }

    std::string path_str(path_env);
    std::hash<std::string> hasher;
    size_t path_hash = hasher(path_str);

    return std::to_string(path_hash);
}

void set_last_path_hash(const std::string& path_hash) {
    if (path_hash.empty()) {
        return;
    }

    auto write_result = write_file_content(g_cjsh_path_hash_cache_path.string(), path_hash);
}

bool has_path_changed() {
    std::string current_hash = get_current_path_hash();
    if (current_hash.empty()) {
        return true;
    }

    auto read_result = read_file_content(g_cjsh_path_hash_cache_path.string());

    if (read_result.is_error()) {
        set_last_path_hash(current_hash);
        return true;
    }

    std::string cached_hash = read_result.value();

    if (!cached_hash.empty() && cached_hash.back() == '\n') {
        cached_hash.pop_back();
    }

    bool changed = (cached_hash != current_hash);
    if (changed) {
        set_last_path_hash(current_hash);
    }

    return changed;
}

void remove_executable_from_cache(const std::string& executable_name) {
    if (executable_name.empty()) {
        return;
    }

    auto cached_executables = read_cached_executables();

    auto original_size = cached_executables.size();
    cached_executables.erase(std::remove_if(cached_executables.begin(), cached_executables.end(),
                                            [&executable_name](const fs::path& exec_path) {
                                                return exec_path.filename().string() ==
                                                       executable_name;
                                            }),
                             cached_executables.end());

    if (cached_executables.size() < original_size) {
        std::string content;
        for (const auto& exec : cached_executables) {
            content += exec.filename().string() + "\n";
        }

        auto write_result = write_file_content(g_cjsh_found_executables_path.string(), content);
        if (write_result.is_ok()) {
            notify_cache_systems_of_update();
        }
    }
}

void cleanup_stale_cache_entries() {
    auto cached_executables = read_cached_executables();
    std::vector<fs::path> valid_executables;
    int removed_count = 0;

    for (const auto& exec_path : cached_executables) {
        std::string exec_name = exec_path.filename().string();
        std::string full_path = find_executable_in_path(exec_name);

        if (!full_path.empty()) {
            valid_executables.push_back(exec_path);
        } else {
            removed_count++;
        }
    }

    if (removed_count > 0) {
        std::string content;
        for (const auto& exec : valid_executables) {
            content += exec.filename().string() + "\n";
        }

        auto write_result = write_file_content(g_cjsh_found_executables_path.string(), content);

        if (write_result.is_ok()) {
            notify_cache_systems_of_update();
        }
    }
}

void notify_cache_systems_of_update() {
    SyntaxHighlighter::refresh_executables_cache();

    // refresh_cached_executables();
}

bool is_first_boot() {
    return !fs::exists(g_cjsh_first_boot_path);
}

}  // namespace cjsh_filesystem

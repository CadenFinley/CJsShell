#include "utils/cjsh_filesystem.h"

#include <errno.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <sstream>
#include <system_error>
#include <vector>

#include "cjsh.h"
#include "error_out.h"
#include "utils/cjsh_completions.h"
#include "utils/cjsh_syntax_highlighter.h"

#ifdef __linux__
#include <linux/limits.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace cjsh_filesystem {

fs::path g_cjsh_path;

Result<int> FileOperations::safe_open(const std::string& path, int flags, mode_t mode) {
    int fd = ::open(path.c_str(), flags, mode);
    if (fd == -1) {
        return Result<int>::error("Failed to open file '" + path + "': " + std::string(strerror(errno)));
    }
    return Result<int>::ok(fd);
}

Result<void> FileOperations::safe_dup2(int oldfd, int newfd) {
    if (::dup2(oldfd, newfd) == -1) {
        return Result<void>::error("Failed to duplicate file descriptor " + std::to_string(oldfd) + " to " + std::to_string(newfd) + ": " +
                                   std::string(strerror(errno)));
    }
    return Result<void>::ok();
}

void FileOperations::safe_close(int fd) {
    if (fd >= 0) {
        ::close(fd);
    }
}

Result<void> FileOperations::redirect_fd(const std::string& file, int target_fd, int flags) {
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

Result<FILE*> FileOperations::safe_fopen(const std::string& path, const std::string& mode) {
    FILE* file = std::fopen(path.c_str(), mode.c_str());
    if (file == nullptr) {
        return Result<FILE*>::error("Failed to open file '" + path + "' with mode '" + mode + "': " + std::string(strerror(errno)));
    }
    return Result<FILE*>::ok(file);
}

void FileOperations::safe_fclose(FILE* file) {
    if (file != nullptr) {
        std::fclose(file);
    }
}

Result<FILE*> FileOperations::safe_popen(const std::string& command, const std::string& mode) {
    FILE* pipe = ::popen(command.c_str(), mode.c_str());
    if (pipe == nullptr) {
        return Result<FILE*>::error("Failed to execute command '" + command + "': " + std::string(strerror(errno)));
    }
    return Result<FILE*>::ok(pipe);
}

int FileOperations::safe_pclose(FILE* file) {
    if (file == nullptr) {
        return -1;
    }
    return ::pclose(file);
}

Result<std::string> FileOperations::create_temp_file(const std::string& prefix) {
    std::string temp_path = "/tmp/" + prefix + "_" + std::to_string(getpid()) + "_" + std::to_string(time(nullptr));
    auto open_result = safe_open(temp_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (open_result.is_error()) {
        return Result<std::string>::error(open_result.error());
    }
    safe_close(open_result.value());
    return Result<std::string>::ok(temp_path);
}

Result<void> FileOperations::write_temp_file(const std::string& path, const std::string& content) {
    auto open_result = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (open_result.is_error()) {
        return Result<void>::error(open_result.error());
    }

    int fd = open_result.value();
    ssize_t written = write(fd, content.c_str(), content.length());
    safe_close(fd);

    if (written != static_cast<ssize_t>(content.length())) {
        return Result<void>::error("Failed to write complete content to file '" + path + "'");
    }

    return Result<void>::ok();
}

void FileOperations::cleanup_temp_file(const std::string& path) {
    std::remove(path.c_str());
}

Result<std::string> FileOperations::read_command_output(const std::string& command) {
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
        return Result<std::string>::error("Command '" + command + "' failed with exit code " + std::to_string(exit_code));
    }

    return Result<std::string>::ok(output);
}

Result<void> FileOperations::write_file_content(const std::string& path, const std::string& content) {
    auto open_result = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (open_result.is_error()) {
        return Result<void>::error(open_result.error());
    }

    int fd = open_result.value();
    ssize_t written = write(fd, content.c_str(), content.length());
    safe_close(fd);

    if (written != static_cast<ssize_t>(content.length())) {
        return Result<void>::error("Failed to write complete content to file '" + path + "'");
    }

    return Result<void>::ok();
}

Result<std::string> FileOperations::read_file_content(const std::string& path) {
    auto open_result = safe_open(path, O_RDONLY);
    if (open_result.is_error()) {
        return Result<std::string>::error(open_result.error());
    }

    int fd = open_result.value();
    std::string content;
    char buffer[4096];
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        content.append(buffer, bytes_read);
    }

    safe_close(fd);

    if (bytes_read < 0) {
        return Result<std::string>::error("Failed to read from file '" + path + "': " + std::string(strerror(errno)));
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
    if (!path_env) {
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

        fs::directory_iterator it(directory_path, fs::directory_options::skip_permission_denied, ec);
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
            constexpr auto exec_mask = fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec;

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

    auto write_result = FileOperations::write_file_content(g_cjsh_found_executables_path.string(), content);

    if (write_result.is_ok()) {
        notify_cache_systems_of_update();
    }

    return write_result.is_ok();
}

std::vector<fs::path> read_cached_executables() {
    std::vector<fs::path> executables;

    auto read_result = FileOperations::read_file_content(g_cjsh_found_executables_path.string());
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
        char* resolved_path = realpath(path, NULL);
        if (resolved_path != nullptr) {
            g_cjsh_path = resolved_path;
            free(resolved_path);
            return true;
        } else {
            g_cjsh_path = path;
            return true;
        }
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
        fs::create_directories(g_config_path);
        fs::create_directories(g_cache_path);
        fs::create_directories(g_cjsh_data_path);
        fs::create_directories(g_cjsh_cache_path);
        fs::create_directories(g_cjsh_plugin_path);
        fs::create_directories(g_cjsh_theme_path);
        fs::create_directories(g_cjsh_ai_conversations_path);

        return true;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error creating cjsh directories: " << e.what() << std::endl;
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
    if (!path_env) {
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
                if ((perms & fs::perms::owner_exec) != fs::perms::none || (perms & fs::perms::group_exec) != fs::perms::none ||
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

void create_profile_file() {
    std::string profile_content =
        "# cjsh Configuration File\n"
        "# this file is sourced when the shell starts in login "
        "mode and is sourced after /etc/profile and ~/.profile\n"
        "# this file supports full shell scripting including "
        "conditional logic\n"
        "# Use the 'cjshopt login-startup-arg' command to set "
        "startup flags conditionally\n"
        "\n"
        "# Example: Conditional startup flags based on environment\n"
        "# if test -n \"$TMUX\"; then\n"
        "#     echo \"In tmux session, no flags required\"\n"
        "# else\n"
        "#     cjshopt login-startup-arg --no-plugins\n"
        "#     cjshopt login-startup-arg --no-themes\n"
        "#     cjshopt login-startup-arg --no-ai\n"
        "#     cjshopt login-startup-arg --no-colors\n"
        "#     cjshopt login-startup-arg --no-titleline\n"
        "# fi\n"
        "\n"
        "# Available startup flags:\n"
        "# cjshopt login-startup-arg --login               # Set login mode\n"
        "# cjshopt login-startup-arg --interactive         # Force interactive "
        "mode\n"
        "# cjshopt login-startup-arg --debug               # Enable debug "
        "mode\n"
        "# cjshopt login-startup-arg --minimal             # Disable all "
        "unique cjsh "
        "features (plugins, themes, AI, colors, completions, syntax "
        "highlighting, smart cd, sourcing, custom ls, startup time display)\n"
        "# cjshopt login-startup-arg --no-plugins          # Disable plugins\n"
        "# cjshopt login-startup-arg --no-themes           # Disable themes\n"
        "# cjshopt login-startup-arg --no-ai               # Disable AI "
        "features\n"
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

    auto write_result = FileOperations::write_file_content(g_cjsh_profile_path.string(), profile_content);

    if (!write_result.is_ok()) {
        print_error({ErrorType::RUNTIME_ERROR, nullptr, write_result.error().c_str(), {"Check file permissions"}});
    }
}

void create_source_file() {
    std::string source_content =
        "# cjsh Source File\n"
        "# this file is sourced when the shell starts in interactive mode\n"
        "# this is where your aliases, theme setup, enabled "
        "plugins will be stored by default.\n"
        "\n"
        "# Alias examples\n"
        "alias ll='ls -la'\n"
        "\n"
        "# Theme configuration\n"
        "# you can change this to load any installed theme, "
        "# by default, the 'default' theme is always loaded unless themes are "
        "disabled\n"
        "theme load default\n"
        "\n"
        "# Plugin examples\n"
        "# plugin example_plugin enable\n"
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
        "# Uninstall function, DO NOT REMOVE THIS FUNCTION\n"
        "cjsh_uninstall() {\n"
        "    rm $(readlink -f $(which cjsh))\n"
        "    echo \"Uninstalled cjsh\"\n"
        "}\n";

    auto write_result = FileOperations::write_file_content(g_cjsh_source_path.string(), source_content);

    if (!write_result.is_ok()) {
        print_error({ErrorType::RUNTIME_ERROR, nullptr, write_result.error().c_str(), {"Check file permissions"}});
    }
}

bool init_login_filesystem() {
    try {
        if (!std::filesystem::exists(g_user_home_path)) {
            print_error({ErrorType::RUNTIME_ERROR, nullptr, "User home path not found", {"Check user account configuration"}});
            return false;
        }

        if (!std::filesystem::exists(g_cjsh_profile_path)) {
            create_profile_file();
        }
    } catch (const std::exception& e) {
        print_error(
            {ErrorType::RUNTIME_ERROR, nullptr, "Failed to initialize login filesystem", {"Check file permissions", "Reinstall cjsh"}});
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
        bool source_exists = std::filesystem::exists(g_cjsh_source_path);
        bool should_refresh_cache = should_refresh_executable_cache();

        if (!home_exists) {
            print_error({ErrorType::RUNTIME_ERROR, nullptr, "User home path not found", {"Check user account configuration"}});
            return false;
        }

        if (!history_exists) {
            auto write_result = FileOperations::write_file_content(g_cjsh_history_path.string(), "");
            if (!write_result.is_ok()) {
                print_error(
                    {ErrorType::RUNTIME_ERROR, g_cjsh_history_path.c_str(), write_result.error().c_str(), {"Check file permissions"}});
                return false;
            }
        }

        if (!source_exists) {
            create_source_file();
        }

        if (should_refresh_cache) {
            build_executable_cache();
        } else {
            static int cleanup_counter = 0;
            if (++cleanup_counter % 10 == 0) {
                cleanup_stale_cache_entries();
            }
        }

    } catch (const std::exception& e) {
        print_error({ErrorType::RUNTIME_ERROR,
                     nullptr,
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
    cached_executables.erase(std::unique(cached_executables.begin(), cached_executables.end()), cached_executables.end());

    std::string content;
    for (const auto& exec : cached_executables) {
        content += exec.filename().string() + "\n";
    }

    auto write_result = FileOperations::write_file_content(g_cjsh_found_executables_path.string(), content);

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
                             [&executable_name](const fs::path& exec_path) { return exec_path.filename().string() == executable_name; });

    return found;
}

std::string get_current_path_hash() {
    const char* path_env = std::getenv("PATH");
    if (!path_env) {
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

    auto write_result = FileOperations::write_file_content(g_cjsh_path_hash_cache_path.string(), path_hash);
}

bool has_path_changed() {
    std::string current_hash = get_current_path_hash();
    if (current_hash.empty()) {
        return true;
    }

    auto read_result = FileOperations::read_file_content(g_cjsh_path_hash_cache_path.string());

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
    cached_executables.erase(
        std::remove_if(cached_executables.begin(), cached_executables.end(),
                       [&executable_name](const fs::path& exec_path) { return exec_path.filename().string() == executable_name; }),
        cached_executables.end());

    if (cached_executables.size() < original_size) {
        std::string content;
        for (const auto& exec : cached_executables) {
            content += exec.filename().string() + "\n";
        }

        auto write_result = FileOperations::write_file_content(g_cjsh_found_executables_path.string(), content);
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

        auto write_result = FileOperations::write_file_content(g_cjsh_found_executables_path.string(), content);

        if (write_result.is_ok()) {
            notify_cache_systems_of_update();
        }
    }
}

void notify_cache_systems_of_update() {

    SyntaxHighlighter::refresh_executables_cache();

    //refresh_cached_executables();
}

}  // namespace cjsh_filesystem

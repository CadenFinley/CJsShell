#include "utils/cjsh_filesystem.h"

#include <errno.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <vector>

#include "error_out.h"
#include "cjsh.h"

#ifdef __linux__
#include <linux/limits.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace cjsh_filesystem {

fs::path g_cjsh_path;

// FileOperations implementation
Result<int> FileOperations::safe_open(const std::string& path, int flags,
                                      mode_t mode) {
  int fd = ::open(path.c_str(), flags, mode);
  if (fd == -1) {
    return Result<int>::error("Failed to open file '" + path +
                              "': " + std::string(strerror(errno)));
  }
  return Result<int>::ok(fd);
}

Result<void> FileOperations::safe_dup2(int oldfd, int newfd) {
  if (::dup2(oldfd, newfd) == -1) {
    return Result<void>::error(
        "Failed to duplicate file descriptor " + std::to_string(oldfd) +
        " to " + std::to_string(newfd) + ": " + std::string(strerror(errno)));
  }
  return Result<void>::ok();
}

void FileOperations::safe_close(int fd) {
  if (fd >= 0) {
    ::close(fd);
  }
}

Result<void> FileOperations::redirect_fd(const std::string& file, int target_fd,
                                         int flags) {
  auto open_result = safe_open(file, flags, 0644);
  if (open_result.is_error()) {
    return Result<void>::error(open_result.error());
  }

  int file_fd = open_result.value();

  // Only dup2 if the file descriptor is different from target
  if (file_fd != target_fd) {
    auto dup_result = safe_dup2(file_fd, target_fd);
    safe_close(file_fd);
    if (dup_result.is_error()) {
      return dup_result;
    }
  }

  return Result<void>::ok();
}

// FILE* based operations
Result<FILE*> FileOperations::safe_fopen(const std::string& path,
                                         const std::string& mode) {
  FILE* file = std::fopen(path.c_str(), mode.c_str());
  if (file == nullptr) {
    return Result<FILE*>::error("Failed to open file '" + path +
                                "' with mode '" + mode +
                                "': " + std::string(strerror(errno)));
  }
  return Result<FILE*>::ok(file);
}

void FileOperations::safe_fclose(FILE* file) {
  if (file != nullptr) {
    std::fclose(file);
  }
}

Result<FILE*> FileOperations::safe_popen(const std::string& command,
                                         const std::string& mode) {
  FILE* pipe = ::popen(command.c_str(), mode.c_str());
  if (pipe == nullptr) {
    return Result<FILE*>::error("Failed to execute command '" + command +
                                "': " + std::string(strerror(errno)));
  }
  return Result<FILE*>::ok(pipe);
}

int FileOperations::safe_pclose(FILE* file) {
  if (file == nullptr) {
    return -1;
  }
  return ::pclose(file);
}

// Temporary file utilities
Result<std::string> FileOperations::create_temp_file(
    const std::string& prefix) {
  std::string temp_path = "/tmp/" + prefix + "_" + std::to_string(getpid()) +
                          "_" + std::to_string(time(nullptr));
  auto open_result = safe_open(temp_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (open_result.is_error()) {
    return Result<std::string>::error(open_result.error());
  }
  safe_close(open_result.value());
  return Result<std::string>::ok(temp_path);
}

Result<void> FileOperations::write_temp_file(const std::string& path,
                                             const std::string& content) {
  auto open_result = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (open_result.is_error()) {
    return Result<void>::error(open_result.error());
  }

  int fd = open_result.value();
  ssize_t written = write(fd, content.c_str(), content.length());
  safe_close(fd);

  if (written != static_cast<ssize_t>(content.length())) {
    return Result<void>::error("Failed to write complete content to file '" +
                               path + "'");
  }

  return Result<void>::ok();
}

void FileOperations::cleanup_temp_file(const std::string& path) {
  std::remove(path.c_str());
}

// High-level utilities
Result<std::string> FileOperations::read_command_output(
    const std::string& command) {
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
    return Result<std::string>::error("Command '" + command +
                                      "' failed with exit code " +
                                      std::to_string(exit_code));
  }

  return Result<std::string>::ok(output);
}

Result<void> FileOperations::write_file_content(const std::string& path,
                                                const std::string& content) {
  auto open_result = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (open_result.is_error()) {
    return Result<void>::error(open_result.error());
  }

  int fd = open_result.value();
  ssize_t written = write(fd, content.c_str(), content.length());
  safe_close(fd);

  if (written != static_cast<ssize_t>(content.length())) {
    return Result<void>::error("Failed to write complete content to file '" +
                               path + "'");
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
    return Result<std::string>::error("Failed to read from file '" + path +
                                      "': " + std::string(strerror(errno)));
  }

  return Result<std::string>::ok(content);
}

bool should_refresh_executable_cache() {
  try {
    if (!fs::exists(g_cjsh_found_executables_path))
      return true;
    auto last = fs::last_write_time(g_cjsh_found_executables_path);
    auto now = decltype(last)::clock::now();
    return (now - last) > std::chrono::hours(24);
  } catch (...) {
    return true;
  }
}

bool build_executable_cache() {
  const char* path_env = std::getenv("PATH");
  if (!path_env)
    return false;
  std::stringstream ss(path_env);
  std::string dir;
  std::vector<fs::path> executables;
  while (std::getline(ss, dir, ':')) {
    fs::path p(dir);
    if (!fs::is_directory(p))
      continue;
    try {
      for (auto& entry : fs::directory_iterator(
               p, fs::directory_options::skip_permission_denied)) {
        auto perms = fs::status(entry.path()).permissions();
        if (fs::is_regular_file(entry.path()) &&
            (perms & fs::perms::owner_exec) != fs::perms::none) {
          executables.push_back(entry.path());
        }
      }
    } catch (const fs::filesystem_error& e) {
    }
  }

  // Build content string
  std::string content;
  for (auto& e : executables) {
    content += e.filename().string() + "\n";
  }

  // Use FileOperations for safe writing
  auto write_result = FileOperations::write_file_content(
      g_cjsh_found_executables_path.string(), content);
  return write_result.is_ok();
}

std::vector<fs::path> read_cached_executables() {
  std::vector<fs::path> executables;

  // Use FileOperations for safe reading
  auto read_result =
      FileOperations::read_file_content(g_cjsh_found_executables_path.string());
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
  if (readlink("/proc/self/exe", path, PATH_MAX) != -1) {
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
    fs::create_directories(g_cjsh_ai_config_path);
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

void create_profile_file() {
  std::string profile_content =
      "# cjsh Configuration File\n"
      "# this file is sourced when the shell starts in login "
      "mode and is sourced after /etc/profile and ~/.profile\n"
      "# this file supports full shell scripting including "
      "conditional logic\n"
      "# Use the 'login-startup-arg' builtin command to set "
      "startup flags conditionally\n"
      "\n"
      "# Example: Conditional startup flags based on environment\n"
      "# if test -n \"$TMUX\"; then\n"
      "#     echo \"In tmux session, no flags required\"\n"
      "# else\n"
      "#     login-startup-arg --no-plugins\n"
      "#     login-startup-arg --no-themes\n"
      "#     login-startup-arg --no-ai\n"
      "#     login-startup-arg --no-colors\n"
      "#     login-startup-arg --no-titleline\n"
      "# fi\n"
      "\n"
      "# Available startup flags:\n"
      "# login-startup-arg --login               # Enable login mode\n"
      "# login-startup-arg --interactive         # Force interactive mode\n"
      "# login-startup-arg --debug               # Enable debug mode\n"
      "# login-startup-arg --minimal             # Disable all unique cjsh "
      "features (plugins, themes, AI, colors, completions, syntax "
      "highlighting, smart cd, sourcing, custom ls colors, startup time "
      "display)\n"
      "# login-startup-arg --no-plugins          # Disable plugins\n"
      "# login-startup-arg --no-themes           # Disable themes\n"
      "# login-startup-arg --no-ai               # Disable AI features\n"
      "# login-startup-arg --no-colors           # Disable colorized output\n"
      "# login-startup-arg --no-titleline        # Disable title line\n"
      "# login-startup-arg --show-startup-time   # Enable startup time "
      "display\n"
      "# login-startup-arg --no-source           # Don't source the .cjshrc "
      "file\n"
      "# login-startup-arg --no-completions      # Disable tab completions\n"
      "# login-startup-arg --no-syntax-highlighting  # Disable syntax "
      "highlighting\n"
      "# login-startup-arg --no-smart-cd         # Disable smart cd "
      "functionality\n"
      "# login-startup-arg --disable-custom-ls   # Use system ls instead of "
      "builtin\n"
      "# login-startup-arg --startup-test        # Enable startup test mode\n";

  auto write_result = FileOperations::write_file_content(
      g_cjsh_profile_path.string(), profile_content);

  if (!write_result.is_ok()) {
    print_error({ErrorType::RUNTIME_ERROR,
                 nullptr,
                 write_result.error().c_str(),
                 {"Check file permissions"}});
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
      "# you can change this to load any installed theme, "
      "# by default, the 'default' theme is always loaded unless themes are "
      "disabled\n"
      "theme load default\n"
      "\n"
      "# plugin examples\n"
      "# plugin example_plugin enable\n"
      "\n"
      "# Uninstall function, DO NOT REMOVE THIS FUNCTION\n"
      "cjsh_uninstall() {\n"
      "    rm -rf " +
      g_cjsh_path.string() +
      "\n"
      "    echo \"Uninstalled cjsh\"\n"
      "}\n";

  auto write_result = FileOperations::write_file_content(
      g_cjsh_source_path.string(), source_content);

  if (!write_result.is_ok()) {
    print_error({ErrorType::RUNTIME_ERROR,
                 nullptr,
                 write_result.error().c_str(),
                 {"Check file permissions"}});
  }
}

bool init_login_filesystem() {
  // verify and create if needed the cjsh login filesystem
  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing login filesystem" << std::endl;
  try {
    if (!std::filesystem::exists(g_user_home_path)) {
      print_error({ErrorType::RUNTIME_ERROR,
                   nullptr,
                   "User home path not found",
                   {"Check user account configuration"}});
      return false;
    }

    if (!std::filesystem::exists(g_cjsh_profile_path)) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Creating profile file" << std::endl;
      create_profile_file();
    }
  } catch (const std::exception& e) {
    print_error({ErrorType::RUNTIME_ERROR,
                 nullptr,
                 "Failed to initialize login filesystem",
                 {"Check file permissions", "Reinstall cjsh"}});
    return false;
  }
  return true;
}

bool init_interactive_filesystem() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing interactive filesystem" << std::endl;

  // Cache current path to avoid multiple filesystem calls
  std::string current_path = std::filesystem::current_path().string();
  if (g_debug_mode)
    std::cerr << "DEBUG: Current path: " << current_path << std::endl;
  setenv("PWD", current_path.c_str(), 1);

  try {
    // Cache file existence results to avoid repeated checks
    bool home_exists = std::filesystem::exists(g_user_home_path);
    bool history_exists = std::filesystem::exists(g_cjsh_history_path);
    bool source_exists = std::filesystem::exists(g_cjsh_source_path);
    bool should_refresh_cache = should_refresh_executable_cache();

    if (!home_exists) {
      print_error({ErrorType::RUNTIME_ERROR,
                   nullptr,
                   "User home path not found",
                   {"Check user account configuration"}});
      return false;
    }

    // Create files if needed based on cached existence checks
    if (!history_exists) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Creating history file" << std::endl;
      auto write_result = FileOperations::write_file_content(
          g_cjsh_history_path.string(), "");
      if (!write_result.is_ok()) {
        print_error({ErrorType::RUNTIME_ERROR,
                     g_cjsh_history_path.c_str(),
                     write_result.error().c_str(),
                     {"Check file permissions"}});
        return false;
      }
    }

    // .cjshrc
    if (!source_exists) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Creating source file" << std::endl;
      create_source_file();
    }

    // Only refresh executable cache if needed
    if (should_refresh_cache) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Refreshing executable cache" << std::endl;
      build_executable_cache();
    } else {
      if (g_debug_mode)
        std::cerr << "DEBUG: Using existing executable cache" << std::endl;
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

}  // namespace cjsh_filesystem

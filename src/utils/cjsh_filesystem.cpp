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

}  // namespace cjsh_filesystem

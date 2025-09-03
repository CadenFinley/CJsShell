#include "restart_command.h"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "ai.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "plugin.h"
#include "theme.h"

int restart_command(const std::vector<std::string>& args) {
  std::cout << "Restarting shell..." << std::endl;

  // Perform cleanup operations before restarting
  std::cout << "Cleaning up resources before restart..." << std::endl;

  // Save important global state
  std::filesystem::path shell_path = cjsh_filesystem::g_cjsh_path;
  std::vector<std::string> startup_args = g_startup_args;

  // Clean up global resources in reverse order of initialization
  if (g_theme) {
    delete g_theme;
    g_theme = nullptr;
  }

  // Delete the AI instance before setting the pointer to nullptr
  if (g_ai) {
    delete g_ai;
    g_ai = nullptr;
  }

  // Delete the plugin instance before setting the pointer to nullptr
  if (g_plugin) {
    delete g_plugin;
    g_plugin = nullptr;
  }

  // Reset the shell last (this will clean up any additional resources)
  g_shell.reset();

  if (!std::filesystem::exists(shell_path)) {
    std::cerr << "Error: Could not find shell executable at " +
                     shell_path.string()
              << std::endl;
    return 1;
  }

  // Make sure the path is absolute to avoid any resolution issues
  shell_path = std::filesystem::absolute(shell_path);

  std::string path_str = shell_path.string();
  const char* path_cstr = path_str.c_str();

  std::vector<std::string> flags_to_remove;
  bool next_is_removal_flag = false;

  for (size_t i = 1; i < args.size(); ++i) {
    const std::string& arg = args[i];

    if (arg == "--remove") {
      next_is_removal_flag = true;
      continue;
    }

    if (next_is_removal_flag) {
      flags_to_remove.push_back(arg);
      next_is_removal_flag = false;
      continue;
    }

    if (arg.substr(0, 9) == "--remove=") {
      flags_to_remove.push_back(arg.substr(9));
    }
  }

  // Keep all the string objects alive for the duration of the function
  std::vector<std::string> arg_strings;
  std::vector<char*> args_vec;

  // Add the executable path as the first argument
  arg_strings.push_back(path_str);
  args_vec.push_back(const_cast<char*>(arg_strings.back().c_str()));

  if (!startup_args.empty()) {
    for (const auto& arg : startup_args) {
      bool should_remove = false;
      for (const auto& flag : flags_to_remove) {
        if (arg == flag) {
          should_remove = true;
          if (g_debug_mode) {
            std::cerr << "DEBUG: Removing startup flag: " << arg << std::endl;
          }
          break;
        }
      }

      if (!should_remove) {
        arg_strings.push_back(arg);
        args_vec.push_back(const_cast<char*>(arg_strings.back().c_str()));
      }
    }
  }

  for (size_t i = 1; i < args.size(); ++i) {
    const std::string& arg = args[i];

    if (arg == "--remove") {
      i++;
      continue;
    }
    if (arg.substr(0, 9) == "--remove=") {
      continue;
    }

    arg_strings.push_back(arg);
    args_vec.push_back(const_cast<char*>(arg_strings.back().c_str()));
  }

  args_vec.push_back(nullptr);

  if (g_debug_mode) {
    std::cerr << "DEBUG: Restarting shell with " << args_vec.size() - 1
              << " args" << std::endl;
    for (size_t i = 0; i < args_vec.size() - 1; ++i) {
      std::cerr << "DEBUG: Arg " << i << ": " << args_vec[i] << std::endl;
    }
  }

  if (access(path_cstr, X_OK) != 0) {
    std::string error_message =
        "Error: Shell executable at " + std::string(path_cstr) +
        " is not accessible or executable: " + std::string(strerror(errno));
    std::cerr << error_message << std::endl;
    return 1;
  }

  // Clear any error state before exec
  errno = 0;

  // Make sure stderr is flushed before the execv call
  std::cerr.flush();
  std::cout.flush();

  // Ensure stderr is set to be inherited (not close-on-exec)
  int stderr_fd = fileno(stderr);
  if (stderr_fd != -1) {
    // Get current flags
    int flags = fcntl(stderr_fd, F_GETFD);
    if (flags != -1) {
      // Clear the close-on-exec flag
      flags &= ~FD_CLOEXEC;
      fcntl(stderr_fd, F_SETFD, flags);
    }
  }

  std::cout << "Cleanup complete. Executing new shell process..." << std::endl;

  // Use execv to replace the current process
  if (execv(path_cstr, args_vec.data()) == -1) {
    std::string error_message =
        "Error restarting shell: " + std::string(strerror(errno)) +
        " (errno: " + std::to_string(errno) + ")";
    std::cerr << error_message << std::endl;
    return 1;
  }

  std::cerr << "Unexpected error: exec call returned" << std::endl;
  return 1;
}

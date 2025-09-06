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
  std::filesystem::path shell_path = cjsh_filesystem::get_cjsh_path();
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

  // Debug startup_args before processing
  if (g_debug_mode) {
    std::cerr << "DEBUG: startup_args before processing:" << std::endl;
    for (size_t i = 0; i < startup_args.size(); ++i) {
      std::cerr << "DEBUG: startup_arg " << i << ": " << startup_args[i]
                << std::endl;
    }
  }

  // First, gather all the flag arguments from startup_args
  std::vector<std::string> flag_args;
  if (g_debug_mode) {
    std::cerr << "DEBUG: Gathering flags from startup_args:" << std::endl;
  }

  for (const auto& arg : startup_args) {
    // Skip empty args or the first arg (executable path)
    if (arg.empty() || arg == path_str) {
      continue;
    }

    // Only include arguments that start with "--"
    if (arg.size() >= 2 && arg[0] == '-' && arg[1] == '-') {
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
        // Add to our flag args list to be processed
        if (g_debug_mode) {
          std::cerr << "DEBUG: Adding flag from startup_args: '" << arg << "'"
                    << std::endl;
        }
        flag_args.push_back(arg);
      }
    }
  }

  // Add any additional flags from the restart command args
  for (size_t i = 1; i < args.size(); ++i) {
    const std::string& arg = args[i];

    // Skip removal flags
    if (arg == "--remove") {
      i++;
      continue;
    }
    if (arg.substr(0, 9) == "--remove=") {
      continue;
    }

    // Add this arg to our flag args
    if (arg.size() >= 2 && arg[0] == '-' && arg[1] == '-') {
      flag_args.push_back(arg);
    }
  }

  // Sort and de-duplicate the flags to avoid repeats
  if (g_debug_mode) {
    std::cerr << "DEBUG: Before sorting and deduplication:" << std::endl;
    for (const auto& flag : flag_args) {
      std::cerr << "DEBUG: Flag: '" << flag << "'" << std::endl;
    }
  }

  std::sort(flag_args.begin(), flag_args.end());
  flag_args.erase(std::unique(flag_args.begin(), flag_args.end()),
                  flag_args.end());

  if (g_debug_mode) {
    std::cerr << "DEBUG: After sorting and deduplication:" << std::endl;
    for (const auto& flag : flag_args) {
      std::cerr << "DEBUG: Flag: '" << flag << "'" << std::endl;
    }
  };

  // Clear any existing args and rebuild from scratch
  arg_strings.clear();
  args_vec.clear();

  // Add the executable path as the first argument
  arg_strings.push_back(path_str);

  // Now add all the flags to the args vector
  for (const auto& flag : flag_args) {
    arg_strings.push_back(flag);
  }

  // Build the args_vec from the arg_strings
  for (const auto& arg : arg_strings) {
    args_vec.push_back(const_cast<char*>(arg.c_str()));
  }

  // Terminate the args list
  args_vec.push_back(nullptr);

  if (g_debug_mode) {
    std::cerr << "DEBUG: Final args_vec after rebuilding:" << std::endl;
    for (size_t i = 0; i < args_vec.size() - 1; ++i) {
      std::cerr << "DEBUG: args_vec[" << i << "]: '" << args_vec[i] << "'"
                << std::endl;
    }
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: Restarting shell with " << args_vec.size() - 1
              << " args" << std::endl;
    std::cerr << "DEBUG: Args vector size: " << args_vec.size() << std::endl;
    std::cerr << "DEBUG: Args string vector size: " << arg_strings.size()
              << std::endl;

    for (size_t i = 0; i < args_vec.size() - 1; ++i) {
      if (args_vec[i] == nullptr) {
        std::cerr << "DEBUG: Arg " << i << ": NULL POINTER" << std::endl;
      } else {
        std::cerr << "DEBUG: Arg " << i << ": '" << args_vec[i] << "'"
                  << std::endl;
      }
    }

    // Also print out the flag_args for better debugging
    std::cerr << "DEBUG: All flags after processing:" << std::endl;
    for (const auto& flag : flag_args) {
      std::cerr << "DEBUG: Flag: '" << flag << "'" << std::endl;
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
  if (g_debug_mode) {
    std::cerr << "DEBUG: Final execv call with args:" << std::endl;
    for (size_t i = 0; i < args_vec.size() - 1; ++i) {
      std::cerr << "DEBUG: execv arg[" << i << "]: '" << args_vec[i] << "'"
                << std::endl;
    }
  }

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

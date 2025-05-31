#include "restart_command.h"

#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "cjsh_filesystem.h"
#include "main.h"

int restart_command(const std::vector<std::string>& args) {
  std::cout << "Restarting shell..." << std::endl;

  std::filesystem::path shell_path = cjsh_filesystem::g_cjsh_path;

  if (!std::filesystem::exists(shell_path)) {
    std::cerr << "Error: Could not find shell executable at " +
                     shell_path.string()
              << std::endl;
    return 1;
  }

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

  std::vector<char*> args_vec;
  args_vec.push_back(const_cast<char*>(path_cstr));

  // Process the startup args first before adding new args
  if (!g_startup_args.empty()) {
    for (const auto& arg : g_startup_args) {
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
        args_vec.push_back(const_cast<char*>(arg.c_str()));
      }
    }
  }

  // Add new arguments that aren't removal flags
  for (size_t i = 1; i < args.size(); ++i) {
    const std::string& arg = args[i];

    if (arg == "--remove") {
      i++;
      continue;
    }
    if (arg.substr(0, 9) == "--remove=") {
      continue;
    }

    args_vec.push_back(const_cast<char*>(arg.c_str()));
  }

  // Important: Add null terminator
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

  // Ensure terminal is in a good state before exec
  if (isatty(STDIN_FILENO)) {
    tcflush(STDIN_FILENO, TCIFLUSH);  // Clear input buffer
    
    // If we're in a login shell, reset terminal attributes
    extern bool g_terminal_state_saved;
    extern struct termios g_shell_tmodes;
    
    if (g_terminal_state_saved) {
      if (g_debug_mode) {
        std::cerr << "DEBUG: Restoring terminal state before exec" << std::endl;
      }
      tcsetattr(STDIN_FILENO, TCSANOW, &g_shell_tmodes);
    }
  }

  // Close all file descriptors except the standard ones
  for (int fd = 3; fd < 256; fd++) {
    close(fd);
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: Executing " << path_cstr << " with " 
              << args_vec.size() - 1 << " arguments" << std::endl;
  }

  // Make sure environment is properly set before exec
  std::string cwd = std::filesystem::current_path().string();
  setenv("PWD", cwd.c_str(), 1);
  
  if (execv(path_cstr, args_vec.data()) == -1) {
    std::string error_message =
        "Error restarting shell: " + std::string(strerror(errno));
    std::cerr << error_message << std::endl;
    return 1;
  }

  std::cerr << "Unexpected error: exec call returned" << std::endl;
  return 1;
}

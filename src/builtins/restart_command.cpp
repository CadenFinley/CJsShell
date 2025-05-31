#include "restart_command.h"

#include <unistd.h>

#include <cstring>
#include <filesystem>
#include <iostream>
#include <vector>

#include "cjsh_filesystem.h"
#include "main.h"

int restart_command() {
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

  std::vector<char*> args_vec;
  args_vec.push_back(const_cast<char*>(path_cstr));

  if (!g_startup_args.empty()) {
    for (const auto& arg : g_startup_args) {
      args_vec.push_back(const_cast<char*>(arg.c_str()));
    }
  }
  args_vec.push_back(nullptr);  // Null termination for execv

  if (g_debug_mode) {
    std::cerr << "DEBUG: Restarting shell with " << args_vec.size() - 1
              << " args" << std::endl;
    for (size_t i = 0; i < args_vec.size() - 1; ++i) {
      std::cerr << "DEBUG: Arg " << i << ": " << args_vec[i] << std::endl;
    }
  }

  if (execv(path_cstr, args_vec.data()) == -1) {
    std::string error_message =
        "Error restarting shell: " + std::string(strerror(errno));
    std::cerr << error_message << std::endl;
    return 1;
  }

  return 0;
}

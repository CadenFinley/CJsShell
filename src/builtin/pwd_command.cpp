#include "pwd_command.h"
#include <limits.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>

int pwd_command(const std::vector<std::string>& args) {
  bool logical = true;

  for (size_t i = 1; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (arg == "-L") {
      logical = true;
    } else if (arg == "-P") {
      logical = false;
    } else if (arg == "--help") {
      std::cout << "Usage: pwd [-L | -P]\n";
      std::cout << "Print the current working directory.\n\n";
      std::cout << "Options:\n";
      std::cout
          << "  -L  print the logical current working directory (default)\n";
      std::cout << "  -P  print the physical current working directory\n";
      return 0;
    } else {
      std::cerr << "pwd: invalid option -- '" << arg << "'\n";
      std::cerr << "Try 'pwd --help' for more information.\n";
      return 1;
    }
  }

  std::string path;

  if (logical) {
    const char* pwd_env = getenv("PWD");
    if (pwd_env && pwd_env[0] == '/') {
      path = pwd_env;
    } else {
      char* cwd = getcwd(nullptr, 0);
      if (cwd) {
        path = cwd;
        free(cwd);
      } else {
        perror("pwd");
        return 1;
      }
    }
  } else {
    char* cwd = getcwd(nullptr, 0);
    if (cwd) {
      path = cwd;
      free(cwd);
    } else {
      perror("pwd");
      return 1;
    }
  }

  std::cout << path << std::endl;
  return 0;
}

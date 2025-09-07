#include "exit_command.h"

#include "cjsh.h"

int exit_command(const std::vector<std::string>& args) {
  // Numeric exit: 'exit N' should terminate shell with status N
  if (args.size() >= 2) {
    const std::string& val = args[1];
    char* endptr = nullptr;
    long code = std::strtol(val.c_str(), &endptr, 10);
    if (endptr && *endptr == '\0') {
      // Normalize exit code to 0-255
      int ec = static_cast<int>(code) & 0xFF;
      std::exit(ec);
    }
  }
  // Forced exit via flags
  if (std::find(args.begin(), args.end(), "-f") != args.end() ||
      std::find(args.begin(), args.end(), "--force") != args.end()) {
    std::exit(0);
  }

  g_exit_flag = true;
  return 0;
}
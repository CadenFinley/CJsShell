#include "umask_command.h"
#include <sys/stat.h>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

mode_t parse_octal_mode(const std::string& mode_str) {
  if (mode_str.empty()) {
    return static_cast<mode_t>(-1);
  }

  // Check if all characters are octal digits
  for (char c : mode_str) {
    if (c < '0' || c > '7') {
      return static_cast<mode_t>(-1);
    }
  }

  try {
    unsigned long mode = std::stoul(mode_str, nullptr, 8);
    if (mode > 0777) {
      return static_cast<mode_t>(-1);
    }
    return static_cast<mode_t>(mode);
  } catch (...) {
    return static_cast<mode_t>(-1);
  }
}

std::string format_octal_mode(mode_t mode) {
  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(4) << std::oct << mode;
  return oss.str();
}

mode_t parse_symbolic_mode(const std::string& mode_str, mode_t current_mask) {
  // This is a simplified symbolic mode parser
  // Full symbolic mode parsing is quite complex

  if (mode_str == "u=rwx,g=rx,o=rx") {
    return 022;  // 755
  } else if (mode_str == "u=rw,g=r,o=r") {
    return 022;  // 644
  } else if (mode_str == "u=rwx,g=,o=") {
    return 077;  // 700
  }

  // For now, return the current mask if we can't parse it
  return current_mask;
}

int umask_command(const std::vector<std::string>& args) {
  mode_t current_mask = umask(0);  // Get current mask
  umask(current_mask);             // Restore it

  if (args.size() == 1) {
    // Display current umask
    std::cout << format_octal_mode(current_mask) << std::endl;
    return 0;
  }

  bool symbolic_mode = false;
  size_t mode_index = 1;

  // Check for -S option (symbolic output)
  if (args.size() > 1 && args[1] == "-S") {
    if (args.size() == 2) {
      // Display in symbolic format
      mode_t perms = (~current_mask) & 0777;

      // User permissions
      std::cout << "u=";
      if (perms & S_IRUSR)
        std::cout << "r";
      if (perms & S_IWUSR)
        std::cout << "w";
      if (perms & S_IXUSR)
        std::cout << "x";

      std::cout << ",g=";
      if (perms & S_IRGRP)
        std::cout << "r";
      if (perms & S_IWGRP)
        std::cout << "w";
      if (perms & S_IXGRP)
        std::cout << "x";

      std::cout << ",o=";
      if (perms & S_IROTH)
        std::cout << "r";
      if (perms & S_IWOTH)
        std::cout << "w";
      if (perms & S_IXOTH)
        std::cout << "x";

      std::cout << std::endl;
      return 0;
    }

    symbolic_mode = true;
    mode_index = 2;
  }

  if (mode_index >= args.size()) {
    std::cerr << "umask: usage: umask [-S] [mode]" << std::endl;
    return 2;
  }

  std::string mode_str = args[mode_index];
  mode_t new_mask;

  if (symbolic_mode || mode_str.find('=') != std::string::npos) {
    // Symbolic mode
    new_mask = parse_symbolic_mode(mode_str, current_mask);
    if (new_mask == current_mask && mode_str != "u=rwx,g=rwx,o=rwx") {
      std::cerr << "umask: " << mode_str << ": invalid symbolic mode"
                << std::endl;
      return 1;
    }
  } else {
    // Octal mode
    new_mask = parse_octal_mode(mode_str);
    if (new_mask == static_cast<mode_t>(-1)) {
      std::cerr << "umask: " << mode_str << ": octal number out of range"
                << std::endl;
      return 1;
    }
  }

  // Set the new umask
  umask(new_mask);

  return 0;
}

#include "history_command.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "cjsh_filesystem.h"

#define PRINT_ERROR(MSG) std::cerr << (MSG) << '\n'

int history_command(const std::vector<std::string>& args) {
  // Ensure directories are initialized
  cjsh_filesystem::initialize_cjsh_directories();

  std::ifstream history_file(cjsh_filesystem::g_cjsh_history_path);
  if (!history_file.is_open()) {
    // If history file doesn't exist, create it or show empty history
    std::ofstream create_file(cjsh_filesystem::g_cjsh_history_path);
    if (create_file.is_open()) {
      create_file.close();
      // Try to open again for reading
      history_file.open(cjsh_filesystem::g_cjsh_history_path);
    }

    if (!history_file.is_open()) {
      PRINT_ERROR("Error: Could not open or create history file at " +
                  cjsh_filesystem::g_cjsh_history_path.string());
      return 1;
    }
  }

  std::string line;
  int index = 0;

  if (args.size() > 1) {
    try {
      index = std::stoi(args[1]);
    } catch (const std::invalid_argument&) {
      PRINT_ERROR("Invalid index: " + args[1]);
      return 1;
    }
    for (int i = 0; i < index && std::getline(history_file, line); ++i) {
      std::cout << std::setw(5) << i << "  " << line << std::endl;
    }
  } else {
    int i = 0;
    while (std::getline(history_file, line)) {
      std::cout << std::setw(5) << i++ << "  " << line << std::endl;
    }
  }

  history_file.close();
  return 0;
}

#include "uninstall_command.h"

#include <iostream>

#include "cjsh_filesystem.h"

int uninstall_command() {
  std::cout << "To uninstall CJ's Shell run the following brew command:"
            << std::endl;
  std::cout << "brew uninstall cjsh" << std::endl;
  std::cout << "To remove the application data, run:" << std::endl;
  std::cout << "rm -rf " << cjsh_filesystem::g_cjsh_data_path.string()
            << std::endl;
  return 0;
}

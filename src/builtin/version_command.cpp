#include "version_command.h"

#include <iostream>

#include "cjsh.h"

int version_command(const std::vector<std::string>& args) {
  (void)args;
  std::cout << "CJ's Shell v" << c_version << std::endl;
  return 0;
}

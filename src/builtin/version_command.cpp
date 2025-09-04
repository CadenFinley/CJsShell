#include "version_command.h"

#include <iostream>

#include "cjsh.h"

int version_command(const std::vector<std::string>& args) {
  (void)args;
  std::cout << "CJ's Shell v" << c_version
            << (PRE_RELEASE ? pre_release_line : "") << std::endl;
  return 0;
}

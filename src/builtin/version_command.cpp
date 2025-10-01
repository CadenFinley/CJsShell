#include "version_command.h"

#include <iostream>

#include "cjsh.h"

int version_command(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "CJ's Shell v" << c_version << " (git " << c_git_hash
              << ")" << std::endl;
    std::cout << "Copyright (c) 2025 Caden Finley" << std::endl;
    std::cout << "Licensed under the MIT License" << std::endl;
    return 0;
}

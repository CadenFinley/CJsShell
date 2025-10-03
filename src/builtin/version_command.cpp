#include "version_command.h"

#include "builtin_help.h"

#include <iostream>
#include <string>

#include "cjsh.h"

static const std::string c_git_hash = CJSH_GIT_HASH;

int version_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args,
                            {"Usage: version",
                             "Display cjsh version and build information."})) {
        return 0;
    }

#ifndef CJSH_BUILD_ARCH
#define CJSH_BUILD_ARCH "unknown"
#endif
#ifndef CJSH_BUILD_PLATFORM
#define CJSH_BUILD_PLATFORM "unknown"
#endif

    std::cout << "cjsh v" << c_version << " (git " << c_git_hash << ") (" << CJSH_BUILD_ARCH << "-"
              << CJSH_BUILD_PLATFORM << ")" << std::endl;
    std::cout << "Copyright (c) 2025 Caden Finley MIT License" << std::endl;
    return 0;
}

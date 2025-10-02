#ifndef CJSH_NOB_DEPENDENCIES_H
#define CJSH_NOB_DEPENDENCIES_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "nob_build_config.h"

static inline bool check_dependencies(void) {
    nob_log(NOB_INFO, "Checking dependencies...");

    Nob_Cmd cmd = {0};

    nob_cmd_append(&cmd, "which", "g++");
    if (nob_cmd_run(&cmd, .stdout_path = "/dev/null", .stderr_path = "/dev/null")) {
        return true;
    }

    cmd.count = 0;
    nob_cmd_append(&cmd, "which", "clang++");
    if (nob_cmd_run(&cmd, .stdout_path = "/dev/null", .stderr_path = "/dev/null")) {
        return true;
    }

    nob_log(NOB_ERROR, "No C++ compiler found. Please install g++ or clang++");
    return false;
}

static inline bool create_required_directories(void) {
    for (size_t i = 0; i < build_config.required_directories_count; i++) {
        if (!nob_mkdir_if_not_exists(build_config.required_directories[i])) {
            nob_log(NOB_ERROR, "Could not create directory: %s",
                    build_config.required_directories[i]);
            return false;
        }
    }

    return true;
}

static inline bool download_dependencies(void) {
    nob_log(NOB_INFO, "No external dependencies to download.");
    return true;
}

#endif  // CJSH_NOB_DEPENDENCIES_H

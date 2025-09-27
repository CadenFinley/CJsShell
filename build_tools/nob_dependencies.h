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
    if (nob_cmd_run(&cmd, .stdout_path = "/dev/null",
                    .stderr_path = "/dev/null")) {
        return true;
    }

    cmd.count = 0;
    nob_cmd_append(&cmd, "which", "clang++");
    if (nob_cmd_run(&cmd, .stdout_path = "/dev/null",
                    .stderr_path = "/dev/null")) {
        return true;
    }

    nob_log(NOB_ERROR, "No C++ compiler found. Please install g++ or clang++");
    return false;
}

static inline bool create_required_directories(void) {
    nob_log(NOB_INFO, "Creating required directories...");

    for (size_t i = 0; i < build_config.required_directories_count; i++) {
        if (!nob_mkdir_if_not_exists(build_config.required_directories[i])) {
            nob_log(NOB_ERROR, "Could not create directory: %s",
                    build_config.required_directories[i]);
            return false;
        }
        nob_log(NOB_INFO, "Created directory: %s",
                build_config.required_directories[i]);
    }

    return true;
}

static inline bool download_dependencies(void) {
    nob_log(NOB_INFO, "Checking external dependencies...");

    const char* json_header_path = build_config.external_dependencies[0];
    if (nob_get_file_type(json_header_path) != NOB_FILE_REGULAR) {
        nob_log(NOB_INFO, "Downloading nlohmann/json...");

        const char* json_url = build_config.dependency_urls[0];
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "curl", "-L", "-o", json_header_path, json_url);
        if (!nob_cmd_run(&cmd)) {
            nob_log(NOB_WARNING,
                    "Failed to download with curl, trying wget...");
            cmd.count = 0;
            nob_cmd_append(&cmd, "wget", "-O", json_header_path, json_url);
            if (!nob_cmd_run(&cmd)) {
                nob_log(NOB_ERROR,
                        "Failed to download nlohmann/json. Please download "
                        "manually or install system package.");
                return false;
            }
        }
        nob_log(NOB_INFO, "Downloaded nlohmann/json successfully");
    }

    bool need_download = true;
    if (nob_get_file_type("build/vendor/utf8proc") == NOB_FILE_DIRECTORY) {
        if (nob_get_file_type("build/vendor/utf8proc/Makefile") ==
            NOB_FILE_REGULAR) {
            need_download = false;
        } else {
            nob_log(NOB_INFO, "Removing empty utf8proc directory...");
            Nob_Cmd cleanup_cmd = {0};
            nob_cmd_append(&cleanup_cmd, "rm", "-rf", "build/vendor/utf8proc");
            nob_cmd_run(&cleanup_cmd);
        }
    }

    if (need_download) {
        nob_log(NOB_INFO, "Downloading utf8proc...");

        const char* utf8proc_url = build_config.dependency_urls[1];
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "git", "clone", "--depth", "1", "--branch",
                       "v2.10.0", utf8proc_url, "build/vendor/utf8proc");
        if (!nob_cmd_run(&cmd)) {
            nob_log(NOB_ERROR, "Failed to download utf8proc");
            return false;
        }

        if (nob_get_file_type("build/vendor/utf8proc/Makefile") !=
            NOB_FILE_REGULAR) {
            nob_log(
                NOB_ERROR,
                "utf8proc download appears to have failed - no Makefile found");
            return false;
        }
    }

    const char* utf8proc_lib_path = build_config.external_dependencies[1];
    if (nob_get_file_type(utf8proc_lib_path) != NOB_FILE_REGULAR) {
        nob_log(NOB_INFO, "Building utf8proc from source...");

        const char* old_cwd = nob_get_current_dir_temp();
        if (!nob_set_current_dir("build/vendor/utf8proc")) {
            nob_log(NOB_ERROR, "Could not enter utf8proc directory");
            return false;
        }

        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "make", "-j");
        if (!nob_cmd_run(&cmd)) {
            nob_log(NOB_ERROR, "Failed to build utf8proc");
            nob_set_current_dir(old_cwd);
            return false;
        }

        nob_set_current_dir(old_cwd);
        nob_log(NOB_INFO, "Built utf8proc successfully");
    } else {
        nob_log(NOB_INFO, "utf8proc already built");
    }

    return true;
}

#endif  // CJSH_NOB_DEPENDENCIES_H

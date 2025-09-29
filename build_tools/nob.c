#define NOB_IMPLEMENTATION
#define NOB_EXPERIMENTAL_DELETE_OLD
#include "nob.h"

#define PROJECT_NAME "cjsh"
#define VERSION "3.5.10"

#include <stdbool.h>
#include <string.h>

#include "nob_cli.h"
#include "nob_compile.h"
#include "nob_dependencies.h"
#include "nob_platform.h"

int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF_PLUS(
        argc, argv, "nob.h", "nob_build_config.h", "nob_cli.h", "nob_compile.h",
        "nob_dependencies.h", "nob_platform.h", "nob_progress.h",
        "nob_sources.h", "nob_toolchain.h", "nob_types.h");

    // Change to parent directory (project root)
    if (!nob_set_current_dir("..")) {
        nob_log(NOB_ERROR, "Could not change to parent directory");
        return 1;
    }

    // Parse command line arguments
    bool help = false;
    bool version = false;
    bool clean = false;
    bool debug = false;
    bool force_32bit = false;
    bool dependencies = false;
    int override_jobs = -1;  // -1 means use automatic calculation

    // Skip the program name
    nob_shift_args(&argc, &argv);

    while (argc > 0) {
        char* arg = nob_shift_args(&argc, &argv);
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            help = true;
        } else if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
            version = true;
        } else if (strcmp(arg, "--clean") == 0) {
            clean = true;
        } else if (strcmp(arg, "--debug") == 0) {
            debug = true;
        } else if (strcmp(arg, "--force-32bit") == 0) {
            force_32bit = true;
        } else if (strcmp(arg, "--dependencies") == 0) {
            dependencies = true;
        } else if (strcmp(arg, "--jobs") == 0 || strcmp(arg, "-j") == 0) {
            if (argc == 0) {
                nob_log(NOB_ERROR, "Expected number after %s", arg);
                print_help();
                return 1;
            }
            char* jobs_str = nob_shift_args(&argc, &argv);
            override_jobs = atoi(jobs_str);
            if (override_jobs < 1) {
                nob_log(NOB_ERROR, "Invalid number of jobs: %s (must be >= 1)",
                        jobs_str);
                return 1;
            }
        } else {
            nob_log(NOB_ERROR, "Unknown argument: %s", arg);
            print_help();
            return 1;
        }
    }

    (void)debug;
    (void)force_32bit;

    if (help) {
        print_help();
        return 0;
    }

    if (version) {
        print_version();
        return 0;
    }

    if (dependencies) {
        print_dependencies();
        return 0;
    }

    if (clean) {
        nob_log(NOB_INFO, "Cleaning build directory...");
        // Remove build directory if it exists
        if (nob_get_file_type("build") == NOB_FILE_DIRECTORY) {
            Nob_Cmd cmd = {0};
            nob_cmd_append(&cmd, "rm", "-rf", "build");
            if (!nob_cmd_run(&cmd)) {
                nob_log(NOB_ERROR, "Failed to clean build directory");
                return 1;
            }
        }
        nob_log(NOB_INFO, "Clean complete");
        return 0;
    }

    // Check dependencies
    if (!check_dependencies()) {
        nob_log(NOB_ERROR, "Dependency check failed");
        return 1;
    }

    // Create required directories first
    if (!create_required_directories()) {
        nob_log(NOB_ERROR, "Failed to create required directories");
        return 1;
    }

    // Download external dependencies if needed
    if (!download_dependencies()) {
        nob_log(NOB_ERROR, "Failed to download dependencies");
        return 1;
    }

    // Compile the project
    if (!compile_cjsh(override_jobs)) {
        nob_log(NOB_ERROR, "Compilation failed");
        return 1;
    }

    nob_log(NOB_INFO, "Build completed successfully!");
    nob_log(NOB_INFO, "Output binary: build/" PROJECT_NAME);

    return 0;
}
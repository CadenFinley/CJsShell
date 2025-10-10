#define NOB_IMPLEMENTATION
#define NOB_EXPERIMENTAL_DELETE_OLD
#include "nob.h"

#define PROJECT_NAME "cjsh"
#define VERSION "3.9.2"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "nob_cli.h"
#include "nob_compile.h"
#include "nob_dependencies.h"

#define NOB_SELF_REBUILD_ENV "NOB_JUST_REBUILT"

bool g_debug_build = false;
bool g_minimal_build = false;

static const char* nob_self_rebuild_sources[] = {
    __FILE__,         "nob.h",         "nob_build_config.h",
    "nob_cli.h",      "nob_compile.h", "nob_dependencies.h",
    "nob_platform.h", "nob_sources.h", "nob_toolchain.h",
    "nob_types.h"};

static const size_t nob_self_rebuild_source_count =
    sizeof(nob_self_rebuild_sources) / sizeof(nob_self_rebuild_sources[0]);

static void nob_set_self_rebuild_env(bool value) {
#ifdef _WIN32
    if (value) {
        _putenv(NOB_SELF_REBUILD_ENV "=1");
    } else {
        _putenv(NOB_SELF_REBUILD_ENV "=");
    }
#else
    if (value) {
        setenv(NOB_SELF_REBUILD_ENV, "1", 1);
    } else {
        unsetenv(NOB_SELF_REBUILD_ENV);
    }
#endif
}

static bool nob_consume_self_rebuild_env(void) {
    const char* value = getenv(NOB_SELF_REBUILD_ENV);
    if (value != NULL && value[0] != '\0') {
        nob_set_self_rebuild_env(false);
        return true;
    }
    return false;
}

static void nob_mark_self_rebuild_if_needed(int argc, char** argv) {
    if (argc <= 0 || argv == NULL || argv[0] == NULL) {
        return;
    }

    const char* binary_path = argv[0];
#ifdef _WIN32
    if (!nob_sv_end_with(nob_sv_from_cstr(binary_path), ".exe")) {
        binary_path = nob_temp_sprintf("%s.exe", binary_path);
    }
#endif

    int rebuild =
        nob_needs_rebuild(binary_path, nob_self_rebuild_sources, nob_self_rebuild_source_count);
    if (rebuild < 0) {
        nob_log(NOB_ERROR, "Could not determine whether %s needs rebuild", binary_path);
        return;
    }
    if (rebuild > 0) {
        nob_set_self_rebuild_env(true);
    }
}

bool clean(void) {
    if (nob_get_file_type("build") == NOB_FILE_DIRECTORY) {
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "rm", "-rf", "build");
        if (!nob_cmd_run(&cmd)) {
            nob_log(NOB_ERROR, "Failed to clean build directory");
            return false;
        }
    }

    if (nob_get_file_type("compile_commands.json") == NOB_FILE_REGULAR) {
        if (remove("compile_commands.json") != 0) {
            nob_log(NOB_WARNING, "Failed to delete compile_commands.json");
        } else {
            nob_log(NOB_INFO, "Deleted compile_commands.json");
        }
    }

    return true;
}

int main(int argc, char** argv) {
    bool auto_clean = nob_consume_self_rebuild_env();
    nob_mark_self_rebuild_if_needed(argc, argv);

    NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "nob.h", "nob_build_config.h", "nob_cli.h",
                               "nob_compile.h", "nob_dependencies.h", "nob_platform.h",
                               "nob_sources.h", "nob_toolchain.h", "nob_types.h");

    if (!nob_set_current_dir("../..")) {
        nob_log(NOB_ERROR, "Could not change to parent directory");
        return 1;
    }

    bool help = false;
    bool version = false;
    bool clean_requested = auto_clean;
    bool debug = false;
    bool minimal = false;
    bool force_32bit = false;
    bool dependencies = false;
    bool generate_compile_commands = true;
    int override_jobs = -1;

    nob_shift_args(&argc, &argv);

    while (argc > 0) {
        char* arg = nob_shift_args(&argc, &argv);
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            help = true;
        } else if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
            version = true;
        } else if (strcmp(arg, "--clean") == 0) {
            clean_requested = true;
        } else if (strcmp(arg, "--debug") == 0) {
            debug = true;
        } else if (strcmp(arg, "--minimal") == 0) {
            minimal = true;
        } else if (strcmp(arg, "--force-32bit") == 0) {
            force_32bit = true;
        } else if (strcmp(arg, "--dependencies") == 0) {
            dependencies = true;
        } else if (strcmp(arg, "--no-compile-commands") == 0) {
            generate_compile_commands = false;
        } else if (strcmp(arg, "--jobs") == 0 || strcmp(arg, "-j") == 0) {
            if (argc == 0) {
                nob_log(NOB_ERROR, "Expected number after %s", arg);
                print_help();
                return 1;
            }
            char* jobs_str = nob_shift_args(&argc, &argv);
            override_jobs = atoi(jobs_str);
            if (override_jobs < 1) {
                nob_log(NOB_ERROR, "Invalid number of jobs: %s (must be >= 1)", jobs_str);
                return 1;
            }
        } else {
            nob_log(NOB_ERROR, "Unknown argument: %s", arg);
            print_help();
            return 1;
        }
    }

    g_debug_build = debug;
    g_minimal_build = minimal;

    if (g_debug_build) {
        nob_log(NOB_INFO,
                "Debug build requested: compiling without optimizations and with AddressSanitizer"
                " enabled");
    } else if (g_minimal_build) {
        nob_log(NOB_INFO,
                "Minimal build requested: compiling with ultra-aggressive size optimizations");
    }
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

    if (clean_requested && !clean()) {
        return 1;
    }

    if (!check_dependencies()) {
        nob_log(NOB_ERROR, "Dependency check failed");
        return 1;
    }

    if (!create_required_directories()) {
        nob_log(NOB_ERROR, "Failed to create required directories");
        return 1;
    }

    if (!download_dependencies()) {
        nob_log(NOB_ERROR, "Failed to download dependencies");
        return 1;
    }

    if (!compile_cjsh(override_jobs, generate_compile_commands)) {
        nob_log(NOB_ERROR, "Compilation failed");
        return 1;
    }

    nob_log(NOB_INFO, "Build completed successfully!");
    nob_log(NOB_INFO, "Output binary: build/" PROJECT_NAME);

    return 0;
}
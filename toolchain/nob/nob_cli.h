#ifndef CJSH_NOB_CLI_H
#define CJSH_NOB_CLI_H

#include <stdio.h>
#include <string.h>

#include "nob_build_config.h"
#include "nob_platform.h"

static inline void print_help(void) {
    printf("CJ's Shell Build System (nob)\n");
    printf("Usage: nob [OPTIONS]\n\n");
    printf("OPTIONS:\n");
    printf("  -h, --help        Show this help message\n");
    printf("  -v, --version     Show version information\n");
    printf("  --clean           Clean build directory\n");
    printf("  --debug           Build with debug symbols\n");
    printf("  --force-32bit     Force 32-bit build (if supported)\n");
    printf("  --dependencies    List project dependencies\n");
    printf(
        "  -j, --jobs N      Override parallel compilation jobs (default: "
        "auto)\n\n");
    printf("Examples:\n");
    printf("  nob                # Build the project (auto parallel jobs)\n");
    printf("  nob --clean        # Clean build files\n");
    printf("  nob --debug        # Build with debug info\n");
    printf("  nob -j 1           # Build with sequential compilation\n");
    printf("  nob -j 4           # Build with 4 parallel jobs\n");
}

static inline void print_version(void) {
    printf("CJ's Shell Build System\n");
    printf("Project: %s\n", PROJECT_NAME);
    printf("Version: %s\n", VERSION);
    printf("Built with nob.h\n");
}

static inline void print_dependencies(void) {
    printf("CJ's Shell Dependencies\n");
    printf("======================\n\n");

    printf("Build Dependencies:\n");
    printf("  - C++ compiler (g++ or clang++)\n");
    printf("  - C compiler (gcc or clang)\n");
    printf("  - make (for building utf8proc)\n");
    printf("  - git (for downloading dependencies)\n");
    printf("  - curl or wget (for downloading files)\n\n");

    printf("Runtime Dependencies (automatically downloaded):\n");
    for (size_t i = 0; i < build_config.external_dependencies_count; i++) {
        const char* dep = build_config.external_dependencies[i];
        if (strstr(dep, "json.hpp")) {
            printf("  - nlohmann/json v3.11.3 (JSON parsing library)\n");
            printf("    URL: https://github.com/nlohmann/json\n");
        } else if (strstr(dep, "utf8proc")) {
            printf("  - utf8proc v2.10.0 (Unicode text processing library)\n");
            printf("    URL: https://github.com/JuliaStrings/utf8proc\n");
        }
    }

    printf("\nSystem Libraries (linked at build time):\n");
    printf("  - pthread (POSIX threads)\n");
    printf("  - C++ standard library\n");
#if defined(PLATFORM_LINUX) || defined(PLATFORM_UNIX)
    printf("  - dl (dynamic loading)\n");
#endif

    printf("\nNote: This build system downloads and builds all external\n");
    printf("dependencies from source for maximum compatibility.\n");
    printf("No system package manager dependencies are required.\n");
}

#endif  // CJSH_NOB_CLI_H

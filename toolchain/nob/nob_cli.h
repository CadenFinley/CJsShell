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
    printf("  - git (for cloning the repository)\n");
    printf("  - curl or wget (for downloading files)\n\n");

    printf("Runtime Dependencies:\n");
    printf("  - None (all functionality built-in)\n");

    printf("\nSystem Libraries (linked at build time):\n");
    printf("  - pthread (POSIX threads)\n");
    printf("  - C++ standard library\n");
#if defined(PLATFORM_LINUX) || defined(PLATFORM_UNIX)
    printf("  - dl (dynamic loading)\n");
#endif
    printf("\nNote: This build system has no external library downloads\n");
    printf("and builds all components from the source tree.\n");
}

#endif  // CJSH_NOB_CLI_H

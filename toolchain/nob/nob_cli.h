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
    printf("  --minimal         Build with ultra-aggressive size optimizations\n");
    printf("  --force-32bit     Force 32-bit build (if supported)\n");
    printf("  --no-compile-commands\n");
    printf("  --asm             Generate assembly files in build/asm\n");
    printf("  --asm-readable    Generate readable assembly with comments/labels\n");
    printf(
        "  -j, --jobs N      Override parallel compilation jobs (default: "
        "auto)\n\n");
    printf("Examples:\n");
    printf("  nob                # Build the project (auto parallel jobs)\n");
    printf("  nob --clean        # Clean build files\n");
    printf("  nob --debug        # Build with debug info\n");
    printf("  nob --minimal      # Build with ultra-small memory footprint\n");
    printf("  nob -j 1           # Build with sequential compilation\n");
    printf("  nob -j 4           # Build with 4 parallel jobs\n");
}

static inline void print_version(void) {
    printf("CJ's Shell Build System\n");
    printf("Project: %s\n", PROJECT_NAME);
    printf("Version: %s\n", VERSION);
    printf("Built with nob.h\n");
}

#endif

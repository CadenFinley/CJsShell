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
    printf("  --clean           Clean build directory\n");
    printf("  --debug           Build with debug symbols\n");
    printf("  --minimal         Build with ultra-aggressive size optimizations\n");
    printf("  --no-compile-commands\n");
    printf("  --asm             Generate assembly files in build/asm\n");
    printf("  --asm-readable    Generate readable assembly with comments/labels\n");
    printf(
        "  -j, --jobs N      Override parallel compilation jobs (default: "
        "auto)\n\n");
}

#endif

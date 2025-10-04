#ifndef CJSH_NOB_TOOLCHAIN_H
#define CJSH_NOB_TOOLCHAIN_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "nob_build_config.h"
#include "nob_platform.h"

static const char* cached_cxx_compiler = NULL;
static const char* cached_c_compiler = NULL;
static const char* cached_linker = NULL;

extern bool g_debug_build;

static char git_hash_define[128] = "-DCJSH_GIT_HASH=\"unknown\"";

static inline void nob_set_git_hash_define(const char* hash) {
    if (hash != NULL && hash[0] != '\0') {
        snprintf(git_hash_define, sizeof(git_hash_define), "-DCJSH_GIT_HASH=\"%s\"", hash);
    } else {
        snprintf(git_hash_define, sizeof(git_hash_define), "-DCJSH_GIT_HASH=\"unknown\"");
    }
}

static inline const char* get_cxx_compiler(void) {
    if (cached_cxx_compiler != NULL) {
        return cached_cxx_compiler;
    }

    Nob_Cmd test_cmd = {0};
    nob_cmd_append(&test_cmd, "which", "g++");
    if (nob_cmd_run(&test_cmd, .stdout_path = "/dev/null", .stderr_path = "/dev/null")) {
        cached_cxx_compiler = "g++";
    } else {
        cached_cxx_compiler = "clang++";
    }
    return cached_cxx_compiler;
}

static inline const char* get_c_compiler(void) {
    if (cached_c_compiler != NULL) {
        return cached_c_compiler;
    }

    Nob_Cmd test_cmd = {0};
    nob_cmd_append(&test_cmd, "which", "gcc");
    if (nob_cmd_run(&test_cmd, .stdout_path = "/dev/null", .stderr_path = "/dev/null")) {
        cached_c_compiler = "gcc";
    } else {
        cached_c_compiler = "clang";
    }
    return cached_c_compiler;
}

static inline const char* get_linker(void) {
    if (cached_linker != NULL) {
        return cached_linker;
    }

    Nob_Cmd test_cmd = {0};
    nob_cmd_append(&test_cmd, "which", "g++");
    if (nob_cmd_run(&test_cmd, .stdout_path = "/dev/null", .stderr_path = "/dev/null")) {
        cached_linker = "g++";
    } else {
        cached_linker = "clang++";
    }
    return cached_linker;
}

static inline bool setup_build_flags(Nob_Cmd* cmd) {
    const char* compiler = get_cxx_compiler();

    nob_cmd_append(cmd, compiler);
    nob_cmd_append(cmd, "-std=c++17", "-Wall", "-Wextra", "-Wpedantic");

#ifdef PLATFORM_MACOS
    if (strcmp(compiler, "clang++") == 0) {
        nob_cmd_append(cmd, "-stdlib=libc++");
    }
#ifdef ARCH_ARM64
    nob_cmd_append(cmd, "-arch", "arm64");
#elif defined(ARCH_X86_64)
    nob_cmd_append(cmd, "-arch", "x86_64");
#endif
#endif

#ifdef PLATFORM_LINUX
    if (!g_debug_build) {
        nob_cmd_append(cmd, "-static-libgcc", "-static-libstdc++");
    }
#endif

    if (g_debug_build) {
        nob_cmd_append(cmd, "-O0", "-g", "-fno-omit-frame-pointer");
        nob_cmd_append(cmd, "-fsanitize=address");
        nob_cmd_append(cmd, "-DDEBUG");
    } else {
        nob_cmd_append(cmd, "-O2", "-DNDEBUG");
        nob_cmd_append(cmd, "-ffunction-sections", "-fdata-sections", "-flto");
    }

    nob_cmd_append(cmd, "-DIC_SEPARATE_OBJS=1");
    nob_cmd_append(cmd, "-DJSON_NOEXCEPTION=1");
    nob_cmd_append(cmd, "-DJSON_USE_IMPLICIT_CONVERSIONS=1");

    // Define architecture and platform for runtime use
#ifdef ARCH_ARM64
    nob_cmd_append(cmd, "-DCJSH_BUILD_ARCH=\"arm64\"");
#elif defined(ARCH_X86_64)
    nob_cmd_append(cmd, "-DCJSH_BUILD_ARCH=\"x86_64\"");
#elif defined(ARCH_X86)
    nob_cmd_append(cmd, "-DCJSH_BUILD_ARCH=\"x86\"");
#else
    nob_cmd_append(cmd, "-DCJSH_BUILD_ARCH=\"unknown\"");
#endif

#ifdef PLATFORM_MACOS
    nob_cmd_append(cmd, "-DCJSH_BUILD_PLATFORM=\"apple-darwin\"");
#elif defined(PLATFORM_LINUX)
    nob_cmd_append(cmd, "-DCJSH_BUILD_PLATFORM=\"linux\"");
#elif defined(PLATFORM_WINDOWS)
    nob_cmd_append(cmd, "-DCJSH_BUILD_PLATFORM=\"windows\"");
#else
    nob_cmd_append(cmd, "-DCJSH_BUILD_PLATFORM=\"unix\"");
#endif

    for (size_t i = 0; i < build_config.include_directories_count; i++) {
        nob_cmd_append(cmd, "-I", build_config.include_directories[i]);
    }

    nob_cmd_append(cmd, git_hash_define);

    return true;
}

static inline bool setup_c_build_flags(Nob_Cmd* cmd) {
    const char* c_compiler = get_c_compiler();
    nob_cmd_append(cmd, c_compiler);

    nob_cmd_append(cmd, "-std=c11", "-Wall", "-Wno-error", "-Wno-unused-function",
                   "-Wno-unused-variable");

#ifdef PLATFORM_MACOS
#ifdef ARCH_ARM64
    nob_cmd_append(cmd, "-arch", "arm64");
#elif defined(ARCH_X86_64)
    nob_cmd_append(cmd, "-arch", "x86_64");
#endif
#endif

    if (g_debug_build) {
        nob_cmd_append(cmd, "-O0", "-g", "-fno-omit-frame-pointer");
        nob_cmd_append(cmd, "-fsanitize=address");
        nob_cmd_append(cmd, "-DDEBUG");
    } else {
        nob_cmd_append(cmd, "-O2", "-DNDEBUG");
        nob_cmd_append(cmd, "-ffunction-sections", "-fdata-sections", "-flto");
    }
    nob_cmd_append(cmd, "-DIC_SEPARATE_OBJS=1");

    // Define architecture and platform for runtime use
#ifdef ARCH_ARM64
    nob_cmd_append(cmd, "-DCJSH_BUILD_ARCH=\"arm64\"");
#elif defined(ARCH_X86_64)
    nob_cmd_append(cmd, "-DCJSH_BUILD_ARCH=\"x86_64\"");
#elif defined(ARCH_X86)
    nob_cmd_append(cmd, "-DCJSH_BUILD_ARCH=\"x86\"");
#else
    nob_cmd_append(cmd, "-DCJSH_BUILD_ARCH=\"unknown\"");
#endif

#ifdef PLATFORM_MACOS
    nob_cmd_append(cmd, "-DCJSH_BUILD_PLATFORM=\"apple-darwin\"");
#elif defined(PLATFORM_LINUX)
    nob_cmd_append(cmd, "-DCJSH_BUILD_PLATFORM=\"linux\"");
#elif defined(PLATFORM_WINDOWS)
    nob_cmd_append(cmd, "-DCJSH_BUILD_PLATFORM=\"windows\"");
#else
    nob_cmd_append(cmd, "-DCJSH_BUILD_PLATFORM=\"unix\"");
#endif

    for (size_t i = 0; i < build_config.c_include_directories_count; i++) {
        nob_cmd_append(cmd, "-I", build_config.c_include_directories[i]);
    }

    nob_cmd_append(cmd, git_hash_define);

    return true;
}

#endif  // CJSH_NOB_TOOLCHAIN_H

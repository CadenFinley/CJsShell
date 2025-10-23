#include "version_command.h"

#include "builtin_help.h"

#include <cstdio>
#include <string>

#include "cjsh.h"

int version_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(
            args, {"Usage: version", "Display cjsh version and build information.", "",
                   "Output format: cjsh v<VERSION> [<TAGS>] (git <HASH>) (<ARCH>-<PLATFORM>)", "",
                   "  VERSION  - The semantic version number (e.g., 1.0.0)",
                   "  TAGS     - Build configuration flags (e.g., (debug) (pre-release))",
                   "  HASH     - Git commit hash used for the build",
                   "  ARCH     - Target architecture (e.g., x86_64, arm64)",
                   "  PLATFORM - Target platform (e.g., darwin, linux, windows)"})) {
        return 0;
    }

#ifndef CJSH_BUILD_ARCH
#define CJSH_BUILD_ARCH "unknown"
#endif
#ifndef CJSH_BUILD_PLATFORM
#define CJSH_BUILD_PLATFORM "unknown"
#endif

    const std::string version = get_version();

    std::string build_tags;
#ifdef CJSH_ENABLE_DEBUG
    build_tags += " (debug)";
#endif

    (void)std::fprintf(stdout, "cjsh v%s%s (git %s) (%s-%s)\n", version.c_str(), build_tags.c_str(),
                       CJSH_GIT_HASH, CJSH_BUILD_ARCH, CJSH_BUILD_PLATFORM);
    (void)std::fputs("Copyright (c) 2025 Caden Finley MIT License\n", stdout);
    return 0;
}

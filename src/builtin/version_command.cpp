/*
  version_command.cpp

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "version_command.h"

#include "builtin_help.h"

#include <cstdio>
#include <string>

const bool PRE_RELEASE = false;
const char* const c_version_base = "1.1.5";

std::string get_version() {
    static std::string cached_version =
        std::string(c_version_base) + (PRE_RELEASE ? " (pre-release)" : "");
    return cached_version;
}

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

#ifndef CJSH_GIT_HASH
#define CJSH_GIT_HASH "unknown"
#endif

    const std::string version = get_version();

    std::string build_tags;
#ifdef CJSH_ENABLE_DEBUG
    build_tags += " (debug)";
#endif

    (void)std::fprintf(stdout, "cjsh v%s%s (git %s) (%s-%s)\n", version.c_str(), build_tags.c_str(),
                       CJSH_GIT_HASH, CJSH_BUILD_ARCH, CJSH_BUILD_PLATFORM);
    (void)std::fputs("Copyright (c) 2026 Caden Finley MIT License\n", stdout);
    return 0;
}

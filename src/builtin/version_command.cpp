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

const bool PRE_RELEASE = true;
const char* const c_version_base = "1.1.6";

std::string get_version() {
    static std::string cached_version =
        std::string(c_version_base) + (PRE_RELEASE ? " (pre-release)" : "");
    return cached_version;
}

int version_command(const std::vector<std::string>& args) {
    const std::vector<std::string> help_lines = {
        "Usage: version [OPTIONS]",
        "Display cjsh version and build information.",
        "",
        "  -a, --all         Show extended build details",
        "  --tag             Print version tag (vX.Y.Z)",
        "  --build-time      Print build timestamp (UTC)",
        "  --compiler        Print compiler and version",
        "  --cpp-standard    Print C++ standard level",
        "  --cxx-standard    Alias for --cpp-standard",
        "  --git-hash        Print short git hash",
        "  --git-hash-full   Print full git hash",
        "  --build-type      Print build configuration",
        "  --arch            Print target architecture",
        "  --platform        Print target platform",
        "",
        "Output format: cjsh v<VERSION> [<TAGS>] (git <HASH>) (<ARCH>-<PLATFORM>)",
        "",
        "  VERSION  - The semantic version number (e.g., 1.0.0)",
        "  TAGS     - Build configuration flags (e.g., (debug) (pre-release))",
        "  HASH     - Git commit hash used for the build",
        "  ARCH     - Target architecture (e.g., x86_64, arm64)",
        "  PLATFORM - Target platform (e.g., darwin, linux, windows)",
        "",
        "Extended details include build time, compiler, C++ standard, build type, and full git "
        "hash.",
        "Field flags can be combined to print only selected metadata."};

    if (builtin_handle_help(args, help_lines)) {
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

#ifndef CJSH_GIT_HASH_FULL
#define CJSH_GIT_HASH_FULL "unknown"
#endif

#ifndef CJSH_BUILD_TIME
#define CJSH_BUILD_TIME "unknown"
#endif

#ifndef CJSH_BUILD_COMPILER
#define CJSH_BUILD_COMPILER "unknown"
#endif

#ifndef CJSH_CXX_STANDARD
#define CJSH_CXX_STANDARD "unknown"
#endif

#ifndef CJSH_BUILD_TYPE
#define CJSH_BUILD_TYPE "unknown"
#endif

    bool show_all = false;
    bool show_fields = false;
    bool show_tag = false;
    bool show_build_time = false;
    bool show_compiler = false;
    bool show_cpp_standard = false;
    bool show_git_hash = false;
    bool show_git_hash_full = false;
    bool show_build_type = false;
    bool show_arch = false;
    bool show_platform = false;
    auto enable_field = [&](bool& flag) {
        flag = true;
        show_fields = true;
    };
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "-a" || arg == "--all") {
            show_all = true;
        } else if (arg == "--tag") {
            show_tag = true;
            show_fields = true;
        } else if (arg == "--build-time") {
            enable_field(show_build_time);
        } else if (arg == "--compiler") {
            enable_field(show_compiler);
        } else if (arg == "--cpp-standard" || arg == "--cxx-standard") {
            enable_field(show_cpp_standard);
        } else if (arg == "--git-hash") {
            enable_field(show_git_hash);
        } else if (arg == "--git-hash-full") {
            enable_field(show_git_hash_full);
        } else if (arg == "--build-type") {
            enable_field(show_build_type);
        } else if (arg == "--arch") {
            enable_field(show_arch);
        } else if (arg == "--platform") {
            enable_field(show_platform);
        } else if (arg == "--") {
            break;
        }
    }

    const std::string version = get_version();

    std::string build_tags;
#ifdef CJSH_ENABLE_DEBUG
    build_tags += " (debug)";
#endif

    if (show_fields && !show_all) {
        int field_count = 0;
        field_count += show_tag ? 1 : 0;
        field_count += show_build_time ? 1 : 0;
        field_count += show_compiler ? 1 : 0;
        field_count += show_cpp_standard ? 1 : 0;
        field_count += show_git_hash ? 1 : 0;
        field_count += show_git_hash_full ? 1 : 0;
        field_count += show_build_type ? 1 : 0;
        field_count += show_arch ? 1 : 0;
        field_count += show_platform ? 1 : 0;

        const std::string tag = std::string("v") + c_version_base;
        if (show_tag && field_count == 1) {
            (void)std::fprintf(stdout, "%s\n", tag.c_str());
            return 0;
        }

        auto print_value = [&](const std::string& value) {
            (void)std::fprintf(stdout, "%s\n", value.c_str());
        };

        if (show_tag) {
            print_value(tag);
        }
        if (show_build_time) {
            print_value(CJSH_BUILD_TIME);
        }
        if (show_compiler) {
            print_value(CJSH_BUILD_COMPILER);
        }
        if (show_cpp_standard) {
            print_value(std::string("C++") + CJSH_CXX_STANDARD);
        }
        if (show_git_hash) {
            print_value(CJSH_GIT_HASH);
        }
        if (show_git_hash_full) {
            print_value(CJSH_GIT_HASH_FULL);
        }
        if (show_build_type) {
            print_value(CJSH_BUILD_TYPE);
        }
        if (show_arch) {
            print_value(CJSH_BUILD_ARCH);
        }
        if (show_platform) {
            print_value(CJSH_BUILD_PLATFORM);
        }
        return 0;
    }

    (void)std::fprintf(stdout, "cjsh v%s%s (git %s) (%s-%s)\n", version.c_str(), build_tags.c_str(),
                       CJSH_GIT_HASH, CJSH_BUILD_ARCH, CJSH_BUILD_PLATFORM);
    (void)std::fputs("Copyright (c) 2026 Caden Finley MIT License\n", stdout);
    if (show_all) {
        (void)std::fputs("Build details:\n", stdout);
        (void)std::fprintf(stdout, "  Build time: %s\n", CJSH_BUILD_TIME);
        (void)std::fprintf(stdout, "  Git hash: %s\n", CJSH_GIT_HASH_FULL);
        (void)std::fprintf(stdout, "  Compiler: %s\n", CJSH_BUILD_COMPILER);
        (void)std::fprintf(stdout, "  C++ standard: C++%s\n", CJSH_CXX_STANDARD);
        (void)std::fprintf(stdout, "  Build type: %s\n", CJSH_BUILD_TYPE);
    }
    return 0;
}

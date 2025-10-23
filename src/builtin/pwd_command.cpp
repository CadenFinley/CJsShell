// pwd - print name of current/working directory
// Copyright (C) 1994-2025 Free Software Foundation, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// Author: Jim Meyering

#include "pwd_command.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <system_error>

#include "builtin_help.h"
#include "error_out.h"

namespace {

// Check if two inodes are the same
inline bool same_inode(const struct stat& st1, const struct stat& st2) {
    return st1.st_ino == st2.st_ino && st1.st_dev == st2.st_dev;
}

// Return PWD from environment if it's valid for logical mode
char* logical_getcwd() {
    const char* wd = std::getenv("PWD");

    // Textual validation: must start with /
    if (!wd || wd[0] != '/') {
        return nullptr;
    }

    // Check for /. or /.. components which indicate non-canonical path
    const char* p = wd;
    while ((p = std::strstr(p, "/."))) {
        // Found /. - check what follows
        if (!p[2] || p[2] == '/' ||                     // ends with /. or has /./
            (p[2] == '.' && (!p[3] || p[3] == '/'))) {  // has /../
            return nullptr;
        }
        p++;
    }

    // System call validation: verify PWD actually refers to current directory
    struct stat st1, st2;
    if (stat(wd, &st1) == 0 && stat(".", &st2) == 0 && same_inode(st1, st2)) {
        return const_cast<char*>(wd);
    }

    return nullptr;
}

}  // namespace

int pwd_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(
            args,
            {"Usage: pwd [OPTION]...", "Print the full filename of the current working directory.",
             "", "  -L, --logical   use PWD from environment, even if it contains symlinks",
             "  -P, --physical  avoid all symlinks (default)", "",
             "If no option is specified, -P is assumed.", "",
             "NOTE: your shell may have its own version of pwd, which usually supersedes",
             "the version described here. Please refer to your shell's documentation",
             "for details about the options it supports."})) {
        return 0;
    }

    bool logical = false;  // Default to physical (-P) for standalone pwd

    // Check if POSIXLY_CORRECT is set (would default to -L)
    if (std::getenv("POSIXLY_CORRECT") != nullptr) {
        logical = true;
    }

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (arg == "-L" || arg == "--logical") {
            logical = true;
        } else if (arg == "-P" || arg == "--physical") {
            logical = false;
        } else if (arg == "--version") {
            std::cout << "pwd (CJsShell coreutils)\n";
            return 0;
        } else if (arg == "--") {
            // End of options
            break;
        } else if (arg[0] == '-' && arg.length() > 1) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "pwd",
                         "invalid option -- '" + arg + "'",
                         {"Try 'pwd --help' for more information."}});
            return 1;
        } else {
            // Non-option argument (should be ignored per POSIX)
            std::cerr << "pwd: ignoring non-option arguments\n";
        }
    }

    std::string path;

    // Try logical mode first if requested
    if (logical) {
        char* wd = logical_getcwd();
        if (wd) {
            std::cout << wd << '\n';
            std::cout.flush();
            return 0;
        }
        // If logical PWD is not valid, fall through to physical mode
    }

    // Physical mode: use getcwd
    std::unique_ptr<char, decltype(&free)> cwd(getcwd(nullptr, 0), free);
    if (cwd) {
        std::cout << cwd.get() << '\n';
        std::cout.flush();
        return 0;
    }

    // getcwd failed
    const auto error_text = std::system_category().message(errno);
    print_error(
        {ErrorType::RUNTIME_ERROR, "pwd", "cannot determine current directory: " + error_text, {}});
    return 1;
}

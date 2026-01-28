#include "pwd_command.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>

#include "builtin_help.h"
#include "cjsh_filesystem.h"
#include "error_out.h"

namespace {

inline bool same_inode(const struct stat& st1, const struct stat& st2) {
    return st1.st_ino == st2.st_ino && st1.st_dev == st2.st_dev;
}

char* logical_getcwd() {
    const char* wd = std::getenv("PWD");

    if (!wd || wd[0] != '/') {
        return nullptr;
    }

    const char* p = wd;
    while ((p = std::strstr(p, "/."))) {
        if (!p[2] || p[2] == '/' || (p[2] == '.' && (!p[3] || p[3] == '/'))) {
            return nullptr;
        }
        p++;
    }

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

    bool logical = false;

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
            break;
        } else if (arg[0] == '-' && arg.length() > 1) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "pwd",
                         "invalid option -- '" + arg + "'",
                         {"Try 'pwd --help' for more information."}});
            return 1;
        } else {
            print_error({ErrorType::INVALID_ARGUMENT,
                         ErrorSeverity::WARNING,
                         "pwd",
                         "ignoring non-option arguments",
                         {"Use '--' to separate options from paths."}});
        }
    }

    std::string path;

    if (logical) {
        char* wd = logical_getcwd();
        if (wd) {
            std::cout << wd << '\n';
            std::cout.flush();
            return 0;
        }
    }

    std::unique_ptr<char, decltype(&free)> cwd(getcwd(nullptr, 0), free);
    if (cwd) {
        std::cout << cwd.get() << '\n';
        std::cout.flush();
        return 0;
    }

    std::string fallback = cjsh_filesystem::safe_current_directory();
    if (!fallback.empty()) {
        std::cout << fallback << '\n';
        std::cout.flush();
        return 0;
    }

    const auto error_text = std::system_category().message(errno);
    print_error(
        {ErrorType::RUNTIME_ERROR, "pwd", "cannot determine current directory: " + error_text, {}});
    return 1;
}

/*
  restart_command.cpp

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

#include "restart_command.h"

#include "builtin_help.h"

#include <unistd.h>
#include <cerrno>
#include <cstring>

#include "cjsh_filesystem.h"
#include "error_out.h"
#include "flags.h"
#include "shell_env.h"

int restart_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args,
                            {"Usage: restart [-n|--no-flags]", "Re-exec the current cjsh process.",
                             "By default, restart reuses the shell's original startup arguments.",
                             "Use --no-flags to relaunch as plain cjsh with no original startup "
                             "arguments."})) {
        return 0;
    }

    bool drop_original_flags = false;
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& token = args[i];
        if (token == "-n" || token == "--no-flags") {
            drop_original_flags = true;
            continue;
        }

        if (token == "--") {
            if (i + 1 < args.size()) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             "restart",
                             "unexpected argument: " + args[i + 1],
                             {"Usage: restart [-n|--no-flags]"}});
                return 2;
            }
            break;
        }

        if (!token.empty() && token.front() == '-') {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "restart",
                         "invalid option: " + token,
                         {"Usage: restart [-n|--no-flags]"}});
            return 2;
        }

        print_error({ErrorType::INVALID_ARGUMENT,
                     "restart",
                     "unexpected argument: " + token,
                     {"Usage: restart [-n|--no-flags]"}});
        return 2;
    }

    const auto& startup_args = flags::startup_args();

    std::string executable_path = cjsh_filesystem::resolve_cjsh_executable_path(startup_args);
    if (executable_path.empty()) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "restart",
                     "unable to locate the cjsh executable",
                     {"Ensure cjsh is available on PATH or set SHELL to the cjsh executable."}});
        return 1;
    }

    const std::string normalized_argv0 =
        cjsh_filesystem::resolve_cjsh_argv0(startup_args, executable_path);

    std::vector<std::string> exec_args = startup_args;
    if (exec_args.empty()) {
        exec_args = {normalized_argv0};
    }

    if (drop_original_flags) {
        exec_args = {normalized_argv0};
    }

    std::vector<char*> c_args = cjsh_env::build_exec_argv(exec_args);
    execvp(executable_path.c_str(), c_args.data());

    print_error({ErrorType::RUNTIME_ERROR,
                 "restart",
                 "failed to restart shell: " + std::string(std::strerror(errno)),
                 {"Resolved executable: " + executable_path}});
    return 1;
}

/*
  internal_subshell_command.cpp

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

#include "internal_subshell_command.h"

#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "error_out.h"
#include "shell.h"

int internal_subshell_command(const std::vector<std::string>& args, Shell* shell) {
    if (args.size() < 2) {
        return 1;
    }

    const std::string& subshell_content = args[1];

    pid_t pid = fork();
    if (pid == -1) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "subshell",
                     "fork failed: " + std::string(strerror(errno)),
                     {}});
        return 1;
    }

    if (pid == 0) {
        int exit_code = shell->execute(subshell_content, true);
        _exit(exit_code);
    } else {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "subshell",
                         "waitpid failed: " + std::string(strerror(errno)),
                         {}});
            return 1;
        }

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            return 128 + WTERMSIG(status);
        } else {
            return 1;
        }
    }
}

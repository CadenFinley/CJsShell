#include "internal_subshell_command.h"

#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

#include "error_out.h"
#include "shell.h"

int internal_subshell_command(const std::vector<std::string>& args,
                              Shell* shell) {
    if (args.size() < 2) {
        return 1;
    }

    std::string subshell_content = args[1];

    pid_t pid = fork();
    if (pid == -1) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "subshell",
                     "fork failed: " + std::string(strerror(errno)),
                     {}});
        return 1;
    }

    if (pid == 0) {
        int exit_code = shell->execute(subshell_content);
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

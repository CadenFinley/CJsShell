#include "internal_subshell_command.h"

#include <sys/wait.h>
#include <unistd.h>

#include <iostream>

#include "shell.h"

int internal_subshell_command(const std::vector<std::string>& args, Shell* shell) {
  if (args.size() < 2) {
    return 1;  // Usage error
  }

  std::string subshell_content = args[1];

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork failed in subshell");
    return 1;
  }

  if (pid == 0) {
    int exit_code = shell->execute(subshell_content);
    _exit(exit_code);
  } else {
    int status;
    if (waitpid(pid, &status, 0) == -1) {
      perror("waitpid failed in subshell");
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

#include "shell.h"
#include "exec.h"

Shell::Shell(pid_t pid, char *argv[]) {
  shell_prompt = new Prompt();
  shell_exec = new Exec(this);

  this->pid = pid;

  if (argv && argv[0] && argv[0][0] == '-') {
    login_mode = true;
  } else {
    login_mode = false;
  }
  
  // Initialize shell_terminal
  shell_terminal = STDIN_FILENO;
}

Shell::~Shell() {
  if (shell_prompt) {
    delete shell_prompt;
  }
  if (shell_exec) {
    delete shell_exec;
  }
}

void Shell::execute_command(std::string command, bool sync) {
  if (command.empty()) {
    return;
  }
  if (sync) {
    shell_exec->execute_command_sync(command);
  } else {
    shell_exec->execute_command_async(command);
  }
}

#include "shell.h"

Shell::Shell(pid_t pid, char *argv[]) {
  shell_prompt = std::make_unique<Prompt>();
  shell_exec = std::make_unique<Exec>(this);
  shell_parser = new Parser();

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
  // unique_ptr automatically handles deletion, no need for manual cleanup
}

void Shell::execute_command(std::string command, bool sync) {
  //since this is a custom shell be dont return bool we handle errors and error messages in the command execution process
  if (command.empty()) {
    return;
  }
  if (!shell_exec) {
    return;
  }
  if (sync) {
    shell_exec->execute_command_sync(command);
  } else {
    shell_exec->execute_command_async(command);
  }
}

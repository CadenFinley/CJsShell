#include "shell.h"

Shell::Shell(pid_t pid, char *argv[]) {
  shell_prompt = new Prompt();
  shell_exec = new Exec();

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

bool Shell::get_interactive_mode() {
  return interactive_mode;
}

bool Shell::get_login_mode() {
  return login_mode;
}

bool Shell::get_exit_flag() {
  return exit_flag;
}

void Shell::set_exit_flag(bool flag) {
  exit_flag = flag;
}

void Shell::set_interactive_mode(bool flag) {
  interactive_mode = flag;
}

void Shell::set_aliases(std::map<std::string, std::string> aliases) {
  this->aliases = aliases;
}

std::string Shell::get_prompt() {
  return shell_prompt->get_prompt();
}

std::string Shell::get_ai_prompt() {
  return shell_prompt->get_ai_prompt();
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

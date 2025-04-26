Shell::Shell(pid_t pid, int argc, char *argv[]) {
  shell_prompt = new prompt();
  shell_exec = new exec();

  this->pid = pid;

  if (argv0 && argv0[0] == '-') {
    login_mode = true;
  } else {
    login_mode = false;
  }
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

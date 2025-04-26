#include "shell.h"
#include "exec.h"

/**
 * @brief Constructs a Shell instance and initializes its components.
 *
 * Creates new Prompt and Exec objects, sets the shell's process ID, determines login mode based on the first argument, and initializes the shell terminal to standard input.
 *
 * @param pid The process ID to associate with the shell.
 * @param argv Argument vector; if the first argument starts with '-', the shell enters login mode.
 */
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

/**
 * @brief Destroys the Shell instance and releases associated resources.
 *
 * Deletes the dynamically allocated Prompt and Exec objects if they exist.
 */
Shell::~Shell() {
  if (shell_prompt) {
    delete shell_prompt;
  }
  if (shell_exec) {
    delete shell_exec;
  }
}

/**
 * @brief Executes a shell command either synchronously or asynchronously.
 *
 * If the command string is empty, the function returns immediately without execution.
 * Otherwise, the command is executed using the shell's execution engine in either synchronous or asynchronous mode, depending on the value of the `sync` flag.
 *
 * @param command The shell command to execute.
 * @param sync If true, executes the command synchronously; if false, executes asynchronously.
 */
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

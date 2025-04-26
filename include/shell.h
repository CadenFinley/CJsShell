#pragma once

#include "prompt.h"
#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <string>
#include <map>
#include <sys/types.h>

// Forward declaration
class Exec;

/**
 * @brief Manages shell state, command execution, and prompt handling.
 *
 * The Shell class encapsulates the core functionality of a shell environment, including command execution, prompt generation, and management of shell state flags such as exit, interactive, and login modes. It also supports setting command aliases and interfaces with external Prompt and Exec components.
 */

class Shell {
  public:
    Shell(pid_t pid, char *argv[]);
    ~Shell();

    void execute_command(std::string command, bool sync = false);

    std::string get_prompt() {
      return shell_prompt->get_prompt();
    }

    std::string get_ai_prompt() {
      return shell_prompt->get_ai_prompt();
    }

    /**
     * @brief Sets the shell's exit flag.
     *
     * Controls whether the shell should terminate its main loop.
     *
     * @param flag If true, signals the shell to exit.
     */
    void set_exit_flag(bool flag) {
      exit_flag = flag;
    }

    /**
     * @brief Returns whether the shell is set to exit.
     *
     * @return true if the shell should exit; false otherwise.
     */
    bool get_exit_flag() {
      return exit_flag;
    }

    /**
     * @brief Sets the shell's interactive mode state.
     *
     * @param flag If true, enables interactive mode; otherwise disables it.
     */
    void set_interactive_mode(bool flag) {
      interactive_mode = flag;
    }

    /**
     * @brief Checks if the shell is in interactive mode.
     *
     * @return true if the shell is running interactively; false otherwise.
     */
    bool get_interactive_mode() {
      return interactive_mode;
    }

    /**
     * @brief Indicates whether the shell is operating in login mode.
     *
     * @return true if the shell is in login mode, false otherwise.
     */
    bool get_login_mode() {
      return login_mode;
    }

    /**
     * @brief Sets the shell's command aliases.
     *
     * Replaces the current set of command aliases with the provided mapping.
     *
     * @param aliases Map of command names to their alias strings.
     */
    void set_aliases(std::map<std::string, std::string> aliases) {
      this->aliases = aliases;
    }

  private:
    bool exit_flag = false;
    bool interactive_mode = false;
    bool login_mode = false;
    int shell_terminal;
    pid_t pid;

    std::map<std::string, std::string> aliases;

    Prompt* shell_prompt = nullptr;
    Exec* shell_exec = nullptr;
};
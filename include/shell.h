#pragma once

#include "prompt.h"
#include "exec.h"
#include "parser.h"
#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <string>
#include <sys/types.h>

// Forward declaration
class Exec;

//this will take input from main.cpp and will handle prompting and executing the command

class Shell {
  public:
    Shell(pid_t pid, char *argv[]);
    ~Shell();

    void execute_command(std::string command, bool sync = false);

    std::string get_prompt() {
      // prompt fallback built-in inside fuction return
      return shell_prompt->get_prompt();
    }

    std::string get_ai_prompt() {
      // prompt fallback built-in inside fuction return
      return shell_prompt->get_ai_prompt();
    }

    void set_exit_flag(bool flag) {
      exit_flag = flag;
    }

    bool get_exit_flag() {
      return exit_flag;
    }

    void set_interactive_mode(bool flag) {
      interactive_mode = flag;
    }

    bool get_interactive_mode() {
      return interactive_mode;
    }

    bool get_login_mode() {
      return login_mode;
    }

    void set_aliases(std::unordered_map<std::string, std::string> aliases) {
      shell_parser->set_aliases(aliases);
    }

    void set_env_vars(std::unordered_map<std::string, std::string> env_vars) {
      shell_parser->set_env_vars(env_vars);
    }

  private:
    bool exit_flag = false;
    bool interactive_mode = false;
    bool login_mode = false;
    int shell_terminal;
    pid_t pid;

    std::unique_ptr<Prompt> shell_prompt;
    std::unique_ptr<Exec> shell_exec;
    Parser* shell_parser = nullptr;
};
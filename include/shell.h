#pragma once

#include "prompt.h"
#include "exec.h"
#include "parser.h"
#include <termios.h>
#include <unistd.h>
#include <string>
#include <sys/types.h>

// Forward declaration
class Exec;
class Built_ins;

//this will take input from main.cpp and will handle prompting and executing the command

class Shell {
  public:
    Shell(char *argv[]);
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

    std::string last_terminal_output_error;
    std::string last_command;

  private:
    bool interactive_mode = false;
    bool login_mode = false;
    int shell_terminal;

    std::unique_ptr<Prompt> shell_prompt;
    std::unique_ptr<Exec> shell_exec;
    Parser* shell_parser = nullptr;
    Built_ins* built_ins = nullptr;
};
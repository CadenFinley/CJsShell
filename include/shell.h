#pragma once

#include "prompt.h"
#include "exec.h"
#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <string>
#include <map>
#include <sys/types.h>

//this will take input from main.cpp and will handle prompting and executing the command

// will also do signal handling

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

    void set_current_working_directory(std::string cwd) {
      current_working_directory = cwd;
    }

    void set_aliases(std::map<std::string, std::string> aliases) {
      this->aliases = aliases;
    }

  private:
    bool exit_flag = false;
    bool interactive_mode = false;
    bool login_mode = false;
    int shell_terminal;
    pid_t pid;
    std::string current_working_directory;

    std::map<std::string, std::string> aliases;

    Prompt* shell_prompt = nullptr;
    Exec* shell_exec = nullptr;
};
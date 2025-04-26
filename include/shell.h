#pragma once

#include "prompt.h"
#include "exec.h"
#include <termios.h>
#include <map>
#include <string>
#include <signal.h>
#include <unistd.h>

//this will take input from main.cpp and will handle prompting and executing the command

// will also do signal handling

class Shell {
  public:
    Shell(pid_t pid, int argc, char *argv[]);
    ~Shell();

    void execute_command(std::string command, bool sync = false);
    std::string get_prompt();

    void set_exit_flag(bool flag);
    bool get_exit_flag();

    void set_interactive_mode(bool flag);
    bool get_interactive_mode();
    bool get_login_mode();

    void set_aliases(std::map<std::string, std::string> aliases);

  private:
    bool exit_flag = false;
    bool interactive_mode = false;
    bool login_mode = false;
    int shell_terminal;
    pid_t pid;

    std::map<std::string, std::string> aliases;

    prompt* shell_prompt = nullptr;
    exec* shell_exec = nullptr;
};
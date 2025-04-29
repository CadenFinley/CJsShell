#pragma once

#include "prompt.h"
#include "exec.h"
#include "parser.h"
#include <termios.h>
#include <unistd.h>
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <signal.h>

class Exec;
class Built_ins;

extern void shell_signal_handler(int signum, siginfo_t* info, void* context);

class Shell {
  public:
    Shell(char *argv[]);
    ~Shell();

    void execute_command(std::string command, bool sync = false);
    void process_pending_signals();

    std::string get_prompt() {
      // prompt fallback built-in inside fuction return
      return shell_prompt->get_prompt();
    }

    std::string get_ai_prompt() {
      // prompt fallback built-in inside fuction return
      return shell_prompt->get_ai_prompt();
    }

    std::string get_newline_prompt() {
      return shell_prompt->get_newline_prompt();
    }

    std::string get_title_prompt() {
      return shell_prompt->get_title_prompt();
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


    void set_aliases(const std::unordered_map<std::string, std::string>& new_aliases) {
      aliases = new_aliases;
      if (shell_parser) {
        shell_parser->set_aliases(aliases);
      }
    }

    void set_env_vars(const std::unordered_map<std::string, std::string>& new_env_vars) {
      env_vars = new_env_vars;
      if (shell_parser) {
        shell_parser->set_env_vars(env_vars);
      }
    }

    std::unordered_map<std::string, std::string>& get_aliases() {
      return aliases;
    }

    std::unordered_map<std::string, std::string>& get_env_vars() {
      return env_vars;
    }
    
    void setup_signal_handlers();
    void save_terminal_state();
    void restore_terminal_state();
    void setup_job_control();

    // lol friend funny c++ 90's OOP moment
    friend void shell_signal_handler(int signum, siginfo_t* info, void* context);

    std::string last_terminal_output_error;
    std::string last_command;
    std::unique_ptr<Exec> shell_exec;

  private:
    bool interactive_mode = false;
    bool login_mode = false;
    int shell_terminal;
    pid_t shell_pgid;
    struct termios shell_tmodes;
    bool terminal_state_saved = false;
    bool job_control_enabled = false;

    std::unique_ptr<Prompt> shell_prompt;
    Parser* shell_parser = nullptr;
    Built_ins* built_ins = nullptr;
    
    std::unordered_map<std::string, std::string> aliases;
    std::unordered_map<std::string, std::string> env_vars;
};

extern Shell* g_shell_instance;
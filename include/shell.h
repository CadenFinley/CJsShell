#pragma once

#include <signal.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "exec.h"
#include "parser.h"
#include "prompt.h"
#include "shell_script_interpreter.h"
#include "signal_handler.h"

class Exec;
class Built_ins;
class ShellScriptInterpreter;

class Shell {
 public:
  Shell(bool login_mode = false);
  ~Shell();

  int execute_command(std::string command);
  int do_ai_request(const std::string& command);
  void process_pending_signals();

  std::string get_prompt() { return shell_prompt->get_prompt(); }

  std::string get_ai_prompt() { return shell_prompt->get_ai_prompt(); }

  std::string get_newline_prompt() {
    return shell_prompt->get_newline_prompt();
  }

  std::string get_title_prompt() { return shell_prompt->get_title_prompt(); }

  void set_interactive_mode(bool flag) { interactive_mode = flag; }

  bool get_interactive_mode() { return interactive_mode; }

  bool get_login_mode() { return login_mode; }

  int get_last_exit_code() const { return last_exit_code; }

  void set_aliases(
      const std::unordered_map<std::string, std::string>& new_aliases) {
    aliases = new_aliases;
    if (shell_parser) {
      shell_parser->set_aliases(aliases);
    }
  }

  void set_env_vars(
      const std::unordered_map<std::string, std::string>& new_env_vars) {
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

  std::string last_terminal_output_error;
  std::string last_command;
  std::unique_ptr<Exec> shell_exec;

  bool get_menu_active() { return menu_active; }

  void set_menu_active(bool active) { menu_active = active; }
  std::unordered_set<std::string> get_available_commands() const;

  std::string get_previous_directory() const;

  Built_ins* built_ins = nullptr;

 private:
  bool interactive_mode = false;
  bool login_mode = false;
  bool menu_active = true;
  int shell_terminal;
  pid_t shell_pgid;
  struct termios shell_tmodes;
  bool terminal_state_saved = false;
  bool job_control_enabled = false;
  int last_exit_code = 0;

  std::unique_ptr<Prompt> shell_prompt;
  std::unique_ptr<SignalHandler> signal_handler;
  ShellScriptInterpreter* shell_script_interpreter = nullptr;
  Parser* shell_parser = nullptr;

  std::unordered_map<std::string, std::string> aliases;
  std::unordered_map<std::string, std::string> env_vars;
};
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
  Shell();
  ~Shell();

  // High-level entry: treat any string as a shell script line and execute via
  // the script interpreter (supports semicolons, conditionals, etc.). Returns
  // last exit code.
  int execute(const std::string& script);

  int execute_command(std::vector<std::string> args,
                      bool run_in_background = false);
  int do_ai_request(const std::string& command);
  void process_pending_signals();

  std::string get_prompt() {
    return shell_prompt->get_prompt();
  }

  std::string get_ai_prompt() {
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

  int get_last_exit_code() const {
    return last_exit_code;
  }

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

  // Positional parameters support
  void set_positional_parameters(const std::vector<std::string>& params);
  int shift_positional_parameters(int count = 1);
  std::vector<std::string> get_positional_parameters() const;
  size_t get_positional_parameter_count() const;

  void setup_signal_handlers();
  void setup_interactive_handlers();
  void save_terminal_state();
  void restore_terminal_state();
  void setup_job_control();

  std::string last_terminal_output_error;
  std::string last_command;
  std::unique_ptr<Exec> shell_exec;

  bool get_menu_active() {
    return menu_active;
  }

  void set_menu_active(bool active) {
    menu_active = active;
  }
  std::unordered_set<std::string> get_available_commands() const;

  std::string get_previous_directory() const;

  Built_ins* get_built_ins() {
    return built_ins.get();
  }
  int get_terminal() const {
    return shell_terminal;
  }
  pid_t get_pgid() const {
    return shell_pgid;
  }
  struct termios get_terminal_modes() const {
    return shell_tmodes;
  }
  bool is_terminal_state_saved() const {
    return terminal_state_saved;
  }
  bool is_job_control_enabled() const {
    return job_control_enabled;
  }
  ShellScriptInterpreter* get_shell_script_interpreter() {
    return shell_script_interpreter.get();
  }

  Parser* get_parser() {
    return shell_parser.get();
  }

 private:
  bool interactive_mode = false;
  bool menu_active = true;
  int shell_terminal;
  pid_t shell_pgid;
  struct termios shell_tmodes;
  bool terminal_state_saved = false;
  bool job_control_enabled = false;
  int last_exit_code = 0;

  std::unique_ptr<Prompt> shell_prompt;
  std::unique_ptr<SignalHandler> signal_handler;
  std::unique_ptr<Built_ins> built_ins;
  std::unique_ptr<Parser> shell_parser;
  std::unique_ptr<ShellScriptInterpreter> shell_script_interpreter;

  std::unordered_map<std::string, std::string> aliases;
  std::unordered_map<std::string, std::string> env_vars;
  std::vector<std::string> positional_parameters;
};
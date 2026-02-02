/*
  shell.h

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <termios.h>

#include "parser.h"
#include "signal_handler.h"

class Exec;
class Built_ins;
class ShellScriptInterpreter;
struct Command;

class Shell {
   public:
    Shell();
    ~Shell();
    int execute(const std::string& script, bool skip_validation = false);

    int execute_command(std::vector<std::string> args, bool run_in_background = false);
    SignalProcessingResult process_pending_signals();

    void set_interactive_mode(bool flag);

    bool get_interactive_mode() const;

    void set_aliases(const std::unordered_map<std::string, std::string>& new_aliases);

    void set_abbreviations(const std::unordered_map<std::string, std::string>& new_abbreviations);

    void set_env_vars(const std::unordered_map<std::string, std::string>& new_env_vars);

    std::unordered_map<std::string, std::string>& get_aliases();

    std::unordered_map<std::string, std::string>& get_abbreviations();

    std::unordered_map<std::string, std::string>& get_env_vars();

    void set_positional_parameters(const std::vector<std::string>& params);
    int shift_positional_parameters(int count = 1);
    std::vector<std::string> get_positional_parameters() const;
    size_t get_positional_parameter_count() const;
    void set_shell_option(const std::string& option, bool value);
    bool get_shell_option(const std::string& option) const;
    bool is_errexit_enabled() const;

    void set_errexit_severity(const std::string& severity);
    std::string get_errexit_severity() const;
    bool should_abort_on_nonzero_exit() const;
    bool should_abort_on_nonzero_exit(int exit_code) const;

    void expand_env_vars(std::string& value);
    void sync_env_vars_from_system();

    void setup_signal_handlers();
    void setup_interactive_handlers();
    void save_terminal_state();
    void restore_terminal_state();
    void setup_job_control();
    void handle_sigcont();

    void register_hook(const std::string& hook_type, const std::string& function_name);
    void unregister_hook(const std::string& hook_type, const std::string& function_name);
    std::vector<std::string> get_hooks(const std::string& hook_type) const;
    void clear_hooks(const std::string& hook_type);
    void execute_hooks(const std::string& hook_type);

    std::string last_terminal_output_error;
    std::string last_command;
    std::unique_ptr<Exec> shell_exec;

    std::unordered_set<std::string> get_available_commands() const;

    std::string get_previous_directory() const;

    Built_ins* get_built_ins();
    bool is_job_control_enabled() const;
    ShellScriptInterpreter* get_shell_script_interpreter();

    Parser* get_parser();

    int execute_script_file(const std::filesystem::path& path, bool optional = false);

   private:
    bool interactive_mode = false;
    int shell_terminal;
    pid_t shell_pgid;
    struct termios shell_tmodes;
    bool terminal_state_saved = false;
    bool job_control_enabled = false;

    std::unique_ptr<SignalHandler> signal_handler;
    std::unique_ptr<Built_ins> built_ins;
    std::unique_ptr<Parser> shell_parser;
    std::unique_ptr<ShellScriptInterpreter> shell_script_interpreter;

    std::unordered_map<std::string, std::string> abbreviations;
    std::unordered_map<std::string, std::string> aliases;
    std::unordered_map<std::string, std::string> env_vars;
    std::vector<std::string> positional_parameters;
    std::unordered_map<std::string, bool> shell_options;
    std::string errexit_severity_level = "error";

    std::unordered_map<std::string, std::vector<std::string>> hooks;
    std::string last_directory;

    void apply_abbreviations_to_line_editor();
};

int read_exit_code_or(int fallback);
int handle_non_interactive_mode(const std::string& script_file);

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

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <termios.h>

#include "error_out.h"
#include "parser.h"
#include "signal_handler.h"

class Exec;
class Built_ins;
class ShellScriptInterpreter;
struct Command;

enum class ShellOption : std::uint8_t {
    Errexit,
    Noclobber,
    Nounset,
    Xtrace,
    Verbose,
    Noexec,
    Noglob,
    Globstar,
    Allexport,
    Huponexit,
    Pipefail,
    Count
};

struct ShellOptionDescriptor {
    ShellOption option;
    char short_flag;
    const char* name;
};

const std::array<ShellOptionDescriptor, static_cast<size_t>(ShellOption::Count)>&
get_shell_option_descriptors();
std::optional<ShellOption> parse_shell_option(const std::string& name);
const char* shell_option_name(ShellOption option);

enum class HookType : std::uint8_t {
    Precmd,
    Preexec,
    Chpwd,
    Count
};

struct HookTypeDescriptor {
    HookType type;
    const char* name;
};

const std::array<HookTypeDescriptor, static_cast<size_t>(HookType::Count)>&
get_hook_type_descriptors();
std::optional<HookType> parse_hook_type(const std::string& name);
const char* hook_type_name(HookType type);

class Shell {
   public:
    Shell();
    ~Shell();
    int execute(const std::string& script, bool skip_validation = false);
    int execute_command(std::vector<std::string> args, bool run_in_background = false,
                        bool auto_background_on_stop = false,
                        bool auto_background_on_stop_silent = false);
    int execute_script_file(const std::filesystem::path& path, bool optional = false);

    SignalProcessingResult process_pending_signals();
    void setup_signal_handlers();
    void setup_interactive_handlers();
    void save_terminal_state();
    void restore_terminal_state();
    void setup_job_control();
    void handle_sigcont();
    bool is_job_control_enabled() const;

    void set_interactive_mode(bool flag);
    bool get_interactive_mode() const;
    void set_abbreviations(const std::unordered_map<std::string, std::string>& new_abbreviations);
    std::unordered_map<std::string, std::string>& get_abbreviations();
    void set_aliases(const std::unordered_map<std::string, std::string>& new_aliases);
    std::unordered_map<std::string, std::string>& get_aliases();

    std::vector<std::string>& get_directory_stack();
    const std::vector<std::string>& get_directory_stack() const;
    void push_directory_stack(const std::string& dir);
    bool pop_directory_stack(std::string* dir_out);
    void clear_directory_stack();

    void register_hook(HookType hook_type, const std::string& function_name);
    void unregister_hook(HookType hook_type, const std::string& function_name);
    std::vector<std::string> get_hooks(HookType hook_type) const;
    void clear_hooks(HookType hook_type);
    void execute_hooks(HookType hook_type);

    void apply_no_exec(bool enabled);
    void set_shell_option(ShellOption option, bool value);
    bool get_shell_option(ShellOption option) const;
    bool is_errexit_enabled() const;
    void set_errexit_severity(const std::string& severity);
    std::string get_errexit_severity() const;
    bool should_abort_on_nonzero_exit() const;
    bool should_abort_on_nonzero_exit(int exit_code) const;

    std::unordered_set<std::string> get_available_commands() const;
    std::string get_previous_directory() const;
    Built_ins* get_built_ins();
    ShellScriptInterpreter* get_shell_script_interpreter();
    Parser* get_parser();

    std::string last_command;
    std::unique_ptr<Exec> shell_exec;

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
    std::array<bool, static_cast<size_t>(ShellOption::Count)> shell_options{};
    std::vector<std::string> directory_stack;
    ErrorSeverity errexit_severity_level = ErrorSeverity::ERROR;

    std::array<std::vector<std::string>, static_cast<size_t>(HookType::Count)> hooks;
    std::string last_directory;

    void apply_abbreviations_to_line_editor();
};

int read_exit_code_or(int fallback);

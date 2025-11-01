#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <termios.h>

#include "parser.h"
#include "prompt.h"
#include "signal_handler.h"

class Exec;
class Built_ins;
class ShellScriptInterpreter;
class Theme;
struct Command;

struct RawModeState {
    bool entered;
    int fd;
    struct termios saved_modes;
};

void raw_mode_state_init(RawModeState* state);
void raw_mode_state_init_with_fd(RawModeState* state, int fd);
void raw_mode_state_release(RawModeState* state);
bool raw_mode_state_entered(const RawModeState* state);

class Shell {
   public:
    Shell();
    ~Shell();
    int execute(const std::string& script, bool skip_validation = false);

    int execute_command(std::vector<std::string> args, bool run_in_background = false);
    SignalProcessingResult process_pending_signals();

    std::string get_prompt() {
        return shell_prompt->get_prompt();
    }

    std::string get_newline_prompt() {
        return shell_prompt->get_newline_prompt();
    }

    std::string get_inline_right_prompt() {
        return shell_prompt->get_inline_right_prompt();
    }

    std::string get_title_prompt() {
        return shell_prompt->get_title_prompt();
    }

    void start_command_timing() {
        if (shell_prompt) {
            shell_prompt->start_command_timing();
        }
    }

    void end_command_timing(int exit_code) {
        if (shell_prompt) {
            shell_prompt->end_command_timing(exit_code);
        }
    }

    void reset_command_timing() {
        if (shell_prompt) {
            shell_prompt->reset_command_timing();
        }
    }

    void set_initial_duration(long long microseconds) {
        if (shell_prompt) {
            shell_prompt->set_initial_duration(microseconds);
        }
    }

    void invalidate_prompt_caches();

    std::string get_initial_duration() {
        if (shell_prompt) {
            return shell_prompt->get_initial_duration();
        }
        return "0";
    }

    void set_interactive_mode(bool flag);

    bool get_interactive_mode() const {
        return interactive_mode;
    }

    int get_last_exit_code() const {
        const char* status_env = getenv("?");
        if (status_env != nullptr) {
            char* end = nullptr;
            long value = std::strtol(status_env, &end, 10);
            if (*end == '\0' && end != status_env) {
                return static_cast<int>(value);
            }
        }
        return 0;
    }

    void set_aliases(const std::unordered_map<std::string, std::string>& new_aliases) {
        aliases = new_aliases;
        if (shell_parser) {
            shell_parser->set_aliases(aliases);
        }
    }

    void set_abbreviations(const std::unordered_map<std::string, std::string>& new_abbreviations);

    void set_env_vars(const std::unordered_map<std::string, std::string>& new_env_vars) {
        env_vars = new_env_vars;
        if (shell_parser) {
            shell_parser->set_env_vars(env_vars);
        }
    }

    std::unordered_map<std::string, std::string>& get_aliases() {
        return aliases;
    }

    std::unordered_map<std::string, std::string>& get_abbreviations() {
        return abbreviations;
    }

    std::unordered_map<std::string, std::string>& get_env_vars() {
        return env_vars;
    }

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

    Theme* ensure_theme();
    Theme* get_theme() const;
    void reset_theme();

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

    int execute_script_file(const std::filesystem::path& path, bool optional = false);

   private:
    bool interactive_mode = false;
    int shell_terminal;
    pid_t shell_pgid;
    struct termios shell_tmodes;
    bool terminal_state_saved = false;
    bool job_control_enabled = false;

    std::unique_ptr<Theme> shell_theme;
    std::unique_ptr<Prompt> shell_prompt;
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
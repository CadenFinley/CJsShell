#include "shell.h"

#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>

#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "exec.h"
#include "isocline.h"
#include "job_control.h"
#include "shell_env.h"
#include "shell_script_interpreter.h"
#include "trap_command.h"

void Shell::set_abbreviations(
    const std::unordered_map<std::string, std::string>& new_abbreviations) {
    abbreviations = new_abbreviations;
    apply_abbreviations_to_line_editor();
}

void Shell::set_interactive_mode(bool flag) {
    if (interactive_mode == flag) {
        return;
    }

    interactive_mode = flag;

    if (interactive_mode) {
        apply_abbreviations_to_line_editor();
    } else {
        ic_clear_abbreviations();
    }
}

bool Shell::get_interactive_mode() const {
    return interactive_mode;
}

int Shell::get_last_exit_code() const {
    const char* status_env = std::getenv("?");
    if (status_env != nullptr) {
        char* end = nullptr;
        long value = std::strtol(status_env, &end, 10);
        if (*end == '\0' && end != status_env) {
            return static_cast<int>(value);
        }
    }
    return 0;
}

void Shell::set_aliases(const std::unordered_map<std::string, std::string>& new_aliases) {
    aliases = new_aliases;
    if (shell_parser) {
        shell_parser->set_aliases(aliases);
    }
}

void Shell::set_env_vars(const std::unordered_map<std::string, std::string>& new_env_vars) {
    env_vars = new_env_vars;
    if (shell_parser) {
        shell_parser->set_env_vars(env_vars);
    }
}

std::unordered_map<std::string, std::string>& Shell::get_aliases() {
    return aliases;
}

std::unordered_map<std::string, std::string>& Shell::get_abbreviations() {
    return abbreviations;
}

std::unordered_map<std::string, std::string>& Shell::get_env_vars() {
    return env_vars;
}

void Shell::apply_abbreviations_to_line_editor() {
    if (!interactive_mode) {
        return;
    }

    ic_clear_abbreviations();
    for (const auto& [name, expansion] : abbreviations) {
        (void)ic_add_abbreviation(name.c_str(), expansion.c_str());
    }
}

SignalProcessingResult Shell::process_pending_signals() {
    if (!signal_handler) {
        return {};
    }

    if (!SignalHandler::has_pending_signals()) {
        return {};
    }

    Exec* exec_ptr = shell_exec ? shell_exec.get() : nullptr;
    return signal_handler->process_pending_signals(exec_ptr);
}

Shell::Shell() : shell_pgid(0), shell_tmodes() {
    save_terminal_state();

    shell_exec = std::make_unique<Exec>();
    signal_handler = std::make_unique<SignalHandler>();
    shell_parser = std::make_unique<Parser>();
    built_ins = std::make_unique<Built_ins>();
    shell_script_interpreter = std::make_unique<ShellScriptInterpreter>();

    if (shell_script_interpreter && shell_parser) {
        shell_script_interpreter->set_parser(shell_parser.get());
        shell_parser->set_shell(this);
    }
    built_ins->set_shell(this);
    built_ins->set_current_directory();

    abbreviations.emplace("abbr", "abbreviate");
    abbreviations.emplace("unabbr", "unabbreviate");
    set_abbreviations(abbreviations);

    shell_terminal = STDIN_FILENO;

    JobManager::instance().set_shell(this);
    trap_manager_set_shell(this);

    setup_signal_handlers();
    g_signal_handler = signal_handler.get();
    setup_job_control();
}

Shell::~Shell() {
    if (interactive_mode) {
        std::cerr << "Destroying Shell.\n";
    }

    shell_exec->terminate_all_child_process();
    restore_terminal_state();
}

int Shell::execute_script_file(const std::filesystem::path& path, bool optional) {
    if (!shell_script_interpreter) {
        print_error({ErrorType::RUNTIME_ERROR, "source", "script interpreter not available", {}});
        return 1;
    }

    std::filesystem::path normalized = path.lexically_normal();
    std::string display_path = normalized.string();

    std::error_code abs_ec;
    auto absolute_path = std::filesystem::absolute(normalized, abs_ec);
    if (!abs_ec) {
        normalized = absolute_path.lexically_normal();
        display_path = normalized.string();
    }

    std::ifstream file(normalized);
    if (!file) {
        if (optional) {
            return 0;
        }
        print_error(
            {ErrorType::FILE_NOT_FOUND, "source", "cannot open file '" + display_path + "'", {}});
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    auto parsed_lines = shell_script_interpreter->parse_into_lines(buffer.str());
    if (parsed_lines.empty()) {
        return 0;
    }

    return shell_script_interpreter->execute_block(parsed_lines);
}

int Shell::execute(const std::string& script, bool skip_validation) {
    if (script.empty()) {
        return 0;
    }
    std::vector<std::string> lines;

    lines = shell_parser->parse_into_lines(script);

    if (shell_script_interpreter) {
        int exit_code = shell_script_interpreter->execute_block(lines, skip_validation);
        last_command = script;
        return exit_code;
    }
    print_error(ErrorInfo{
        ErrorType::RUNTIME_ERROR, "", "No script interpreter available", {"Restart cjsh"}});
    return 1;
}

void Shell::setup_signal_handlers() {
    signal_handler->setup_signal_handlers();
}

void Shell::setup_interactive_handlers() {
    signal_handler->setup_interactive_handlers();
}

void Shell::save_terminal_state() {
    if (isatty(STDIN_FILENO) != 0) {
        if (tcgetattr(STDIN_FILENO, &shell_tmodes) == 0) {
            terminal_state_saved = true;
        }
    }
}

void Shell::restore_terminal_state() {
    if (interactive_mode) {
        std::cerr << "Restoring terminal state.\n";
    }

    if (terminal_state_saved) {
        if (tcsetattr(STDIN_FILENO, TCSANOW, &shell_tmodes) != 0) {
            tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_tmodes);
        }
        terminal_state_saved = false;
    }

    (void)fflush(stdout);
    (void)fflush(stderr);
}

void Shell::setup_job_control() {
    if (isatty(STDIN_FILENO) == 0) {
        job_control_enabled = false;
        return;
    }

    shell_pgid = getpid();

    if (setpgid(shell_pgid, shell_pgid) < 0) {
        if (errno != EPERM) {
            const auto error_text = std::system_category().message(errno);
            print_error({ErrorType::RUNTIME_ERROR,
                         "setpgid",
                         "couldn't put the shell in its own process group: " + error_text,
                         {}});
        }
    }

    try {
        shell_terminal = STDIN_FILENO;

        int tpgrp = tcgetpgrp(shell_terminal);
        if (tpgrp != -1) {
            if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
                const auto error_text = std::system_category().message(errno);
                print_error({ErrorType::RUNTIME_ERROR,
                             "tcsetpgrp",
                             "couldn't grab terminal control: " + error_text,
                             {}});
            }
        }
        job_control_enabled = true;
    } catch (const std::exception& e) {
        print_error(
            {ErrorType::RUNTIME_ERROR, "", e.what(), {"Check terminal settings", "Restart cjsh"}});
        job_control_enabled = false;
    }
}

void Shell::handle_sigcont() {
    if (!job_control_enabled) {
        return;
    }

    if (isatty(shell_terminal) != 0) {
        if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
            const auto error_text = std::system_category().message(errno);
            print_error({ErrorType::RUNTIME_ERROR,
                         "tcsetpgrp",
                         "failed to reclaim terminal after SIGCONT: " + error_text,
                         {}});
        }

        if (terminal_state_saved) {
            if (tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes) < 0) {
                const auto error_text = std::system_category().message(errno);
                print_error({ErrorType::RUNTIME_ERROR,
                             "tcsetattr",
                             "failed to restore terminal mode after SIGCONT: " + error_text,
                             {}});
            }
        } else {
            save_terminal_state();
        }
    }

    apply_abbreviations_to_line_editor();
}

int Shell::execute_command(std::vector<std::string> args, bool run_in_background) {
    if (args.empty()) {
        return 0;
    }
    if (!shell_exec || !built_ins) {
        g_exit_flag = true;
        print_error(
            {ErrorType::RUNTIME_ERROR, "", "Shell not properly initialized", {"Restart cjsh"}});
        return 1;
    }

    if (get_shell_option("xtrace") && !args.empty()) {
        std::cerr << "+ ";
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) {
                std::cerr << " ";
            }
            std::cerr << args[i];
        }
        std::cerr << '\n';
    }

    if (get_shell_option("noexec")) {
        return 0;
    }

    if (args.size() == 1 && shell_parser) {
        std::string var_name;
        std::string var_value;
        if (shell_parser->is_env_assignment(args[0], var_name, var_value)) {
            shell_parser->expand_env_vars(var_value);

            if (shell_script_interpreter) {
                shell_script_interpreter->get_variable_manager().set_environment_variable(
                    var_name, var_value);
            }

            return 0;
        }

        if (args[0].find('=') != std::string::npos) {
            return 1;
        }
    }

    std::vector<std::pair<std::string, std::string>> env_assignments;
    size_t cmd_start_idx = cjsh_env::collect_env_assignments(args, env_assignments);
    std::vector<std::string> command_args;
    if (cmd_start_idx < args.size()) {
        command_args.assign(std::next(args.begin(), static_cast<std::ptrdiff_t>(cmd_start_idx)),
                            args.end());
    }
    const bool has_temporary_env = !env_assignments.empty() && !command_args.empty();

    if (!command_args.empty() && (built_ins->is_builtin_command(command_args[0]) != 0)) {
        int code = 0;

        if (has_temporary_env) {
            struct SavedEnvState {
                std::string name;
                bool had_env = false;
                std::string env_value;
                bool had_map = false;
                std::string map_value;
            };

            auto& env_map = get_env_vars();
            std::vector<SavedEnvState> saved_states;
            saved_states.reserve(env_assignments.size());

            auto apply_assignments = [&]() {
                for (const auto& [name, value] : env_assignments) {
                    SavedEnvState state;
                    state.name = name;
                    if (const char* existing = std::getenv(name.c_str())) {
                        state.had_env = true;
                        state.env_value = existing;
                    }
                    auto map_it = env_map.find(name);
                    if (map_it != env_map.end()) {
                        state.had_map = true;
                        state.map_value = map_it->second;
                    }

                    setenv(name.c_str(), value.c_str(), 1);
                    env_map[name] = value;
                    saved_states.push_back(std::move(state));
                }
                if (shell_parser) {
                    shell_parser->set_env_vars(env_map);
                }
            };

            auto restore_assignments = [&]() {
                for (auto it = saved_states.rbegin(); it != saved_states.rend(); ++it) {
                    if (it->had_env) {
                        setenv(it->name.c_str(), it->env_value.c_str(), 1);
                    } else {
                        unsetenv(it->name.c_str());
                    }

                    if (it->had_map) {
                        env_map[it->name] = it->map_value;
                    } else {
                        env_map.erase(it->name);
                    }
                }
                if (shell_parser) {
                    shell_parser->set_env_vars(env_map);
                }
            };

            apply_assignments();
            code = built_ins->builtin_command(command_args);
            restore_assignments();
        } else {
            code = built_ins->builtin_command(command_args);
        }
        last_terminal_output_error = built_ins->get_last_error();

        return code;
    }

    if (!run_in_background && command_args.size() == 1 && built_ins) {
        const std::string& candidate = command_args[0];

        bool has_alias = aliases.find(candidate) != aliases.end();
        bool is_builtin = built_ins->is_builtin_command(candidate) != 0;
        bool is_function =
            shell_script_interpreter && shell_script_interpreter->has_function(candidate);
        bool is_executable =
            cjsh_filesystem::resolves_to_executable(candidate, built_ins->get_current_directory());
        bool is_directory = cjsh_filesystem::path_is_directory_candidate(
            candidate, built_ins->get_current_directory());

        if (!has_alias && !is_builtin && !is_function && !is_executable && is_directory &&
            !g_startup_active) {
            std::vector<std::string> cd_args = {"cd", candidate};
            int code = built_ins->builtin_command(cd_args);
            last_terminal_output_error = built_ins->get_last_error();
            return code;
        }
    }

    if (run_in_background) {
        int job_id = shell_exec->execute_command_async(args);
        if (job_id > 0) {
            auto jobs = shell_exec->get_jobs();
            auto it = jobs.find(job_id);
            if (it != jobs.end() && !it->second.pids.empty()) {
                pid_t last_pid = it->second.pids.back();
                setenv("!", std::to_string(last_pid).c_str(), 1);

                JobManager::instance().set_last_background_pid(last_pid);
            }
        }
        last_terminal_output_error = "Background command launched";
        return 0;
    }
    shell_exec->execute_command_sync(args);
    last_terminal_output_error = shell_exec->get_error_string();
    int exit_code = shell_exec->get_exit_code();

    if (exit_code != 0) {
        ErrorInfo error = shell_exec->get_error();
        if (error.type != ErrorType::RUNTIME_ERROR ||
            error.message.find("command failed with exit code") == std::string::npos) {
            shell_exec->print_last_error();
        }
    }
    return exit_code;
}

std::unordered_set<std::string> Shell::get_available_commands() const {
    std::unordered_set<std::string> cmds;
    if (built_ins) {
        auto b = built_ins->get_builtin_commands();
        cmds.insert(b.begin(), b.end());
    }
    for (const auto& alias : aliases) {
        cmds.insert(alias.first);
    }

    if (shell_script_interpreter) {
        auto function_names = shell_script_interpreter->get_function_names();
        cmds.insert(function_names.begin(), function_names.end());
    }
    return cmds;
}

std::string Shell::get_previous_directory() const {
    return built_ins->get_previous_directory();
}

Built_ins* Shell::get_built_ins() {
    return built_ins.get();
}

int Shell::get_terminal() const {
    return shell_terminal;
}

pid_t Shell::get_pgid() const {
    return shell_pgid;
}

struct termios Shell::get_terminal_modes() const {
    return shell_tmodes;
}

bool Shell::is_terminal_state_saved() const {
    return terminal_state_saved;
}

bool Shell::is_job_control_enabled() const {
    return job_control_enabled;
}

ShellScriptInterpreter* Shell::get_shell_script_interpreter() {
    return shell_script_interpreter.get();
}

Parser* Shell::get_parser() {
    return shell_parser.get();
}

void Shell::set_positional_parameters(const std::vector<std::string>& params) {
    positional_parameters = params;
}

int Shell::shift_positional_parameters(int count) {
    if (count < 0) {
        return 1;
    }

    if (static_cast<size_t>(count) >= positional_parameters.size()) {
        positional_parameters.clear();
    } else {
        positional_parameters.erase(positional_parameters.begin(),
                                    positional_parameters.begin() + count);
    }

    return 0;
}

std::vector<std::string> Shell::get_positional_parameters() const {
    return positional_parameters;
}

size_t Shell::get_positional_parameter_count() const {
    return positional_parameters.size();
}

void Shell::set_shell_option(const std::string& option, bool value) {
    shell_options[option] = value;
}

bool Shell::get_shell_option(const std::string& option) const {
    auto it = shell_options.find(option);
    return it != shell_options.end() ? it->second : false;
}

bool Shell::is_errexit_enabled() const {
    return get_shell_option("errexit");
}

void Shell::set_errexit_severity(const std::string& severity) {
    std::string lower_severity = severity;
    std::transform(lower_severity.begin(), lower_severity.end(), lower_severity.begin(), ::tolower);

    if (lower_severity == "info" || lower_severity == "warning" || lower_severity == "error" ||
        lower_severity == "critical") {
        errexit_severity_level = lower_severity;
    } else {
        errexit_severity_level = "error";
    }
}

std::string Shell::get_errexit_severity() const {
    return errexit_severity_level;
}

bool Shell::should_abort_on_nonzero_exit() const {
    if (!is_errexit_enabled()) {
        return false;
    }

    std::string threshold = get_errexit_severity();

    if (threshold == "critical") {
        return false;
    }

    return true;
}

bool Shell::should_abort_on_nonzero_exit(int exit_code) const {
    if (!is_errexit_enabled()) {
        return false;
    }

    std::string threshold = get_errexit_severity();

    ErrorSeverity error_severity = ErrorSeverity::ERROR;  // default

    if (exit_code == 127) {
        error_severity = ErrorInfo::get_default_severity(ErrorType::COMMAND_NOT_FOUND);
    } else if (exit_code == 126) {
        error_severity = ErrorInfo::get_default_severity(ErrorType::PERMISSION_DENIED);
    } else if (exit_code == 2) {
        error_severity = ErrorInfo::get_default_severity(ErrorType::SYNTAX_ERROR);
    }

    ErrorSeverity threshold_severity = ErrorSeverity::ERROR;
    if (threshold == "info") {
        threshold_severity = ErrorSeverity::INFO;
    } else if (threshold == "warning") {
        threshold_severity = ErrorSeverity::WARNING;
    } else if (threshold == "error") {
        threshold_severity = ErrorSeverity::ERROR;
    } else if (threshold == "critical") {
        threshold_severity = ErrorSeverity::CRITICAL;
    }

    return error_severity >= threshold_severity;
}

void Shell::expand_env_vars(std::string& value) {
    if (shell_parser) {
        shell_parser->expand_env_vars(value);
    }
}

void Shell::sync_env_vars_from_system() {
    extern char** environ;
    for (char** env = environ; *env != nullptr; env++) {
        std::string env_str(*env);
        size_t eq_pos = env_str.find('=');
        if (eq_pos != std::string::npos) {
            std::string name = env_str.substr(0, eq_pos);
            std::string value = env_str.substr(eq_pos + 1);
            env_vars[name] = value;
        }
    }

    if (shell_parser) {
        shell_parser->set_env_vars(env_vars);
    }
}

void Shell::register_hook(const std::string& hook_type, const std::string& function_name) {
    if (function_name.empty()) {
        return;
    }

    auto& hook_list = hooks[hook_type];

    if (std::find(hook_list.begin(), hook_list.end(), function_name) == hook_list.end()) {
        hook_list.push_back(function_name);
    }
}

void Shell::unregister_hook(const std::string& hook_type, const std::string& function_name) {
    auto it = hooks.find(hook_type);
    if (it == hooks.end()) {
        return;
    }

    auto& hook_list = it->second;
    hook_list.erase(std::remove(hook_list.begin(), hook_list.end(), function_name),
                    hook_list.end());

    if (hook_list.empty()) {
        hooks.erase(it);
    }
}

std::vector<std::string> Shell::get_hooks(const std::string& hook_type) const {
    auto it = hooks.find(hook_type);
    if (it != hooks.end()) {
        return it->second;
    }
    return {};
}

void Shell::clear_hooks(const std::string& hook_type) {
    hooks.erase(hook_type);
}

void Shell::execute_hooks(const std::string& hook_type) {
    auto it = hooks.find(hook_type);
    if (it == hooks.end()) {
        return;
    }

    const auto& hook_list = it->second;
    if (hook_list.empty()) {
        return;
    }

    for (const auto& function_name : hook_list) {
        execute(function_name);
    }
}

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
#include <mutex>
#include <sstream>
#include <string_view>
#include <system_error>
#include <unordered_map>

#include "builtin.h"
#include "cjsh.h"
#include "error_out.h"
#include "exec.h"
#include "job_control.h"
#include "shell_script_interpreter.h"
#include "theme.h"
#include "trap_command.h"

namespace {
struct CachedScript {
    std::filesystem::file_time_type modified;
    std::shared_ptr<const std::vector<std::string>> lines;
};

std::mutex g_script_cache_mutex;
std::unordered_map<std::string, CachedScript> g_script_cache;

std::string to_lower_copy(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return result;
}

bool has_theme_extension(const std::filesystem::path& path) {
    if (!path.has_extension()) {
        return false;
    }
    std::string ext_lower = to_lower_copy(path.extension().string());
    std::string expected = to_lower_copy(std::string(Theme::kThemeFileExtension));
    return ext_lower == expected;
}

bool resolves_to_executable(const std::string& name, const std::string& cwd) {
    if (name.empty()) {
        return false;
    }

    std::error_code ec;

    auto check_path = [&](const std::filesystem::path& candidate) -> bool {
        if (!std::filesystem::exists(candidate, ec) || ec) {
            return false;
        }
        if (std::filesystem::is_directory(candidate, ec) || ec) {
            return false;
        }
        return access(candidate.c_str(), X_OK) == 0;
    };

    std::filesystem::path candidate(name);
    if (name.find('/') != std::string::npos) {
        if (!candidate.is_absolute()) {
            candidate = std::filesystem::path(cwd) / candidate;
        }
        return check_path(candidate);
    }

    const char* path_env = std::getenv("PATH");
    if (path_env == nullptr) {
        return false;
    }

    std::stringstream path_stream(path_env);
    std::string segment;
    while (std::getline(path_stream, segment, ':')) {
        if (segment.empty()) {
            segment = ".";
        }
        std::filesystem::path path_candidate = std::filesystem::path(segment) / name;
        if (check_path(path_candidate)) {
            return true;
        }
    }

    return false;
}

bool path_is_directory_candidate(const std::string& value, const std::string& cwd) {
    if (value.empty()) {
        return false;
    }

    std::error_code ec;
    std::filesystem::path candidate(value);
    if (!candidate.is_absolute()) {
        candidate = std::filesystem::path(cwd) / candidate;
    }

    return std::filesystem::exists(candidate, ec) && !ec &&
           std::filesystem::is_directory(candidate, ec) && !ec;
}
}  // namespace

void raw_mode_state_init(RawModeState* state) {
    if (state == nullptr) {
        return;
    }
    raw_mode_state_init_with_fd(state, STDIN_FILENO);
}

void raw_mode_state_init_with_fd(RawModeState* state, int fd) {
    if (state == nullptr) {
        return;
    }

    state->entered = false;
    state->fd = fd;

    if (fd < 0 || (isatty(fd) == 0)) {
        return;
    }

    if (tcgetattr(fd, &state->saved_modes) == -1) {
        return;
    }

    struct termios raw_modes = state->saved_modes;
    raw_modes.c_lflag &= ~ICANON;
    raw_modes.c_cc[VMIN] = 0;
    raw_modes.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &raw_modes) == -1) {
        return;
    }

    state->entered = true;
}

void raw_mode_state_release(RawModeState* state) {
    if ((state == nullptr) || !state->entered) {
        return;
    }

    if (tcsetattr(state->fd, TCSANOW, &state->saved_modes) == -1) {
    }

    state->entered = false;
}

bool raw_mode_state_entered(const RawModeState* state) {
    return (state != nullptr) && state->entered;
}

void Shell::process_pending_signals() {
    if (signal_handler && shell_exec) {
        signal_handler->process_pending_signals(shell_exec.get());
    }
}

Shell::Shell() : shell_pgid(0), shell_tmodes() {
    save_terminal_state();

    shell_prompt = std::make_unique<Prompt>();
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

    std::string cache_key = normalized.string();

    std::error_code mod_ec;
    auto mod_time = std::filesystem::last_write_time(normalized, mod_ec);

    std::shared_ptr<const std::vector<std::string>> cached_lines;
    if (!mod_ec) {
        std::lock_guard<std::mutex> lock(g_script_cache_mutex);
        auto it = g_script_cache.find(cache_key);
        if (it != g_script_cache.end() && it->second.modified == mod_time) {
            cached_lines = it->second.lines;
        }
    }

    if (!cached_lines) {
        std::ifstream file(normalized);
        if (!file) {
            if (optional) {
                return 0;
            }
            print_error({ErrorType::FILE_NOT_FOUND,
                         "source",
                         "cannot open file '" + display_path + "'",
                         {}});
            return 1;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        auto parsed_lines = std::make_shared<std::vector<std::string>>(
            shell_script_interpreter->parse_into_lines(buffer.str()));
        cached_lines = parsed_lines;

        if (!mod_ec) {
            std::lock_guard<std::mutex> lock(g_script_cache_mutex);
            g_script_cache[cache_key] = {mod_time, parsed_lines};
        }
    }

    return shell_script_interpreter->execute_block(*cached_lines);
}

int Shell::load_theme_from_file(const std::filesystem::path& path, bool optional) {
    std::filesystem::path normalized = path.lexically_normal();
    std::string display_path = normalized.string();

    std::error_code abs_ec;
    auto absolute_path = std::filesystem::absolute(normalized, abs_ec);
    if (!abs_ec) {
        normalized = absolute_path.lexically_normal();
        display_path = normalized.string();
    }

    if (!has_theme_extension(normalized)) {
        return 1;
    }

    std::error_code status_ec;
    auto status = std::filesystem::status(normalized, status_ec);
    if (status_ec || !std::filesystem::is_regular_file(status)) {
        if (optional) {
            return 0;
        }
        print_error({ErrorType::FILE_NOT_FOUND,
                     "load_theme",
                     "Theme file '" + display_path + "' does not exist.",
                     {"Check the file path and try again."}});
        return 1;
    }

    if (!config::themes_enabled) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "theme",
                     "Themes are disabled",
                     {"Enable themes via configuration or run 'cjshopt "
                      "--themes on'."}});
        return 1;
    }

    if (!g_theme) {
        initialize_themes();
    }

    if (!g_theme) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "theme",
                     "Theme manager not initialized",
                     {"Try running 'theme' again after initialization completes."}});
        return 1;
    }

    bool loaded = g_theme->load_theme_from_path(normalized, true);
    if (!loaded) {
        return 2;
    }

    return 0;
}

int Shell::execute(const std::string& script) {
    if (script.empty()) {
        return 0;
    }
    std::vector<std::string> lines;

    lines = shell_parser->parse_into_lines(script);

    if (shell_script_interpreter) {
        int exit_code = shell_script_interpreter->execute_block(lines);
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

            env_vars[var_name] = var_value;

            if (var_name == "PATH" || var_name == "PWD" || var_name == "HOME" ||
                var_name == "USER" || var_name == "SHELL") {
                setenv(var_name.c_str(), var_value.c_str(), 1);
            }

            if (shell_parser) {
                shell_parser->set_env_vars(env_vars);
            }

            return 0;
        }

        if (args[0].find('=') != std::string::npos) {
            return 1;
        }
    }

    if (!args.empty() && (built_ins->is_builtin_command(args[0]) != 0)) {
        int code = built_ins->builtin_command(args);
        last_terminal_output_error = built_ins->get_last_error();

        return code;
    }

    if (!run_in_background && args.size() == 1 && built_ins) {
        const std::string& candidate = args[0];

        std::string trimmed_candidate = candidate;
        while (trimmed_candidate.size() > 1 && trimmed_candidate.back() == '/') {
            trimmed_candidate.pop_back();
        }

        if (trimmed_candidate.empty()) {
            trimmed_candidate = candidate;
        }

        bool has_path_separator = trimmed_candidate.find('/') != std::string::npos;

        bool has_alias = aliases.find(candidate) != aliases.end();
        bool is_builtin = built_ins->is_builtin_command(candidate) != 0;
        bool is_function =
            shell_script_interpreter && shell_script_interpreter->has_function(candidate);
        bool is_executable = resolves_to_executable(candidate, built_ins->get_current_directory());
        bool is_directory =
            path_is_directory_candidate(candidate, built_ins->get_current_directory());

        if (!has_path_separator && !has_alias && !is_builtin && !is_function && !is_executable &&
            is_directory) {
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

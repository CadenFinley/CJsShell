#include "shell.h"

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>
#include <string_view>
#include <unordered_map>

#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "exec.h"
#include "job_control.h"
#include "plugin.h"
#include "shell_script_interpreter.h"
#include "suggestion_utils.h"
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
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
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
}  // namespace

ScopedRawMode::ScopedRawMode() : ScopedRawMode(STDIN_FILENO) {
}

ScopedRawMode::ScopedRawMode(int fd) : entered_(false), fd_(fd) {
    if (fd_ < 0 || !isatty(fd_)) {
        return;
    }

    if (tcgetattr(fd_, &saved_modes_) == -1) {
        if (g_debug_mode) {
            std::cerr << "DEBUG: ScopedRawMode failed to save current termios: " << strerror(errno) << std::endl;
        }
        return;
    }

    struct termios raw_modes = saved_modes_;
    raw_modes.c_lflag &= ~ICANON;
    raw_modes.c_cc[VMIN] = 0;
    raw_modes.c_cc[VTIME] = 0;

    if (tcsetattr(fd_, TCSANOW, &raw_modes) == -1) {
        if (g_debug_mode) {
            std::cerr << "DEBUG: ScopedRawMode failed to enter raw mode: " << strerror(errno) << std::endl;
        }
        return;
    }

    entered_ = true;
}

ScopedRawMode::~ScopedRawMode() {
    release();
}

void ScopedRawMode::release() {
    if (!entered_) {
        return;
    }

    if (tcsetattr(fd_, TCSANOW, &saved_modes_) == -1) {
        if (g_debug_mode) {
            std::cerr << "DEBUG: ScopedRawMode failed to restore termios: " << strerror(errno) << std::endl;
        }
    }

    entered_ = false;
}

void Shell::process_pending_signals() {
    if (signal_handler && shell_exec) {
        signal_handler->process_pending_signals(shell_exec.get());
    }
}

Shell::Shell() {
    if (g_debug_mode)
        std::cerr << "DEBUG: Constructing Shell" << std::endl;

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
    TrapManager::instance().set_shell(this);

    setup_signal_handlers();
    g_signal_handler = signal_handler.get();
    setup_job_control();
}

Shell::~Shell() {
    if (interactive_mode) {
        std::cerr << "Destroying Shell." << std::endl;
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

    if (has_theme_extension(normalized)) {
        return load_theme_from_file(normalized, optional);
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
            if (g_debug_mode) {
                std::cerr << "DEBUG: Using cached script: " << cache_key << std::endl;
            }
        }
    }

    if (!cached_lines) {
        std::ifstream file(normalized);
        if (!file) {
            if (optional) {
                if (g_debug_mode) {
                    std::cerr << "DEBUG: Optional script not found: " << display_path << std::endl;
                }
                return 0;
            }
            print_error({ErrorType::FILE_NOT_FOUND, "source", "cannot open file '" + display_path + "'", {}});
            return 1;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        auto parsed_lines = std::make_shared<std::vector<std::string>>(shell_script_interpreter->parse_into_lines(buffer.str()));
        cached_lines = parsed_lines;

        if (!mod_ec) {
            std::lock_guard<std::mutex> lock(g_script_cache_mutex);
            g_script_cache[cache_key] = {mod_time, parsed_lines};
        }

        if (g_debug_mode) {
            std::cerr << "DEBUG: Parsed script: " << display_path << " (" << parsed_lines->size() << " lines)" << std::endl;
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
        if (g_debug_mode) {
            std::cerr << "DEBUG: load_theme_from_file called for non-theme "
                      << "path: " << display_path << std::endl;
        }
        return 1;
    }

    std::error_code status_ec;
    auto status = std::filesystem::status(normalized, status_ec);
    if (status_ec || !std::filesystem::is_regular_file(status)) {
        if (optional) {
            if (g_debug_mode) {
                std::cerr << "DEBUG: Optional theme file not found: " << display_path << std::endl;
            }
            return 0;
        }
        print_error({ErrorType::FILE_NOT_FOUND,
                     "load_theme",
                     "Theme file '" + display_path + "' does not exist.",
                     {"Use 'theme' to see available themes."}});
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

    if (g_debug_mode) {
        std::cerr << "DEBUG: Loaded theme from '" << display_path << "'" << std::endl;
    }

    return 0;
}

int Shell::execute(const std::string& script) {
    if (script.empty()) {
        return 0;
    }
    std::string processed_script = script;
    if (!get_menu_active()) {
        if (script == ":") {
            set_menu_active(true);
            return 0;
        } else if (script[0] == ':') {
            processed_script = script.substr(1);
        } else {
            return do_ai_request(script);
        }
    }
    std::vector<std::string> lines;

    lines = shell_parser->parse_into_lines(processed_script);

    if (g_debug_mode) {
        std::cerr << "DEBUG: Executing script with " << lines.size() << " lines:" << std::endl;
        for (size_t i = 0; i < lines.size(); i++) {
            std::cerr << "DEBUG:   Line " << (i + 1) << ": " << lines[i] << std::endl;
        }
    }

    if (shell_script_interpreter) {
        int exit_code = shell_script_interpreter->execute_block(lines);
        last_command = processed_script;
        return exit_code;
    } else {
        print_error(ErrorInfo{ErrorType::RUNTIME_ERROR, "", "No script interpreter available", {"Restart cjsh"}});
        return 1;
    }
}

void Shell::setup_signal_handlers() {
    signal_handler->setup_signal_handlers();
}

void Shell::setup_interactive_handlers() {
    signal_handler->setup_interactive_handlers();
}

void Shell::save_terminal_state() {
    if (g_debug_mode)
        std::cerr << "DEBUG: Saving terminal state" << std::endl;

    if (isatty(STDIN_FILENO)) {
        if (tcgetattr(STDIN_FILENO, &shell_tmodes) == 0) {
            terminal_state_saved = true;
        }
    }
}

void Shell::restore_terminal_state() {
    if (interactive_mode) {
        std::cerr << "Restoring terminal state." << std::endl;
    }

    if (terminal_state_saved) {
        if (tcsetattr(STDIN_FILENO, TCSANOW, &shell_tmodes) != 0) {
            if (g_debug_mode) {
                std::cerr << "DEBUG: Warning - failed to restore terminal state: " << strerror(errno) << std::endl;
            }

            tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_tmodes);
        }
        terminal_state_saved = false;
    }

    fflush(stdout);
    fflush(stderr);
}

void Shell::setup_job_control() {
    if (g_debug_mode)
        std::cerr << "DEBUG: Setting up job control" << std::endl;

    if (!isatty(STDIN_FILENO)) {
        job_control_enabled = false;
        return;
    }

    shell_pgid = getpid();

    if (setpgid(shell_pgid, shell_pgid) < 0) {
        if (errno != EPERM) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "setpgid",
                         "couldn't put the shell in its own process group: " + std::string(strerror(errno)),
                         {}});
        }
    }

    try {
        shell_terminal = STDIN_FILENO;

        int tpgrp = tcgetpgrp(shell_terminal);
        if (tpgrp != -1) {
            if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
                print_error({ErrorType::RUNTIME_ERROR, "tcsetpgrp", "couldn't grab terminal control: " + std::string(strerror(errno)), {}});
            }
        }

        job_control_enabled = true;
    } catch (const std::exception& e) {
        print_error({ErrorType::RUNTIME_ERROR, "", e.what(), {"Check terminal settings", "Restart cjsh"}});
        job_control_enabled = false;
    }
}

int Shell::do_ai_request(const std::string& command) {
    if (command == "exit" || command == "clear" || command == "quit") {
        return execute(command);
    }
    std::string first_word;
    std::istringstream iss(command);
    iss >> first_word;

    if (!first_word.empty()) {
        auto cached_executables = cjsh_filesystem::read_cached_executables();
        std::unordered_set<std::string> available_commands = get_available_commands();

        bool is_executable = false;
        for (const auto& exec_path : cached_executables) {
            if (exec_path.filename().string() == first_word) {
                is_executable = true;
                break;
            }
        }

        if (!is_executable && available_commands.find(first_word) != available_commands.end()) {
            is_executable = true;
        }

        if (is_executable) {
            std::cout << "It looks like you're trying to run a command '" << first_word
                      << "' in AI mode. Did you mean to run it as a shell "
                         "command? (y/n): ";
            std::string response;
            std::getline(std::cin, response);

            if (!response.empty() && (response[0] == 'y' || response[0] == 'Y')) {
                std::vector<std::string> lines = shell_parser->parse_into_lines(command);
                if (shell_script_interpreter) {
                    int exit_code = shell_script_interpreter->execute_block(lines);
                    last_command = command;
                    return exit_code;
                } else {
                    print_error(ErrorInfo{ErrorType::RUNTIME_ERROR, "", "No script interpreter available", {"Restart cjsh"}});
                    return 1;
                }
            }
        }
    }

    return built_ins->do_ai_request(command);
}

int Shell::execute_command(std::vector<std::string> args, bool run_in_background) {
    if (g_debug_mode)
        std::cerr << "DEBUG: Executing command: '" << args[0] << "'" << std::endl;

    if (args.empty()) {
        return 0;
    }
    if (!shell_exec || !built_ins) {
        g_exit_flag = true;
        print_error({ErrorType::RUNTIME_ERROR, "", "Shell not properly initialized", {"Restart cjsh"}});
        return 1;
    }

    if (args.size() == 1 && shell_parser) {
        std::string var_name, var_value;
        if (shell_parser->is_env_assignment(args[0], var_name, var_value)) {
            shell_parser->expand_env_vars(var_value);

            env_vars[var_name] = var_value;

            if (var_name == "PATH" || var_name == "PWD" || var_name == "HOME" || var_name == "USER" || var_name == "SHELL") {
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

    if (!args.empty() && built_ins->is_builtin_command(args[0])) {
        int code = built_ins->builtin_command(args);
        last_terminal_output_error = built_ins->get_last_error();

        if (args[0] == "break" || args[0] == "continue" || args[0] == "return") {
            if (g_debug_mode)
                std::cerr << "DEBUG: Detected loop control command: " << args[0] << " with exit code " << code << std::endl;
        }

        return code;
    }

    if (g_plugin) {
        std::vector<std::string> enabled_plugins = g_plugin->get_enabled_plugins();
        if (!args.empty() && !enabled_plugins.empty()) {
            for (const auto& plugin : enabled_plugins) {
                std::vector<std::string> plugin_commands = g_plugin->get_plugin_commands(plugin);
                if (std::find(plugin_commands.begin(), plugin_commands.end(), args[0]) != plugin_commands.end()) {
                    return g_plugin->handle_plugin_command(plugin, args) ? 0 : 1;
                }
            }
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

                if (g_debug_mode) {
                    std::cerr << "DEBUG: Background job " << job_id << " started with PID " << last_pid << std::endl;
                }
            }
        }
        last_terminal_output_error = "Background command launched";
        return 0;
    } else {
        shell_exec->execute_command_sync(args);
        last_terminal_output_error = shell_exec->get_error_string();
        int exit_code = shell_exec->get_exit_code();

        if (exit_code == 0 && !args.empty()) {
            suggestion_utils::update_command_usage_stats(args[0]);
        }

        if (exit_code != 0) {
            ErrorInfo error = shell_exec->get_error();
            if (error.type != ErrorType::RUNTIME_ERROR || error.message.find("command failed with exit code") == std::string::npos) {
                shell_exec->print_last_error();
            }
        }
        return exit_code;
    }
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
    if (g_plugin) {
        auto enabled_plugins = g_plugin->get_enabled_plugins();
        for (const auto& plugin : enabled_plugins) {
            auto plugin_commands = g_plugin->get_plugin_commands(plugin);
            cmds.insert(plugin_commands.begin(), plugin_commands.end());
        }
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
        positional_parameters.erase(positional_parameters.begin(), positional_parameters.begin() + count);
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
    if (g_debug_mode) {
        std::cerr << "DEBUG: Set shell option '" << option << "' to " << (value ? "true" : "false") << std::endl;
    }
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
    if (g_debug_mode)
        std::cerr << "DEBUG: Syncing shell env_vars cache from system environment" << std::endl;

    extern char** environ;
    for (char** env = environ; *env != nullptr; env++) {
        std::string env_str(*env);
        size_t eq_pos = env_str.find('=');
        if (eq_pos != std::string::npos) {
            std::string name = env_str.substr(0, eq_pos);
            std::string value = env_str.substr(eq_pos + 1);
            env_vars[name] = value;

            if (g_debug_mode && name == "PATH") {
                std::cerr << "DEBUG: Synced PATH=" << value << std::endl;
            }
        }
    }

    if (shell_parser) {
        shell_parser->set_env_vars(env_vars);
    }
}

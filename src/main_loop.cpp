#include "main_loop.h"

#include <fcntl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#ifdef __APPLE__
#include <AvailabilityMacros.h>
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#include <sys/select.h>
#include <sys/time.h>

#include "cjsh.h"
#include "cjsh_completions.h"
#include "cjsh_filesystem.h"
#include "cjsh_syntax_highlighter.h"
#include "cjshopt_command.h"
#include "error_out.h"
#include "exec.h"
#include "history_expansion.h"
#include "interpreter.h"
#include "isocline.h"
#include "job_control.h"
#include "pipeline_status_utils.h"
#include "prompt.h"
#include "shell.h"
#include "shell_env.h"
#include "status_line.h"
#include "trap_command.h"
#include "typeahead.h"
#include "version_command.h"

std::chrono::steady_clock::time_point& startup_begin_time() {
    static std::chrono::steady_clock::time_point value;
    return value;
}

namespace {

struct CommandInfo {
    std::string command;
    std::string history_entry;
    bool available = false;
};

struct TerminalStatus {
    bool terminal_alive;
    bool parent_alive;
};

enum class TerminalCheckLevel : std::uint8_t {
    QUICK,
    RESPONSIVE,
    COMPREHENSIVE
};

bool last_prompt_started_with_newline = false;

TerminalStatus check_terminal_health(TerminalCheckLevel level = TerminalCheckLevel::COMPREHENSIVE) {
    TerminalStatus status{true, true};

    if (!config::interactive_mode) {
        return status;
    }

    if ((isatty(STDIN_FILENO) == 0) || (isatty(STDOUT_FILENO) == 0)) {
        status.terminal_alive = false;
        return status;
    }

    if (level == TerminalCheckLevel::QUICK) {
        return status;
    }

    fd_set readfds;
    struct timeval timeout{};
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int result = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout);
    if (result > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
        char test_buf = 0;
        ssize_t bytes = read(STDIN_FILENO, &test_buf, 0);
        if (bytes == 0) {
            status.terminal_alive = false;
            return status;
        }
        if (bytes < 0 && (errno == ECONNRESET || errno == EIO || errno == ENXIO)) {
            status.terminal_alive = false;
            return status;
        }
    }

    if (level == TerminalCheckLevel::RESPONSIVE) {
        return status;
    }

    if (isatty(STDERR_FILENO) == 0) {
        status.terminal_alive = false;
        return status;
    }

    pid_t tpgrp = tcgetpgrp(STDIN_FILENO);
    if (tpgrp == -1) {
        if (errno == ENOTTY || errno == ENXIO || errno == EIO) {
            status.terminal_alive = false;
            return status;
        }
    }

    char* tty_name = ttyname(STDIN_FILENO);
    if (tty_name == nullptr) {
        status.terminal_alive = false;
        return status;
    }

    pid_t parent_pid = getppid();
    if (parent_pid == 1 || (kill(parent_pid, 0) == -1 && errno == ESRCH)) {
        status.parent_alive = false;
    }

    return status;
}

bool process_command_line(const std::string& command) {
    if (command.empty()) {
        return g_exit_flag;
    }

    ++g_command_sequence;

    std::string expanded_command = command;
    if (config::history_expansion_enabled && isatty(STDIN_FILENO)) {
        auto history_entries = HistoryExpansion::read_history_entries();
        auto expansion_result = HistoryExpansion::expand(command, history_entries);

        if (expansion_result.has_error) {
            print_error({ErrorType::RUNTIME_ERROR,
                         ErrorSeverity::ERROR,
                         "history-expansion",
                         expansion_result.error_message,
                         {"Review your history expansion syntax or disable '!' expansions."}});
            setenv("?", "1", 1);
            return g_exit_flag;
        }

        if (expansion_result.was_expanded) {
            expanded_command = expansion_result.expanded_command;
            if (expansion_result.should_echo) {
                std::cout << expanded_command << '\n';
            }
        }
    }

    g_shell->execute_hooks("preexec");
    trap_manager_execute_debug_trap();

    int exit_code = g_shell->execute(expanded_command);

    Exec* exec_ptr = (g_shell && g_shell->shell_exec) ? g_shell->shell_exec.get() : nullptr;
    pipeline_status_utils::apply_pipeline_status_env(exec_ptr);

    std::string status_str = std::to_string(exit_code);

    ic_history_add_with_exit_code(command.c_str(), exit_code);
    setenv("?", status_str.c_str(), 1);

#if defined(__APPLE__) && MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
    (void)malloc_zone_pressure_relief(nullptr, 0);
#elif defined(__linux__)
    malloc_trim(0);
#else
    // do nothing for other platforms
#endif

    std::string typeahead_input = typeahead::capture_available_input();
    if (!typeahead_input.empty()) {
        typeahead::ingest_typeahead_input(typeahead_input);
    }

    return g_exit_flag;
}

bool perform_terminal_check() {
    TerminalStatus status = check_terminal_health(TerminalCheckLevel::QUICK);
    if (!status.terminal_alive) {
        g_exit_flag = true;
        return false;
    }
    return true;
}

void update_job_management() {
    JobManager::instance().update_job_status();
    JobManager::instance().cleanup_finished_jobs();
}

std::string generate_prompt(bool command_was_available) {
    std::printf(" \r");
    (void)std::fflush(stdout);
    const bool prompt_cleanup_enabled = ic_prompt_cleanup_is_enabled();
    const bool prompt_cleanup_newline = ic_prompt_cleanup_newline_is_enabled();
    const size_t extra_cleanup_lines =
        (prompt_cleanup_enabled && prompt_cleanup_newline && command_was_available) ? 1 : 0;
    ic_enable_prompt_cleanup(prompt_cleanup_enabled, extra_cleanup_lines);
    return prompt::render_primary_prompt();
}

bool handle_null_input() {
    TerminalStatus status = check_terminal_health(TerminalCheckLevel::COMPREHENSIVE);

    if (!status.terminal_alive || !status.parent_alive) {
        g_exit_flag = true;
        return true;
    }
    return false;
}

std::pair<std::string, bool> get_next_command(bool command_was_available) {
    std::string command_to_run;
    bool command_available = false;

    g_shell->execute_hooks("precmd");
    prompt::execute_prompt_command();

    cjsh_env::update_terminal_dimensions();

    std::string prompt = generate_prompt(command_was_available);
    last_prompt_started_with_newline = (!prompt.empty() && prompt.front() == '\n');
    std::string inline_right_text = prompt::render_right_prompt();
    std::string continuation_prompt = prompt::render_secondary_prompt();
    if (continuation_prompt.empty()) {
        ic_set_prompt_marker("", nullptr);
    } else {
        ic_set_prompt_marker("", continuation_prompt.c_str());
    }

    thread_local static std::string sanitized_buffer;
    sanitized_buffer.clear();

    typeahead::flush_pending_typeahead();
    const std::string& pending_buffer = typeahead::get_input_buffer();
    if (!pending_buffer.empty()) {
        sanitized_buffer.reserve(pending_buffer.size());
        typeahead::filter_escape_sequences_into(pending_buffer, sanitized_buffer);
    }

    const char* initial_input = sanitized_buffer.empty() ? nullptr : sanitized_buffer.c_str();
    const char* inline_right_ptr = inline_right_text.empty() ? nullptr : inline_right_text.c_str();
    char* input = ic_readline(prompt.c_str(), inline_right_ptr, initial_input);
    typeahead::clear_input_buffer();
    sanitized_buffer.clear();

    if (input == nullptr) {
        if (handle_null_input()) {
            return {command_to_run, false};
        }
        return {command_to_run, false};
    }

    command_to_run.assign(input);
    if (input != nullptr) {
        ic_free(input);
        input = nullptr;
    }

    if (command_to_run == IC_READLINE_TOKEN_CTRL_D) {
        g_exit_flag = true;
        return {std::string(), false};
    }

    if (command_to_run == IC_READLINE_TOKEN_CTRL_C) {
        return {std::string(), false};
    }

    command_available = true;

    return {command_to_run, command_available};
}

bool handle_runoff_bind(ic_keycode_t key, void*) {
    if (has_custom_keybinding(key)) {
        std::string command = get_custom_keybinding(key);
        if (!command.empty()) {
            const char* buffer = ic_get_buffer();
            size_t cursor_pos = 0;
            ic_get_cursor_pos(&cursor_pos);

            std::string original_buffer = buffer ? buffer : "";

            setenv("CJSH_LINE", original_buffer.c_str(), 1);
            setenv("CJSH_POINT", std::to_string(cursor_pos).c_str(), 1);

            g_shell->execute(command);

            const char* new_buffer_env = getenv("CJSH_LINE");
            const char* new_point_env = getenv("CJSH_POINT");

            if (new_buffer_env && original_buffer != new_buffer_env) {
                ic_set_buffer(new_buffer_env);
            }

            if (new_point_env) {
                char* endptr;
                long new_pos = strtol(new_point_env, &endptr, 10);
                if (endptr != new_point_env && new_pos >= 0) {
                    ic_set_cursor_pos((size_t)new_pos);
                }
            }

            unsetenv("CJSH_LINE");
            unsetenv("CJSH_POINT");

            return true;
        }
    }
    return false;
}

bool should_show_creator_line() {
    const char* env = std::getenv("CJSH_SHOW_CREATED");
    if (env == nullptr || env[0] == '\0') {
        return false;
    }

    std::string value(env);
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    (void)unsetenv("CJSH_SHOW_CREATED");

    return value == "1" || value == "true" || value == "yes" || value == "on";
}

}  // namespace

void initialize_isocline() {
    initialize_completion_system();
    SyntaxHighlighter::initialize_syntax_highlighting();
    ic_enable_history_duplicates(false);
    ic_set_prompt_marker("", nullptr);
    ic_set_unhandled_key_handler(handle_runoff_bind, nullptr);
    ic_set_status_message_callback(status_line::create_below_syntax_message, nullptr);
    if (!config::status_line_enabled) {
        (void)ic_set_status_hint_mode(IC_STATUS_HINT_OFF);
    }
}

void main_process_loop() {
    typeahead::initialize();
    prompt::apply_terminal_window_title();

    std::string command_to_run;
    bool command_available = false;

    while (true) {
        g_shell->process_pending_signals();

        if (g_exit_flag) {
            break;
        }

        if (!perform_terminal_check()) {
            break;
        }

        update_job_management();

        std::tie(command_to_run, command_available) = get_next_command(command_available);

        if (g_exit_flag) {
            break;
        }

        if (!command_available) {
            continue;
        }

        bool exit_requested = process_command_line(command_to_run);
        if (exit_requested || g_exit_flag) {
            break;
        }

        if (config::newline_after_execution && command_to_run != "clear" && command_available &&
            !last_prompt_started_with_newline && ic_prompt_cleanup_is_enabled()) {
            (void)std::fputc('\n', stdout);
            (void)std::fflush(stdout);
            command_available = false;
        }
    }

    typeahead::cleanup();
}

void start_interactive_process() {
    initialize_isocline();
    g_startup_active = false;
    bool first_boot = cjsh_filesystem::is_first_boot();

    std::chrono::microseconds startup_duration(0);
    if (config::show_startup_time || first_boot) {
        auto startup_end_time = std::chrono::steady_clock::now();
        startup_duration = std::chrono::duration_cast<std::chrono::microseconds>(
            startup_end_time - startup_begin_time());
    }

    if (config::show_title_line) {
        const bool show_creator_line = should_show_creator_line();
        std::cout << " CJ's Shell v" << get_version() << " - Caden J Finley (c) 2026" << '\n';
        if (show_creator_line) {
            // cjsh first started as part of an undergrad project at my alma mater, ACU ( abilene
            // christian univeristy ), to create some shell paradigms and shell/ gnu builtins, and
            // eventually a full shell project and i fell in love with the project. That is the
            // reason for this line. I wanted to give the school credit that helped me fall in love
            // with my main project. Most people couldn't care less which is why this is guareded
            // behind a hidden option. But for those who do care like myself the option is still
            // there to have this line appear in the title line during startup.
            std::cout << " Created 2024 @ \033[1;35mAbilene Christian University\033[0m" << '\n';
        }
        std::cout << "\n";
    }

    if (first_boot) {
        std::cout << " Be sure to give us a star on GitHub!" << '\n';
        std::cout << " Type 'help' to see available commands and options." << '\n';
        std::cout << " For additional help and documentation, please visit: "
                  << " https://cadenfinley.github.io/CJsShell/" << '\n';
        std::cout << '\n';

        std::cout << " To suppress this help message run the command: 'touch "
                  << cjsh_filesystem::g_cjsh_first_boot_path().string() << "'" << '\n';
        std::cout << " To suppress the title line, put this command in .cjprofile: 'cjshopt "
                     "login-startup-arg --no-titleline'"
                  << '\n';
        std::cout << " Or alternatively execute cjsh with this flag: --no-titleline" << '\n';
        std::cout << " You can find many more toggles like this to fully customize your cjsh "
                     "experience with: 'cjshopt --help'\n";
        std::cout << '\n';
        std::cout << " cjsh uses a very complex, but very smart completions system.\n";
        std::cout << " During shell use it learns about the commands you use and provides better "
                     "completions as you use cjsh.\n";
        std::cout << " If you would like to skip the learning process and make all completions "
                     "faster please see: 'generate-completions --help'\n";
        std::cout << "\n";
    }

    if (config::show_startup_time || first_boot) {
        long long microseconds = startup_duration.count();
        std::string startup_time_str;
        if (microseconds < 1000) {
            startup_time_str = std::to_string(microseconds) + "μs";
        } else if (microseconds < 1000000) {
            double milliseconds = static_cast<double>(microseconds) / 1000.0;
            char buffer[32];
            (void)snprintf(buffer, sizeof(buffer), "%.2fms", milliseconds);
            startup_time_str = buffer;
        } else {
            double seconds = static_cast<double>(microseconds) / 1000000.0;
            char buffer[32];
            (void)snprintf(buffer, sizeof(buffer), "%.2fs", seconds);
            startup_time_str = buffer;
        }
        std::cout << " Started in " << startup_time_str << '\n';
        std::cout << "\n";
    }

    if (!config::startup_test) {
        main_process_loop();
    }
}

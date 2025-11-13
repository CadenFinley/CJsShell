#include "main_loop.h"

#include <fcntl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#ifdef __APPLE__
#include <AvailabilityMacros.h>
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#include <sys/select.h>
#include <sys/time.h>

#include "builtin/trap_command.h"
#include "cjsh.h"
#include "cjsh_completions.h"
#include "cjsh_syntax_highlighter.h"
#include "cjshopt_command.h"
#include "error_out.h"
#include "exec.h"
#include "history_expansion.h"
#include "isocline.h"
#include "job_control.h"
#include "prompt.h"
#include "shell.h"
#include "shell_env.h"
#include "typeahead.h"

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
    if (exec_ptr != nullptr) {
        const std::vector<int>& pipeline_statuses = exec_ptr->get_last_pipeline_statuses();
        if (!pipeline_statuses.empty()) {
            std::stringstream status_builder;
            for (size_t i = 0; i < pipeline_statuses.size(); ++i) {
                if (i != 0) {
                    status_builder << ' ';
                }
                status_builder << pipeline_statuses[i];
            }

            const std::string pipe_status_str = status_builder.str();
            setenv("PIPESTATUS", pipe_status_str.c_str(), 1);
        } else {
            unsetenv("PIPESTATUS");
        }
    } else {
        unsetenv("PIPESTATUS");
    }

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
    // if (config::uses_cleanup && command_was_available) {
    //     std::printf(" \n");
    // }
    ic_enable_prompt_cleanup(
        config::uses_cleanup,
        (config::cleanup_newline_after_execution && command_was_available) ? 1 : 0);
    ic_enable_prompt_cleanup_empty_line(config::cleanup_adds_empty_line);
    ic_enable_prompt_cleanup_truncate_multiline(config::cleanup_truncates_multiline);
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

std::string previous_passed_buffer;

const char* create_below_syntax_message(const char* input_buffer, void*) {
    static thread_local std::string status_message;

    // avoid recomputing if the input buffer hasn't changed
    if (previous_passed_buffer == input_buffer)
        return status_message.c_str();

    previous_passed_buffer = input_buffer;

    // no current input, so no needed status message
    if (input_buffer == nullptr || input_buffer[0] == '\0') {
        status_message.clear();
        return nullptr;
    }

    status_message.assign("this is the users current input: ");
    status_message.append(input_buffer);
    return status_message.c_str();
}

}  // namespace

void initialize_isocline() {
    initialize_completion_system();
    SyntaxHighlighter::initialize_syntax_highlighting();
    ic_enable_history_duplicates(false);
    ic_set_prompt_marker("", nullptr);
    ic_set_unhandled_key_handler(handle_runoff_bind, nullptr);
    ic_set_status_message_callback(create_below_syntax_message, nullptr);
}

void main_process_loop() {
    typeahead::initialize();

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
            if (g_exit_flag) {
                std::cerr << "Exiting main process loop..." << '\n';
            }
            break;
        }

        if (config::newline_after_execution && command_to_run != "clear" && command_available &&
            !last_prompt_started_with_newline && config::uses_cleanup) {
            (void)std::fputc('\n', stdout);
            (void)std::fflush(stdout);
            command_available = false;
        }
    }

    typeahead::cleanup();
}

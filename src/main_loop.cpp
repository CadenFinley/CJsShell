#include "main_loop.h"

#include <fcntl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <iostream>

#ifdef __APPLE__
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#include <sys/select.h>
#include <sys/time.h>

#include "cjsh.h"
#include "cjsh_completions.h"
#include "isocline.h"
#include "job_control.h"
#include "shell.h"
#include "theme.h"
#include "typeahead.h"

namespace {

struct TerminalStatus {
    bool terminal_alive;
    bool parent_alive;
};

enum class TerminalCheckLevel : std::uint8_t {
    QUICK,
    RESPONSIVE,
    COMPREHENSIVE
};

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
        g_shell->reset_command_timing();
        return g_exit_flag;
    }

    g_shell->start_command_timing();
    int exit_code = g_shell->execute(command);
    g_shell->end_command_timing(exit_code);

    std::string status_str = std::to_string(exit_code);

    ic_history_add(command.c_str());
    setenv("?", status_str.c_str(), 1);

#ifdef __APPLE__
    (void)malloc_zone_pressure_relief(nullptr, 0);
#elif defined(__linux__)
    malloc_trim(0);
#else
    g_shell->execute("echo '' > /dev/null");
#endif

    std::string typeahead_input = typeahead::capture_available_input();
    if (!typeahead_input.empty()) {
        typeahead::ingest_typeahead_input(typeahead_input);
    }

    return g_exit_flag;
}
}  // namespace

static void update_terminal_title() {
    std::cout << "\033]0;" << g_shell->get_title_prompt() << "\007";
    std::cout.flush();
}

static bool perform_terminal_check() {
    TerminalStatus status = check_terminal_health(TerminalCheckLevel::QUICK);
    if (!status.terminal_alive) {
        g_exit_flag = true;
        return false;
    }
    return true;
}

static void update_job_management() {
    JobManager::instance().update_job_status();
    JobManager::instance().cleanup_finished_jobs();
}

static std::string generate_prompt() {
    std::printf(" \r");
    (void)std::fflush(stdout);

    std::string prompt = g_shell->get_prompt();

    if (g_theme && g_theme->uses_newline()) {
        prompt += "\n";
        prompt += g_shell->get_newline_prompt();
    }
    if (g_theme) {
        ic_enable_prompt_cleanup(g_theme->uses_cleanup(), g_theme->cleanup_nl_after_exec() ? 1 : 0);
        ic_enable_prompt_cleanup_empty_line(g_theme->cleanup_adds_empty_line());
    }

    return prompt;
}

static bool handle_null_input() {
    TerminalStatus status = check_terminal_health(TerminalCheckLevel::COMPREHENSIVE);

    if (!status.terminal_alive || !status.parent_alive) {
        g_exit_flag = true;
        return true;
    }
    return false;
}

static std::pair<std::string, bool> get_next_command() {
    std::string command_to_run;
    bool command_available = false;

    std::string prompt = generate_prompt();
    std::string inline_right_text = g_shell->get_inline_right_prompt();

    typeahead::flush_pending_typeahead();

    const std::string& pending_buffer = typeahead::get_input_buffer();
    thread_local static std::string sanitized_buffer;
    sanitized_buffer.clear();
    if (!pending_buffer.empty()) {
        sanitized_buffer.reserve(pending_buffer.size());
        typeahead::filter_escape_sequences_into(pending_buffer, sanitized_buffer);
    }

    const char* initial_input = sanitized_buffer.empty() ? nullptr : sanitized_buffer.c_str();
    char* input = nullptr;
    if (!inline_right_text.empty()) {
        input = ic_readline_inline(prompt.c_str(), inline_right_text.c_str(), initial_input);
    } else {
        input = ic_readline(prompt.c_str(), initial_input);
    }
    typeahead::clear_input_buffer();
    sanitized_buffer.clear();

    if (input == nullptr) {
        if (handle_null_input()) {
            return {command_to_run, false};
        }
        g_shell->reset_command_timing();
        return {command_to_run, false};
    }

    command_to_run.assign(input);
    ic_free(input);
    command_available = true;

    return {command_to_run, command_available};
}

void main_process_loop() {
    initialize_completion_system();
    typeahead::initialize();

    typeahead::flush_pending_typeahead();

    while (true) {
        g_shell->process_pending_signals();

        if (g_exit_flag) {
            break;
        }

        if (!perform_terminal_check()) {
            break;
        }

        update_job_management();

        update_terminal_title();

        typeahead::flush_pending_typeahead();

        auto [command_to_run, command_available] = get_next_command();

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
        if (g_theme && g_theme->newline_after_execution()) {
            (void)std::fputc('\n', stdout);
            (void)std::fflush(stdout);
        }
    }

    typeahead::cleanup();
}

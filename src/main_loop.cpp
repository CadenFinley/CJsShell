#include "main_loop.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

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
#include "plugin.h"
#include "shell.h"
#include "theme.h"
#include "typeahead.h"

void notify_plugins(const std::string& trigger, const std::string& data) {
    if (g_plugin == nullptr) {
        if (g_debug_mode)
            std::cerr << "DEBUG: notify_plugins: plugin manager is nullptr"
                      << std::endl;
        return;
    }
    if (g_plugin->get_enabled_plugins().empty()) {
        if (g_debug_mode)
            std::cerr << "DEBUG: notify_plugins: no enabled plugins"
                      << std::endl;
        return;
    }
    if (g_debug_mode) {
        std::cerr << "DEBUG: Notifying plugins of trigger: " << trigger
                  << " with data: " << data << std::endl;
    }
    g_plugin->trigger_subscribed_global_event(trigger, data);
}

namespace {

struct TerminalStatus {
    bool terminal_alive;
    bool parent_alive;
};

enum class TerminalCheckLevel {
    QUICK,
    RESPONSIVE,
    COMPREHENSIVE
};

TerminalStatus check_terminal_health(
    TerminalCheckLevel level = TerminalCheckLevel::COMPREHENSIVE) {
    TerminalStatus status{true, true};

    if (!config::interactive_mode) {
        return status;
    }

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        status.terminal_alive = false;
        if (g_debug_mode) {
            std::cerr << "DEBUG: Standard file descriptors no longer TTY"
                      << std::endl;
        }
        return status;
    }

    if (level == TerminalCheckLevel::QUICK) {
        return status;
    }

    fd_set readfds;
    struct timeval timeout;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int result = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout);
    if (result > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
        char test_buf;
        ssize_t bytes = read(STDIN_FILENO, &test_buf, 0);
        if (bytes == 0) {
            status.terminal_alive = false;
            if (g_debug_mode) {
                std::cerr << "DEBUG: EOF detected on stdin, terminal closed"
                          << std::endl;
            }
            return status;
        } else if (bytes < 0 &&
                   (errno == ECONNRESET || errno == EIO || errno == ENXIO)) {
            status.terminal_alive = false;
            if (g_debug_mode) {
                std::cerr << "DEBUG: Terminal connection broken (errno="
                          << errno << ")" << std::endl;
            }
            return status;
        }
    }

    if (level == TerminalCheckLevel::RESPONSIVE) {
        return status;
    }

    if (!isatty(STDERR_FILENO)) {
        status.terminal_alive = false;
        if (g_debug_mode) {
            std::cerr << "DEBUG: STDERR no longer a TTY" << std::endl;
        }
        return status;
    }

    pid_t tpgrp = tcgetpgrp(STDIN_FILENO);
    if (tpgrp == -1) {
        if (errno == ENOTTY || errno == ENXIO || errno == EIO) {
            status.terminal_alive = false;
            if (g_debug_mode) {
                std::cerr
                    << "DEBUG: Lost controlling terminal (tcgetpgrp failed)"
                    << std::endl;
            }
            return status;
        }
    } else {
        pid_t our_pgrp = getpgrp();
        if (tpgrp != our_pgrp && g_debug_mode) {
            std::cerr << "DEBUG: Not in foreground process group (tpgrp="
                      << tpgrp << ", our_pgrp=" << our_pgrp << ")" << std::endl;
        }
    }

    char* tty_name = ttyname(STDIN_FILENO);
    if (tty_name == nullptr) {
        status.terminal_alive = false;
        if (g_debug_mode) {
            std::cerr << "DEBUG: Cannot get terminal name, terminal closed"
                      << std::endl;
        }
        return status;
    }

    pid_t parent_pid = getppid();
    if (parent_pid == 1) {
        status.parent_alive = false;
        if (g_debug_mode) {
            std::cerr << "DEBUG: Parent process appears to have died (PPID=1)"
                      << std::endl;
        }
    } else if (kill(parent_pid, 0) == -1 && errno == ESRCH) {
        status.parent_alive = false;
        if (g_debug_mode) {
            std::cerr << "DEBUG: Parent process no longer exists" << std::endl;
        }
    }

    return status;
}

bool process_command_line(const std::string& command) {
    if (command.empty()) {
        if (g_debug_mode) {
            std::cerr << "DEBUG: Received empty command" << std::endl;
        }
        g_shell->reset_command_timing();
        return g_exit_flag;
    }

    g_shell->start_command_timing();
    int exit_code = g_shell->execute(command);
    g_shell->end_command_timing(exit_code);

    std::string status_str = std::to_string(exit_code);

    if (g_debug_mode) {
        std::cerr << "DEBUG: Command exit status: " << status_str << std::endl;
    }

    ic_history_add(command.c_str());
    setenv("?", status_str.c_str(), 1);

#ifdef __APPLE__
    malloc_zone_pressure_relief(nullptr, 0);
#elif defined(__linux__)
    malloc_trim(0);
#else
    g_shell->execute("echo '' > /dev/null");
#endif

    std::string typeahead_input = typeahead::capture_available_input();
    if (g_debug_mode) {
        if (typeahead_input.empty()) {
            std::cerr
                << "DEBUG: Post-command typeahead capture returned no data"
                << std::endl;
        } else {
            std::cerr << "DEBUG: Post-command typeahead capture (len="
                      << typeahead_input.size() << "): '"
                      << typeahead::to_debug_visible(typeahead_input) << "'"
                      << std::endl;
        }
    }
    if (!typeahead_input.empty()) {
        typeahead::ingest_typeahead_input(typeahead_input);
    }

    if (g_theme && g_theme->newline_after_execution()) {
        std::fputc('\n', stdout);
        std::fflush(stdout);
    }

    return g_exit_flag;
}
}  // namespace

void update_terminal_title() {
    if (g_debug_mode) {
        std::cout << "\033]0;" << "<<<DEBUG MODE ENABLED>>>" << "\007";
        std::cout.flush();
        return;
    }
    std::cout << "\033]0;" << g_shell->get_title_prompt() << "\007";
    std::cout.flush();
}

bool perform_terminal_check() {
    TerminalStatus status = check_terminal_health(TerminalCheckLevel::QUICK);
    if (!status.terminal_alive) {
        if (g_debug_mode) {
            std::cerr << "DEBUG: Fast check: terminal no longer alive, exiting"
                      << std::endl;
        }
        g_exit_flag = true;
        return false;
    }
    return true;
}

void update_job_management() {
    if (g_debug_mode)
        std::cerr << "DEBUG: Calling JobManager::update_job_status()"
                  << std::endl;
    JobManager::instance().update_job_status();

    if (g_debug_mode)
        std::cerr << "DEBUG: Calling JobManager::cleanup_finished_jobs()"
                  << std::endl;
    JobManager::instance().cleanup_finished_jobs();
}

std::string generate_prompt() {
    if (g_debug_mode)
        std::cerr << "DEBUG: Generating prompt" << std::endl;

    std::printf(" \r");
    std::fflush(stdout);

    std::chrono::steady_clock::time_point render_time_start;
    if (g_debug_mode) {
        render_time_start = std::chrono::steady_clock::now();
    }

    std::string prompt;
    if (g_shell->get_menu_active()) {
        prompt = g_shell->get_prompt();
    } else {
        prompt = g_shell->get_ai_prompt();
    }
    if (g_theme && g_theme->uses_newline()) {
        prompt += "\n";
        prompt += g_shell->get_newline_prompt();
    }
    if (g_theme) {
        ic_enable_prompt_cleanup(g_theme->uses_cleanup());
        ic_enable_prompt_cleanup_empty_line(g_theme->cleanup_adds_empty_line());
    }

    if (g_debug_mode) {
        auto render_time_end = std::chrono::steady_clock::now();
        auto render_duration =
            std::chrono::duration_cast<std::chrono::microseconds>(
                render_time_end - render_time_start);
        std::cerr << "DEBUG: Prompt rendering took " << render_duration.count()
                  << "μs" << std::endl;
    }

    return prompt;
}

bool handle_null_input() {
    if (g_debug_mode) {
        std::cerr << "DEBUG: ic_readline returned NULL (could be EOF/Ctrl+D, "
                     "interrupt/Ctrl+C, or terminal closed)"
                  << std::endl;
    }

    TerminalStatus status =
        check_terminal_health(TerminalCheckLevel::COMPREHENSIVE);

    if (!status.terminal_alive || !status.parent_alive) {
        if (g_debug_mode) {
            std::cerr
                << "DEBUG: Terminal or parent process dead, setting exit flag"
                << std::endl;
        }
        g_exit_flag = true;
        return true;
    } else {
        if (g_debug_mode) {
            std::cerr << "DEBUG: Terminal and parent alive, treating as "
                         "interrupt - continuing loop"
                      << std::endl;
        }
        return false;
    }
}

std::pair<std::string, bool> get_next_command() {
    std::string command_to_run;
    bool command_available = false;

    std::string prompt = generate_prompt();
    std::string inline_right_text = g_shell->get_inline_right_prompt();

    if (g_debug_mode) {
        std::cerr << "DEBUG: About to call ic_readline with prompt: '" << prompt
                  << "'" << std::endl;
        if (!inline_right_text.empty()) {
            std::cerr << "DEBUG: Inline right text: '" << inline_right_text
                      << "'" << std::endl;
        }
    }

    typeahead::flush_pending_typeahead();

    std::string sanitized_buffer = typeahead::get_input_buffer();
    if (!sanitized_buffer.empty()) {
        sanitized_buffer = typeahead::filter_escape_sequences(sanitized_buffer);
        if (g_debug_mode && sanitized_buffer != typeahead::get_input_buffer()) {
            std::cerr
                << "DEBUG: Additional sanitization applied to input buffer"
                << std::endl;
        }
    }

    const char* initial_input =
        sanitized_buffer.empty() ? nullptr : sanitized_buffer.c_str();
    char* input = nullptr;
    if (!inline_right_text.empty()) {
        input = ic_readline_inline(prompt.c_str(), inline_right_text.c_str(),
                                   initial_input);
    } else {
        input = ic_readline(prompt.c_str(), initial_input);
    }
    typeahead::clear_input_buffer();

    if (g_debug_mode) {
        std::cerr << "DEBUG: ic_readline returned" << std::endl;
    }

    if (input == nullptr) {
        if (handle_null_input()) {
            return {command_to_run, false};
        } else {
            g_shell->reset_command_timing();
            return {command_to_run, false};
        }
    }

    command_to_run.assign(input);
    ic_free(input);
    command_available = true;

    if (g_debug_mode) {
        std::cerr << "DEBUG: User input: " << command_to_run << std::endl;
    }

    return {command_to_run, command_available};
}

void main_process_loop() {
    if (g_debug_mode)
        std::cerr << "DEBUG: Entering main process loop" << std::endl;

    initialize_completion_system();
    typeahead::initialize();

    typeahead::flush_pending_typeahead();

    notify_plugins("main_process_pre_run", "");

    while (true) {
        if (g_debug_mode) {
            std::cerr << "---------------------------------------" << std::endl;
            std::cerr << "DEBUG: Starting new command input cycle" << std::endl;
        }

        g_shell->process_pending_signals();

        if (g_exit_flag) {
            if (g_debug_mode) {
                std::cerr << "DEBUG: Exit flag set after processing signals, "
                             "breaking main loop"
                          << std::endl;
            }
            break;
        }

        if (!perform_terminal_check()) {
            break;
        }

        update_job_management();

        if (g_debug_mode)
            std::cerr << "DEBUG: Calling update_terminal_title()" << std::endl;
        update_terminal_title();

        typeahead::flush_pending_typeahead();

        notify_plugins("main_process_start", "");

        auto [command_to_run, command_available] = get_next_command();

        if (g_exit_flag) {
            break;
        }

        if (!command_available) {
            continue;
        }

        notify_plugins("main_process_command_processed", command_to_run);

        bool exit_requested = process_command_line(command_to_run);
        notify_plugins("main_process_end", "");
        if (exit_requested || g_exit_flag) {
            if (g_exit_flag) {
                std::cerr << "Exiting main process loop..." << std::endl;
            }
            break;
        }
    }

    notify_plugins("main_process_exit", "");

    typeahead::cleanup();
}
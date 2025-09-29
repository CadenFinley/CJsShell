#include "main_loop.h"

#include <cctype>
#include <chrono>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

#ifdef __APPLE__
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#include "cjsh.h"
#include "cjsh_completions.h"
#include "isocline.h"
#include "job_control.h"
#include "typeahead.h"

namespace {

std::string g_input_buffer;
std::deque<std::string> g_typeahead_queue;

struct TerminalStatus {
    bool terminal_alive;
    bool parent_alive;
};

constexpr std::size_t kMaxQueuedCommands = 32;

std::string to_debug_visible(const std::string& data) {
    if (data.empty()) {
        return "";
    }

    std::ostringstream oss;
    oss << std::hex << std::uppercase;
    for (unsigned char ch : data) {
        switch (ch) {
            case '\\':
                oss << "\\\\";
                break;
            case '\n':
                oss << "\\n";
                break;
            case '\r':
                oss << "\\r";
                break;
            case '\t':
                oss << "\\t";
                break;
            case '\f':
                oss << "\\f";
                break;
            case '\v':
                oss << "\\v";
                break;
            case '\b':
                oss << "\\b";
                break;
            case '\a':
                oss << "\\a";
                break;
            case '\0':
                oss << "\\0";
                break;
            case 0x1B:
                oss << "\\e";
                break;
            default:
                if (std::isprint(ch)) {
                    oss << static_cast<char>(ch);
                } else {
                    oss << "\\x" << std::setw(2) << std::setfill('0')
                        << static_cast<int>(ch);
                }
                break;
        }
    }
    oss << std::dec;
    return oss.str();
}

std::string filter_escape_sequences(const std::string& input) {
    if (input.empty()) {
        return input;
    }

    std::string filtered;
    filtered.reserve(input.size());
    
    for (std::size_t i = 0; i < input.size(); ++i) {
        unsigned char ch = input[i];
        
        if (ch == '\x1b' && i + 1 < input.size()) {
            // Handle escape sequences
            char next = input[i + 1];
            std::size_t seq_start = i;
            
            if (next == '[') {
                // ANSI CSI sequence - skip until we find the terminator
                i += 2; // Skip ESC[
                while (i < input.size()) {
                    char c = input[i];
                    // CSI sequences end with a letter (A-Za-z) or certain symbols
                    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        c == '~' || c == 'c' || c == 'h' || c == 'l' ||
                        c == 'm' || c == 'n' || c == 'r' || c == 'J' ||
                        c == 'K' || c == 'H' || c == 'f') {
                        break;
                    }
                    // Skip over parameters (digits, semicolons, etc.)
                    if (!((c >= '0' && c <= '9') || c == ';' || c == '?' || 
                          c == '!' || c == '=' || c == '>' || c == '<')) {
                        // Invalid character in CSI sequence, abort
                        break;
                    }
                    i++;
                }
                if (g_debug_mode) {
                    std::cerr << "DEBUG: Filtered ANSI CSI escape sequence: " 
                              << to_debug_visible(input.substr(seq_start, i - seq_start + 1))
                              << std::endl;
                }
            } else if (next == ']') {
                // OSC sequence - skip until ST (\x1b\\) or BEL (\x07)
                i += 2; // Skip ESC]
                while (i < input.size()) {
                    if (input[i] == '\x07') {
                        break;
                    } else if (input[i] == '\x1b' && i + 1 < input.size() && input[i + 1] == '\\') {
                        i++;
                        break;
                    }
                    i++;
                }
                if (g_debug_mode) {
                    std::cerr << "DEBUG: Filtered OSC escape sequence: " 
                              << to_debug_visible(input.substr(seq_start, i - seq_start + 1))
                              << std::endl;
                }
            } else if (next == '(' || next == ')') {
                // Character set selection - skip 3 characters total
                if (i + 2 < input.size()) {
                    i += 2;
                } else {
                    i += 1;
                }
                if (g_debug_mode) {
                    std::cerr << "DEBUG: Filtered character set escape sequence: " 
                              << to_debug_visible(input.substr(seq_start, i - seq_start + 1))
                              << std::endl;
                }
            } else if (next >= '0' && next <= '9') {
                // Potential private escape sequence, skip it
                i += 1;
                while (i + 1 < input.size() && input[i + 1] >= '0' && input[i + 1] <= '9') {
                    i++;
                }
                if (g_debug_mode) {
                    std::cerr << "DEBUG: Filtered numeric escape sequence: " 
                              << to_debug_visible(input.substr(seq_start, i - seq_start + 1))
                              << std::endl;
                }
            } else {
                // Single character escape sequence (like ESC c for reset)
                i += 1;
                if (g_debug_mode) {
                    std::cerr << "DEBUG: Filtered single-char escape sequence: " 
                              << to_debug_visible(input.substr(seq_start, i - seq_start + 1))
                              << std::endl;
                }
            }
        } else if (ch == '\x07') {
            // BEL character - potentially part of incomplete escape sequence
            if (g_debug_mode) {
                std::cerr << "DEBUG: Filtered BEL character" << std::endl;
            }
        } else if (ch < 0x20 && ch != '\t' && ch != '\n' && ch != '\r') {
            // Filter out other control characters except tab, newline, carriage return
            if (g_debug_mode) {
                std::cerr << "DEBUG: Filtered control character: \\x" 
                          << std::hex << std::setw(2) << std::setfill('0') 
                          << static_cast<int>(ch) << std::dec << std::endl;
            }
        } else {
            // Regular character, keep it
            filtered.push_back(static_cast<char>(ch));
        }
    }
    
    return filtered;
}

std::string normalize_line_edit_sequences(const std::string& input) {
    std::string normalized;
    normalized.reserve(input.size());

    for (unsigned char ch : input) {
        switch (ch) {
            case '\b':
            case 0x7F: {
                if (!normalized.empty()) {
                    normalized.pop_back();
                }
                break;
            }
            case 0x15: {
                while (!normalized.empty() && normalized.back() != '\n') {
                    normalized.pop_back();
                }
                break;
            }
            case 0x17: {
                while (!normalized.empty() && (normalized.back() == ' ' ||
                                               normalized.back() == '\t')) {
                    normalized.pop_back();
                }
                while (!normalized.empty() && normalized.back() != ' ' &&
                       normalized.back() != '\t' && normalized.back() != '\n') {
                    normalized.pop_back();
                }
                break;
            }
            default:
                normalized.push_back(static_cast<char>(ch));
                break;
        }
    }

    return normalized;
}

void enqueue_queued_command(const std::string& command) {
    if (g_typeahead_queue.size() >= kMaxQueuedCommands) {
        if (g_debug_mode) {
            std::cerr << "DEBUG: Typeahead queue full, dropping oldest entry"
                      << std::endl;
        }
        g_typeahead_queue.pop_front();
    }

    std::string sanitized_command = filter_escape_sequences(command);
    
    if (g_debug_mode && sanitized_command != command) {
        std::cerr << "DEBUG: Command sanitized before queuing: '"
                  << to_debug_visible(command) << "' -> '"
                  << to_debug_visible(sanitized_command) << "'" << std::endl;
    }

    g_typeahead_queue.push_back(sanitized_command);

    if (g_debug_mode) {
        std::cerr << "DEBUG: Queued typeahead command: '"
                  << to_debug_visible(sanitized_command) << "'" << std::endl;
    }
}

void ingest_typeahead_input(const std::string& raw_input) {
    if (raw_input.empty()) {
        return;
    }

    if (g_debug_mode) {
        std::cerr << "DEBUG: ingest_typeahead_input raw (len="
                  << raw_input.size() << "): '" << to_debug_visible(raw_input)
                  << "'" << std::endl;
    }

    std::string combined = g_input_buffer;
    g_input_buffer.clear();
    combined += raw_input;

    if (g_debug_mode) {
        std::cerr << "DEBUG: ingest_typeahead_input combined buffer (len="
                  << combined.size() << "): '" << to_debug_visible(combined)
                  << "'" << std::endl;
    }

    if (combined.find('\x1b') != std::string::npos) {
        if (g_debug_mode) {
            std::cerr
                << "DEBUG: Found escape sequences in typeahead input, filtering..."
                << std::endl;
        }
        combined = filter_escape_sequences(combined);
        if (g_debug_mode) {
            std::cerr
                << "DEBUG: After filtering escape sequences: '" 
                << to_debug_visible(combined) << "'" << std::endl;
        }
    }

    std::string normalized = normalize_line_edit_sequences(combined);

    if (g_debug_mode) {
        std::cerr << "DEBUG: ingest_typeahead_input normalized (len="
                  << normalized.size() << "): '" << to_debug_visible(normalized)
                  << "'" << std::endl;
    }

    std::size_t start = 0;
    while (start < normalized.size()) {
        std::size_t newline_pos = normalized.find('\n', start);
        if (newline_pos == std::string::npos) {
            g_input_buffer += normalized.substr(start);
            if (g_debug_mode && !g_input_buffer.empty()) {
                std::cerr << "DEBUG: ingest_typeahead_input buffered prefill: '"
                          << to_debug_visible(g_input_buffer) << "'"
                          << std::endl;
            }
            break;
        }

        std::string line = normalized.substr(start, newline_pos - start);
        enqueue_queued_command(line);
        start = newline_pos + 1;

        if (start == normalized.size()) {
            g_input_buffer.clear();
        }
    }

    if (!normalized.empty() && normalized.back() == '\n') {
        g_input_buffer.clear();
        if (g_debug_mode) {
            std::cerr << "DEBUG: ingest_typeahead_input cleared buffer after "
                         "trailing newline"
                      << std::endl;
        }
    }

    if (g_debug_mode && !g_input_buffer.empty()) {
        std::cerr << "DEBUG: Buffered typeahead prefill: '"
                  << to_debug_visible(g_input_buffer) << "'" << std::endl;
    }
}

void flush_pending_typeahead() {
    std::string pending_input = typeahead::capture_available_input();
    if (!pending_input.empty()) {
        if (g_debug_mode) {
            std::cerr << "DEBUG: flush_pending_typeahead captured (len="
                      << pending_input.size() << "): '"
                      << to_debug_visible(pending_input) << "'" << std::endl;
        }
        ingest_typeahead_input(pending_input);
    } else if (g_debug_mode) {
        std::cerr << "DEBUG: flush_pending_typeahead captured no data"
                  << std::endl;
    }
}

bool process_command_line(const std::string& command) {
    if (command.empty()) {
        if (g_debug_mode) {
            std::cerr << "DEBUG: Received empty command" << std::endl;
        }
        g_shell->reset_command_timing();
        return g_exit_flag;
    }

    notify_plugins("main_process_command_processed", command);

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
                      << to_debug_visible(typeahead_input) << "'" << std::endl;
        }
    }
    if (!typeahead_input.empty()) {
        ingest_typeahead_input(typeahead_input);
    }

    return g_exit_flag;
}
}  // namespace

void update_terminal_title() {
    if (g_debug_mode) {
        std::cout << "\033]0;" << "<<<DEBUG MODE ENABLED>>>" << "\007";
        std::cout.flush();
    }
    std::cout << "\033]0;" << g_shell->get_title_prompt() << "\007";
    std::cout.flush();
}

void reprint_prompt() {
    if (g_debug_mode) {
        std::cerr << "DEBUG: Reprinting prompt" << std::endl;
    }

    update_terminal_title();

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
    ic_print_prompt(prompt.c_str(), false);
}

bool check_terminal_health() {
    if (!config::interactive_mode) {
        return true;
    }
    
    if (!isatty(STDIN_FILENO)) {
        // STDIN is no longer a TTY - terminal was likely closed
        if (g_debug_mode) {
            std::cerr << "DEBUG: STDIN is no longer a TTY, exiting" << std::endl;
        }
        g_exit_flag = true;
        return false;
    }
    
    // Try to get the process group of the controlling terminal
    pid_t tpgrp = tcgetpgrp(STDIN_FILENO);
    if (tpgrp == -1) {
        if (errno == ENOTTY || errno == ENXIO) {
            // Terminal is no longer controlling terminal - exit immediately
            if (g_debug_mode) {
                std::cerr << "DEBUG: Controlling terminal lost (errno=" << errno << "), exiting" << std::endl;
            }
            g_exit_flag = true;
            return false;
        }
    }
    
    // Additional safety: check if we can write to the terminal
    if (write(STDOUT_FILENO, "", 0) == -1 && (errno == EBADF || errno == EIO)) {
        if (g_debug_mode) {
            std::cerr << "DEBUG: Cannot write to terminal (errno=" << errno << "), exiting" << std::endl;
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

    if (g_debug_mode) {
        auto render_time_end = std::chrono::steady_clock::now();
        auto render_duration =
            std::chrono::duration_cast<std::chrono::microseconds>(
                render_time_end - render_time_start);
        std::cerr << "DEBUG: Prompt rendering took "
                  << render_duration.count() << "μs" << std::endl;
    }

    return prompt;
}

TerminalStatus check_terminal_and_parent_status() {
    TerminalStatus status{true, true};
    
    if (config::interactive_mode) {
        // Check if we still have a controlling terminal
        if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
            status.terminal_alive = false;
            if (g_debug_mode) {
                std::cerr << "DEBUG: Terminal is no longer a TTY" << std::endl;
            }
        } else {
            // Try to get the process group of the controlling terminal
            pid_t tpgrp = tcgetpgrp(STDIN_FILENO);
            if (tpgrp == -1 && (errno == ENOTTY || errno == ENXIO)) {
                status.terminal_alive = false;
                if (g_debug_mode) {
                    std::cerr << "DEBUG: Lost controlling terminal" << std::endl;
                }
            }
        }
        
        // Check if parent process is still alive
        pid_t parent_pid = getppid();
        if (parent_pid == 1) {
            // Parent PID is 1 (init), which usually means our parent died
            status.parent_alive = false;
            if (g_debug_mode) {
                std::cerr << "DEBUG: Parent process appears to have died (PPID=1)" << std::endl;
            }
        } else if (kill(parent_pid, 0) == -1 && errno == ESRCH) {
            // Parent process doesn't exist
            status.parent_alive = false;
            if (g_debug_mode) {
                std::cerr << "DEBUG: Parent process no longer exists" << std::endl;
            }
        }
    }
    
    return status;
}

bool handle_null_input() {
    if (g_debug_mode) {
        std::cerr << "DEBUG: ic_readline returned NULL (could be EOF/Ctrl+D, interrupt/Ctrl+C, or terminal closed)" << std::endl;
    }
    
    // Check if we're still in a live terminal session and parent is alive
    TerminalStatus status = check_terminal_and_parent_status();
    
    // Only exit if terminal is dead or parent is dead
    // If both are alive, this was likely just Ctrl+C, so continue the loop
    if (!status.terminal_alive || !status.parent_alive) {
        if (g_debug_mode) {
            std::cerr << "DEBUG: Terminal or parent process dead, setting exit flag" << std::endl;
        }
        g_exit_flag = true;
        notify_plugins("main_process_end", "");
        return true; // Should exit
    } else {
        // Terminal and parent are alive, this was likely Ctrl+C - continue loop
        if (g_debug_mode) {
            std::cerr << "DEBUG: Terminal and parent alive, treating as interrupt - continuing loop" << std::endl;
        }
        notify_plugins("main_process_end", "");
        return false; // Should continue
    }
}

std::pair<std::string, bool> get_next_command() {
    std::string command_to_run;
    bool command_available = false;

    if (!g_typeahead_queue.empty()) {
        command_to_run = g_typeahead_queue.front();
        g_typeahead_queue.pop_front();
        command_available = true;
        if (g_debug_mode) {
            std::cerr << "DEBUG: Dequeued queued command: '"
                      << command_to_run << "'" << std::endl;
        }
    } else {
        std::string prompt = generate_prompt();
        std::string inline_right_text = g_shell->get_inline_right_prompt();

        if (g_debug_mode) {
            std::cerr << "DEBUG: About to call ic_readline with prompt: '"
                      << prompt << "'" << std::endl;
            if (!inline_right_text.empty()) {
                std::cerr << "DEBUG: Inline right text: '"
                          << inline_right_text << "'" << std::endl;
            }
        }

        flush_pending_typeahead();

        std::string sanitized_buffer = g_input_buffer;
        if (!sanitized_buffer.empty()) {
            sanitized_buffer = filter_escape_sequences(sanitized_buffer);
            if (g_debug_mode && sanitized_buffer != g_input_buffer) {
                std::cerr << "DEBUG: Additional sanitization applied to input buffer"
                          << std::endl;
            }
        }

        const char* initial_input =
            sanitized_buffer.empty() ? nullptr : sanitized_buffer.c_str();
        char* input = nullptr;
        if (!inline_right_text.empty()) {
            input = ic_readline_inline(
                prompt.c_str(), inline_right_text.c_str(), initial_input);
        } else {
            input = ic_readline(prompt.c_str(), initial_input);
        }
        g_input_buffer.clear();

        if (g_debug_mode) {
            std::cerr << "DEBUG: ic_readline returned" << std::endl;
        }

        if (input == nullptr) {
            // handle_null_input returns true if we should exit the main loop
            if (handle_null_input()) {
                return {command_to_run, false}; // Exit requested
            } else {
                return {command_to_run, false}; // Continue loop, no command available
            }
        }

        command_to_run.assign(input);
        ic_free(input);
        command_available = true;

        if (g_debug_mode) {
            std::cerr << "DEBUG: User input: " << command_to_run
                      << std::endl;
        }
    }

    return {command_to_run, command_available};
}

void main_process_loop() {
    if (g_debug_mode)
        std::cerr << "DEBUG: Entering main process loop" << std::endl;
    notify_plugins("main_process_pre_run", "");

    initialize_completion_system();
    typeahead::initialize();

    g_input_buffer.clear();
    g_typeahead_queue.clear();

    flush_pending_typeahead();

    while (true) {
        if (g_debug_mode) {
            std::cerr << "---------------------------------------" << std::endl;
            std::cerr << "DEBUG: Starting new command input cycle" << std::endl;
        }
        notify_plugins("main_process_start", "");

        g_shell->process_pending_signals();
        
        // Check if we should exit immediately after processing signals
        if (g_exit_flag) {
            if (g_debug_mode) {
                std::cerr << "DEBUG: Exit flag set after processing signals, breaking main loop" << std::endl;
            }
            break;
        }
        
        // Check if controlling terminal is still valid (detect terminal closure)
        if (!check_terminal_health()) {
            break;
        }

        update_job_management();

        if (g_debug_mode)
            std::cerr << "DEBUG: Calling update_terminal_title()" << std::endl;
        update_terminal_title();

        flush_pending_typeahead();

        auto [command_to_run, command_available] = get_next_command();
        
        // Check if get_next_command requested an exit
        if (g_exit_flag) {
            break;
        }

        if (!command_available) {
            notify_plugins("main_process_end", "");
            continue;
        }

        bool exit_requested = process_command_line(command_to_run);
        notify_plugins("main_process_end", "");
        if (exit_requested || g_exit_flag) {
            if (g_exit_flag) {
                std::cerr << "Exiting main process loop..." << std::endl;
            }
            break;
        }
    }

    typeahead::cleanup();
}

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
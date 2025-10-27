#include "main_loop.h"

#include <fcntl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

#ifdef __APPLE__
#include <malloc/malloc.h>
#include <AvailabilityMacros.h>
#else
#include <malloc.h>
#endif

#include <sys/select.h>
#include <sys/time.h>

#include "cjsh.h"
#include "cjsh_completions.h"
#include "cjsh_syntax_highlighter.h"
#include "cjshopt_command.h"
#include "history_expansion.h"
#include "isocline.h"
#include "job_control.h"
#include "shell.h"
#include "theme.h"
#include "typeahead.h"

namespace {

struct HeredocInfo {
    bool has_heredoc = false;
    bool strip_tabs = false;
    std::string delimiter;
    size_t operator_pos = std::string::npos;
};

struct CommandInfo {
    std::string command;
    std::string history_entry;
    bool available = false;
};

HeredocInfo detect_heredoc(const std::string& command) {
    HeredocInfo info;

    size_t pos = 0;
    bool in_single_quote = false;
    bool in_double_quote = false;

    while (pos < command.length()) {
        char c = command[pos];

        if (c == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            pos++;
            continue;
        }
        if (c == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            pos++;
            continue;
        }

        if (in_single_quote || in_double_quote) {
            pos++;
            continue;
        }

        if (c == '<' && pos + 1 < command.length() && command[pos + 1] == '<') {
            info.has_heredoc = true;
            info.operator_pos = pos;

            if (pos + 2 < command.length() && command[pos + 2] == '-') {
                info.strip_tabs = true;
                pos += 3;
            } else {
                pos += 2;
            }

            while (pos < command.length() &&
                   std::isspace(static_cast<unsigned char>(command[pos])) != 0) {
                pos++;
            }

            bool delimiter_quoted = false;
            char quote_char = '\0';

            if (pos < command.length() &&
                (command[pos] == '\'' || command[pos] == '"' || command[pos] == '\\')) {
                quote_char = command[pos];
                delimiter_quoted = true;
                pos++;
            }

            while (pos < command.length()) {
                char dc = command[pos];

                if (delimiter_quoted && dc == quote_char) {
                    break;
                }
                if (!delimiter_quoted &&
                    (std::isspace(static_cast<unsigned char>(dc)) != 0 || dc == ';' || dc == '&' ||
                     dc == '|' || dc == '<' || dc == '>')) {
                    break;
                }

                info.delimiter += dc;
                pos++;
            }

            return info;
        }

        pos++;
    }

    return info;
}

std::pair<std::string, std::vector<std::string>> process_heredoc_command(
    const std::string& initial_command) {
    HeredocInfo info = detect_heredoc(initial_command);

    if (!info.has_heredoc || info.delimiter.empty()) {
        return {initial_command, {}};
    }

    char* heredoc_content = ic_read_heredoc(info.delimiter.c_str(), info.strip_tabs);

    if (heredoc_content == nullptr) {
        return {"", {}};
    }

    std::unique_ptr<char, decltype(&free)> heredoc_ptr(heredoc_content, &free);

    std::vector<std::string> history_entries;
    history_entries.emplace_back(initial_command);

    std::string heredoc_string(heredoc_content);
    std::size_t line_start = 0;
    while (line_start < heredoc_string.size()) {
        std::size_t newline_pos = heredoc_string.find('\n', line_start);
        if (newline_pos == std::string::npos) {
            history_entries.emplace_back(heredoc_string.substr(line_start));
            break;
        }
        history_entries.emplace_back(heredoc_string.substr(line_start, newline_pos - line_start));
        line_start = newline_pos + 1;
    }

    char temp_template[] = "/tmp/cjsh_heredoc_XXXXXX";
    int temp_fd = mkstemp(temp_template);

    if (temp_fd == -1) {
        std::cerr << "cjsh: failed to create temporary file for heredoc\n";
        return {"", {}};
    }

    ssize_t written = write(temp_fd, heredoc_content, strlen(heredoc_content));
    if (written == -1) {
        std::cerr << "cjsh: failed to write heredoc content\n";
        close(temp_fd);
        unlink(temp_template);
        return {"", {}};
    }

    close(temp_fd);

    std::string reconstructed;

    reconstructed += initial_command.substr(0, info.operator_pos);

    reconstructed += "< ";
    reconstructed += temp_template;

    size_t after_delimiter = info.operator_pos + 2;
    if (info.strip_tabs) {
        after_delimiter++;
    }

    while (after_delimiter < initial_command.length() &&
           std::isspace(static_cast<unsigned char>(initial_command[after_delimiter])) != 0) {
        after_delimiter++;
    }

    bool found_quote = false;
    if (after_delimiter < initial_command.length()) {
        char fc = initial_command[after_delimiter];
        if (fc == '\'' || fc == '"' || fc == '\\') {
            found_quote = true;
            after_delimiter++;
        }
    }

    size_t delim_len = info.delimiter.length();
    after_delimiter += delim_len;

    if (found_quote && after_delimiter < initial_command.length()) {
        after_delimiter++;
    }

    if (after_delimiter < initial_command.length()) {
        reconstructed += initial_command.substr(after_delimiter);
    }

    reconstructed += "; rm -f ";
    reconstructed += temp_template;

    return {reconstructed, history_entries};
}

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

bool process_command_line(const std::string& command, bool skip_history = false) {
    if (command.empty()) {
        g_shell->reset_command_timing();
        return g_exit_flag;
    }

    std::string expanded_command = command;
    if (config::history_expansion_enabled && isatty(STDIN_FILENO)) {
        auto history_entries = HistoryExpansion::read_history_entries();
        auto expansion_result = HistoryExpansion::expand(command, history_entries);

        if (expansion_result.has_error) {
            std::cerr << "cjsh: " << expansion_result.error_message << std::endl;
            setenv("?", "1", 1);
            return g_exit_flag;
        }

        if (expansion_result.was_expanded) {
            expanded_command = expansion_result.expanded_command;
            if (expansion_result.should_echo) {
                std::cout << expanded_command << std::endl;
            }
        }
    }

    g_shell->execute_hooks("preexec");

    g_shell->start_command_timing();
    int exit_code = g_shell->execute(expanded_command);
    g_shell->end_command_timing(exit_code);

    std::string status_str = std::to_string(exit_code);

    if (!skip_history) {
        ic_history_add(command.c_str());
    }
    setenv("?", status_str.c_str(), 1);

#if defined(__APPLE__) && MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
    (void)malloc_zone_pressure_relief(nullptr, 0);
#elif defined(__linux__)
    malloc_trim(0);
#else
    g_shell->execute("echo '' > /dev/null");
#endif

    if (!config::posix_mode) {
        std::string typeahead_input = typeahead::capture_available_input();
        if (!typeahead_input.empty()) {
            typeahead::ingest_typeahead_input(typeahead_input);
        }
    }

    return g_exit_flag;
}

void update_terminal_title() {
    std::cout << "\033]0;" << g_shell->get_title_prompt() << "\007";
    std::cout.flush();
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
    // std::printf(" \r");
    // (void)std::fflush(stdout);

    if (config::no_prompt) {
        return "# ";
    }

    std::string prompt = g_shell->get_prompt();

    if (g_theme && g_theme->uses_newline()) {
        prompt += "\n";
        prompt += g_shell->get_newline_prompt();
    }
    if (g_theme) {
        ic_enable_prompt_cleanup(
            g_theme->uses_cleanup(),
            (g_theme->cleanup_nl_after_exec() && command_was_available) ? 1 : 0);
        ic_enable_prompt_cleanup_empty_line(g_theme->cleanup_adds_empty_line());
        ic_enable_prompt_cleanup_truncate_multiline(g_theme->cleanup_truncates_multiline());
    }

    return prompt;
}

bool handle_null_input() {
    TerminalStatus status = check_terminal_health(TerminalCheckLevel::COMPREHENSIVE);

    if (!status.terminal_alive || !status.parent_alive) {
        g_exit_flag = true;
        return true;
    }
    return false;
}

std::pair<std::string, bool> get_next_command(bool command_was_available,
                                              bool& history_already_added) {
    std::string command_to_run;
    bool command_available = false;
    history_already_added = false;

    g_shell->execute_hooks("precmd");

    std::string prompt = generate_prompt(command_was_available);
    std::string inline_right_text = g_shell->get_inline_right_prompt();

    thread_local static std::string sanitized_buffer;
    sanitized_buffer.clear();

    if (!config::posix_mode) {
        typeahead::flush_pending_typeahead();
        const std::string& pending_buffer = typeahead::get_input_buffer();
        if (!pending_buffer.empty()) {
            sanitized_buffer.reserve(pending_buffer.size());
            typeahead::filter_escape_sequences_into(pending_buffer, sanitized_buffer);
        }
    } else {
        typeahead::clear_input_buffer();
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
        g_shell->reset_command_timing();
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
        g_shell->reset_command_timing();
        return {std::string(), false};
    }

    command_available = true;

    HeredocInfo heredoc_info = detect_heredoc(command_to_run);
    if (heredoc_info.has_heredoc && !heredoc_info.delimiter.empty()) {
        auto [processed_command, history_entries] = process_heredoc_command(command_to_run);
        if (processed_command.empty()) {
            g_shell->reset_command_timing();
            return {std::string(), false};
        }

        if (!history_entries.empty()) {
            for (const auto& entry : history_entries) {
                ic_history_add(entry.c_str());
            }
            history_already_added = true;
        }

        command_to_run = processed_command;
    }

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

}  // namespace

void initialize_isocline() {
    initialize_completion_system();
    SyntaxHighlighter::initialize_syntax_highlighting();
    ic_enable_history_duplicates(false);
    ic_set_prompt_marker("", nullptr);
    ic_set_unhandled_key_handler(handle_runoff_bind, nullptr);
}

void main_process_loop() {
    initialize_isocline();
    typeahead::initialize();

    std::string command_to_run;
    bool command_available = false;
    bool history_already_added = false;

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

        std::tie(command_to_run, command_available) =
            get_next_command(command_available, history_already_added);

        if (g_exit_flag) {
            break;
        }

        if (!command_available) {
            continue;
        }

        bool exit_requested = process_command_line(command_to_run, history_already_added);
        if (exit_requested || g_exit_flag) {
            if (g_exit_flag) {
                std::cerr << "Exiting main process loop..." << '\n';
            }
            break;
        }
        if (g_theme && g_theme->newline_after_execution() && command_available &&
            (command_to_run != "clear")) {
            (void)std::fputc('\n', stdout);
            (void)std::fflush(stdout);
        }

        history_already_added = false;
    }

    typeahead::cleanup();
}

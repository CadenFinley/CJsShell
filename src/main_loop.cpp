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
#include <exception>
#include <iostream>
#include <memory>
#include <vector>

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
#include "trap_command.h"
#include "typeahead.h"
#include "version_command.h"

std::chrono::steady_clock::time_point& startup_begin_time() {
    static std::chrono::steady_clock::time_point value;
    return value;
}

namespace {

std::string sanitize_for_status(const std::string& text) {
    if (text.empty()) {
        return {};
    }

    std::string sanitized;
    sanitized.reserve(text.size());
    bool previous_space = false;

    for (char ch : text) {
        unsigned char uch = static_cast<unsigned char>(ch);
        char normalized = ch;

        if (normalized == '\n' || normalized == '\r' || normalized == '\t') {
            normalized = ' ';
        }

        if (std::iscntrl(uch) && normalized != ' ') {
            continue;
        }

        if (normalized == ' ') {
            if (sanitized.empty() || previous_space) {
                previous_space = true;
                continue;
            }
            previous_space = true;
        } else {
            previous_space = false;
        }

        sanitized.push_back(normalized);
    }

    size_t start = sanitized.find_first_not_of(' ');
    if (start == std::string::npos) {
        return {};
    }
    if (start > 0) {
        sanitized.erase(0, start);
    }

    size_t end = sanitized.find_last_not_of(' ');
    if (end != std::string::npos && end + 1 < sanitized.size()) {
        sanitized.erase(end + 1);
    }

    return sanitized;
}

const char* severity_to_label(ErrorSeverity severity) {
    switch (severity) {
        case ErrorSeverity::CRITICAL:
            return "critical";
        case ErrorSeverity::ERROR:
            return "error";
        case ErrorSeverity::WARNING:
            return "warning";
        case ErrorSeverity::INFO:
        default:
            return "info";
    }
}

constexpr const char* kAnsiReset = "\x1b[0m";

const char* severity_to_underline_style(ErrorSeverity severity) {
    switch (severity) {
        case ErrorSeverity::CRITICAL:
            return "\x1b[4m\x1b[58;5;196m";
        case ErrorSeverity::ERROR:
            return "\x1b[4m\x1b[58;5;160m";
        case ErrorSeverity::WARNING:
            return "\x1b[4m\x1b[58;5;214m";
        case ErrorSeverity::INFO:
        default:
            return "\x1b[4m\x1b[58;5;51m";
    }
}

std::string format_error_location(const ShellScriptInterpreter::SyntaxError& error) {
    const auto& pos = error.position;
    if (pos.line_number == 0) {
        return {};
    }

    std::string location = "line ";
    location += std::to_string(pos.line_number);
    if (pos.column_start > 0) {
        location += ", col ";
        location += std::to_string(pos.column_start + 1);
    }
    return location;
}

std::string build_validation_status_message(
    const std::vector<ShellScriptInterpreter::SyntaxError>& errors) {
    if (errors.empty()) {
        return {};
    }

    struct SeverityBuckets {
        size_t info = 0;
        size_t warning = 0;
        size_t error = 0;
        size_t critical = 0;
    } buckets;

    std::vector<const ShellScriptInterpreter::SyntaxError*> sorted_errors;
    sorted_errors.reserve(errors.size());

    for (const auto& err : errors) {
        switch (err.severity) {
            case ErrorSeverity::CRITICAL:
                ++buckets.critical;
                break;
            case ErrorSeverity::ERROR:
                ++buckets.error;
                break;
            case ErrorSeverity::WARNING:
                ++buckets.warning;
                break;
            case ErrorSeverity::INFO:
            default:
                ++buckets.info;
                break;
        }

        sorted_errors.push_back(&err);
    }

    auto severity_rank = [](ErrorSeverity severity) {
        switch (severity) {
            case ErrorSeverity::CRITICAL:
                return 3;
            case ErrorSeverity::ERROR:
                return 2;
            case ErrorSeverity::WARNING:
                return 1;
            case ErrorSeverity::INFO:
            default:
                return 0;
        }
    };

    std::sort(sorted_errors.begin(), sorted_errors.end(), [&](const auto* lhs, const auto* rhs) {
        int lhs_rank = severity_rank(lhs->severity);
        int rhs_rank = severity_rank(rhs->severity);
        if (lhs_rank != rhs_rank) {
            return lhs_rank > rhs_rank;
        }
        if (lhs->position.line_number != rhs->position.line_number) {
            return lhs->position.line_number < rhs->position.line_number;
        }
        if (lhs->position.column_start != rhs->position.column_start) {
            return lhs->position.column_start < rhs->position.column_start;
        }
        return lhs->message < rhs->message;
    });

    if (sorted_errors.empty()) {
        return {};
    }

    std::vector<std::string> counter_parts;
    counter_parts.reserve(4);

    auto append_part = [&counter_parts](size_t count, const char* label) {
        if (count == 0) {
            return;
        }
        std::string part = std::to_string(count);
        part.push_back(' ');
        part.append(label);
        if (count > 1) {
            part.push_back('s');
        }
        counter_parts.push_back(std::move(part));
    };

    append_part(buckets.critical, "critical");
    append_part(buckets.error, "error");
    append_part(buckets.warning, "warning");
    append_part(buckets.info, "info");

    std::string message;
    message.reserve(256);

    for (size_t i = 0; i < sorted_errors.size(); ++i) {
        const auto* error = sorted_errors[i];

        if (i != 0) {
            message.push_back('\n');
        }

        std::string line;
        line.reserve(128);
        line.push_back('[');
        line.append(severity_to_label(error->severity));
        line.append("]");

        std::string location = format_error_location(*error);
        std::string sanitized_text = sanitize_for_status(error->message);
        std::string sanitized_suggestion = sanitize_for_status(error->suggestion);
        std::string detail_text;

        if (!location.empty()) {
            detail_text.append(location);
        }
        if (!sanitized_text.empty()) {
            if (!detail_text.empty()) {
                detail_text.append(" - ");
            }
            detail_text.append(sanitized_text);
        }
        if (!sanitized_suggestion.empty()) {
            if (!detail_text.empty()) {
                detail_text.append(" | ");
            }
            // detail_text.append("Suggestion: ");
            detail_text.append(sanitized_suggestion);
        }

        if (!detail_text.empty()) {
            line.push_back(' ');
            line.append(detail_text);
        }

        const char* style_prefix = severity_to_underline_style(error->severity);
        if (style_prefix != nullptr && style_prefix[0] != '\0') {
            message.append(style_prefix);
            message.append(line);
            message.append(kAnsiReset);
        } else {
            message.append(line);
        }
    }

    return message;
}

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

std::string previous_passed_buffer;

const char* create_below_syntax_message(const char* input_buffer, void*) {
    static thread_local std::string status_message;

    const std::string current_input = (input_buffer != nullptr) ? input_buffer : "";

    if (previous_passed_buffer == current_input) {
        return status_message.empty() ? nullptr : status_message.c_str();
    }

    previous_passed_buffer = current_input;

    if (current_input.empty()) {
        status_message.clear();
        return nullptr;
    }

    bool has_visible_content = std::any_of(current_input.begin(), current_input.end(), [](char ch) {
        return std::isspace(static_cast<unsigned char>(ch)) == 0;
    });

    if (!has_visible_content) {
        status_message.clear();
        return nullptr;
    }

    Shell* shell = g_shell.get();
    if (shell == nullptr) {
        status_message.clear();
        return nullptr;
    }

    ShellScriptInterpreter* interpreter = shell->get_shell_script_interpreter();
    if (interpreter == nullptr) {
        status_message.clear();
        return nullptr;
    }

    std::vector<std::string> lines = interpreter->parse_into_lines(current_input);
    if (lines.empty()) {
        lines.emplace_back(current_input);
    }

    std::vector<ShellScriptInterpreter::SyntaxError> errors;
    try {
        errors = interpreter->validate_comprehensive_syntax(lines);
    } catch (const std::exception& ex) {
        status_message.assign("Validation failed: ");
        status_message.append(sanitize_for_status(ex.what()));
        return status_message.c_str();
    } catch (...) {
        status_message.assign("Validation failed: unknown error.");
        return status_message.c_str();
    }

    status_message = build_validation_status_message(errors);
    if (status_message.empty()) {
        return nullptr;
    }

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

void start_interactive_process() {
    initialize_isocline();
    g_startup_active = false;

    auto startup_end_time = std::chrono::steady_clock::now();
    std::chrono::microseconds startup_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(startup_end_time -
                                                              startup_begin_time());

    if (config::show_title_line) {
        std::cout << " CJ's Shell v" << get_version() << " - Caden J Finley (c) 2025" << '\n';
        std::cout << " Created 2025 @ \033[1;35mAbilene Christian University\033[0m" << '\n';
    }

    if (cjsh_filesystem::is_first_boot()) {
        std::cout << " Be sure to give us a star on GitHub!" << '\n';
        std::cout << " Type 'help' to see available commands and options." << '\n';
        std::cout << " For additional help and documentation, please visit: "
                  << " https://cadenfinley.github.io/CJsShell/" << '\n';
        std::cout << '\n';
        if (!cjsh_filesystem::file_exists(cjsh_filesystem::g_cjsh_source_path())) {
            std::cout << " To create .cjshrc run 'cjshopt generate-rc'" << '\n';
        }
        if (!cjsh_filesystem::file_exists(cjsh_filesystem::g_cjsh_profile_path())) {
            std::cout << " To create .cjprofile run 'cjshopt generate-profile'" << '\n';
        }
        if (!cjsh_filesystem::file_exists(cjsh_filesystem::g_cjsh_logout_path())) {
            std::cout << " To create .cjsh_logout run 'cjshopt generate-logout'" << '\n';
        }
        std::cout << '\n';
        std::cout << " To suppress this help message run the command: 'touch "
                  << cjsh_filesystem::g_cjsh_first_boot_path().string() << "'" << '\n';
        std::cout << " To suppress the title line, put this command in .cjprofile: 'cjshopt "
                     "login-startup-arg --no-titleline'"
                  << '\n';
        std::cout << " Or alternatively execute cjsh with this flag: --no-titleline" << '\n';
        std::cout << '\n';

        std::cout << " cjsh uses a very complex, but very smart completions system.\n";
        std::cout << " During shell use it learns about the commands you use and provides better "
                     "completions as you use cjsh.\n";
        std::cout << " If you would like to skip the learning process and make all completions "
                     "faster please run: 'generate-completions'\n";
        std::cout
            << " Please note: This may take a few minutes depending on how many commands you have "
               "installed, and it can be sped up using the -j flag.\n";
        std::cout << " For example to use 8 parallel jobs run: 'generate-completions -j 8'\n";
        std::cout << "\n";
        config::show_startup_time = true;
    }

    if (config::show_title_line && config::show_startup_time) {
        std::cout << '\n';
    }

    if (config::show_startup_time) {
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
    }

    if (!config::startup_test) {
        main_process_loop();
    }
}

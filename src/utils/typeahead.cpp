#include "utils/typeahead.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <array>
#include <cctype>
#include <cerrno>
#include <deque>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "cjsh.h"
#include "job_control.h"

namespace typeahead {

bool initialized = false;
std::string g_input_buffer;
std::deque<std::string> g_typeahead_queue;
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
                    oss << "\\x" << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
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
            char next = input[i + 1];

            if (next == '[') {
                i += 2;
                while (i < input.size()) {
                    char c = input[i];

                    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '~' || c == 'c' ||
                        c == 'h' || c == 'l' || c == 'm' || c == 'n' || c == 'r' || c == 'J' ||
                        c == 'K' || c == 'H' || c == 'f') {
                        break;
                    }

                    if (!((c >= '0' && c <= '9') || c == ';' || c == '?' || c == '!' || c == '=' ||
                          c == '>' || c == '<')) {
                        break;
                    }
                    i++;
                }
            } else if (next == ']') {
                i += 2;
                while (i < input.size()) {
                    if (input[i] == '\x07') {
                        break;
                    } else if (input[i] == '\x1b' && i + 1 < input.size() && input[i + 1] == '\\') {
                        i++;
                        break;
                    }
                    i++;
                }
            } else if (next == '(' || next == ')') {
                if (i + 2 < input.size()) {
                    i += 2;
                } else {
                    i += 1;
                }
            } else if (next >= '0' && next <= '9') {
                i += 1;
                while (i + 1 < input.size() && input[i + 1] >= '0' && input[i + 1] <= '9') {
                    i++;
                }
            } else {
                i += 1;
            }
        } else if (ch == '\x07') {
            // Skip bell character
        } else if (ch < 0x20 && ch != '\t' && ch != '\n' && ch != '\r') {
            // Skip other control characters
        } else {
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
                while (!normalized.empty() &&
                       (normalized.back() == ' ' || normalized.back() == '\t')) {
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
        g_typeahead_queue.pop_front();
    }

    std::string sanitized_command = filter_escape_sequences(command);

    g_typeahead_queue.push_back(sanitized_command);
}

void ingest_typeahead_input(const std::string& raw_input) {
    if (raw_input.empty()) {
        return;
    }

    std::string combined = g_input_buffer;
    g_input_buffer.clear();
    combined += raw_input;

    if (combined.find('\x1b') != std::string::npos) {
        combined = filter_escape_sequences(combined);
    }

    std::string normalized = normalize_line_edit_sequences(combined);

    std::size_t start = 0;
    while (start < normalized.size()) {
        std::size_t newline_pos = normalized.find('\n', start);
        if (newline_pos == std::string::npos) {
            g_input_buffer += normalized.substr(start);
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
    }
}

void flush_pending_typeahead() {
    std::string pending_input = capture_available_input();
    if (!pending_input.empty()) {
        ingest_typeahead_input(pending_input);
    }
}

bool has_queued_commands() {
    return !g_typeahead_queue.empty();
}

std::string dequeue_command() {
    if (g_typeahead_queue.empty()) {
        return "";
    }

    std::string command = g_typeahead_queue.front();
    g_typeahead_queue.pop_front();
    return command;
}

void clear_input_buffer() {
    g_input_buffer.clear();
}

void clear_command_queue() {
    g_typeahead_queue.clear();
}

std::string get_input_buffer() {
    return g_input_buffer;
}

std::string capture_available_input() {
    if (!initialized) {
        return {};
    }

    if (!isatty(STDIN_FILENO)) {
        return {};
    }

    if (JobManager::instance().foreground_job_reads_stdin()) {
        return {};
    }

    int fd_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (fd_flags == -1) {
        return {};
    }

    bool restore_flags = false;
    if ((fd_flags & O_NONBLOCK) == 0) {
        if (fcntl(STDIN_FILENO, F_SETFL, fd_flags | O_NONBLOCK) == 0) {
            restore_flags = true;
        }
    }

    struct termios original_termios{};
    bool restore_termios = false;
    if (tcgetattr(STDIN_FILENO, &original_termios) == 0) {
        struct termios raw_termios = original_termios;
        raw_termios.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        raw_termios.c_cc[VMIN] = 0;
        raw_termios.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw_termios) == 0) {
            restore_termios = true;
        }
    }

    struct RestoreState {
        bool restore_termios;
        bool restore_flags;
        struct termios original_termios;
        int fd_flags;

        ~RestoreState() {
            if (restore_termios) {
                tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
            }
            if (restore_flags) {
                fcntl(STDIN_FILENO, F_SETFL, fd_flags);
            }
        }
    } restore_state{restore_termios, restore_flags, original_termios, fd_flags};

    int queued_bytes = 0;
    if (ioctl(STDIN_FILENO, FIONREAD, &queued_bytes) == 0) {
    }

    std::string captured_data;
    std::array<char, 256> buffer{};

    for (;;) {
        ssize_t bytes_read = ::read(STDIN_FILENO, buffer.data(), buffer.size());
        if (bytes_read > 0) {
            captured_data.append(buffer.data(), static_cast<size_t>(bytes_read));
            if (bytes_read < static_cast<ssize_t>(buffer.size())) {
                break;
            }
            continue;
        }

        if (bytes_read == 0) {
            break;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        break;
    }

    if (captured_data.empty()) {
        struct pollfd pfd{};
        pfd.fd = STDIN_FILENO;
        pfd.events = POLLIN;
        poll(&pfd, 1, 0);
    }

    for (char& c : captured_data) {
        if (c == '\r') {
            c = '\n';
        }
    }

    return captured_data;
}

void initialize() {
    initialized = true;
    g_input_buffer.clear();
    g_typeahead_queue.clear();
}

void cleanup() {
    initialized = false;
    g_input_buffer.clear();
    g_typeahead_queue.clear();
}

}  // namespace typeahead
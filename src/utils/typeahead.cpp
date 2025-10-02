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
            
            char next = input[i + 1];
            std::size_t seq_start = i;

            if (next == '[') {
                
                i += 2;  
                while (i < input.size()) {
                    char c = input[i];
                    
                    
                    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        c == '~' || c == 'c' || c == 'h' || c == 'l' ||
                        c == 'm' || c == 'n' || c == 'r' || c == 'J' ||
                        c == 'K' || c == 'H' || c == 'f') {
                        break;
                    }
                    
                    if (!((c >= '0' && c <= '9') || c == ';' || c == '?' ||
                          c == '!' || c == '=' || c == '>' || c == '<')) {
                        
                        break;
                    }
                    i++;
                }
                if (g_debug_mode) {
                    std::cerr << "DEBUG: Filtered ANSI CSI escape sequence: "
                              << to_debug_visible(
                                     input.substr(seq_start, i - seq_start + 1))
                              << std::endl;
                }
            } else if (next == ']') {
                
                i += 2;  
                while (i < input.size()) {
                    if (input[i] == '\x07') {
                        break;
                    } else if (input[i] == '\x1b' && i + 1 < input.size() &&
                               input[i + 1] == '\\') {
                        i++;
                        break;
                    }
                    i++;
                }
                if (g_debug_mode) {
                    std::cerr << "DEBUG: Filtered OSC escape sequence: "
                              << to_debug_visible(
                                     input.substr(seq_start, i - seq_start + 1))
                              << std::endl;
                }
            } else if (next == '(' || next == ')') {
                
                if (i + 2 < input.size()) {
                    i += 2;
                } else {
                    i += 1;
                }
                if (g_debug_mode) {
                    std::cerr
                        << "DEBUG: Filtered character set escape sequence: "
                        << to_debug_visible(
                               input.substr(seq_start, i - seq_start + 1))
                        << std::endl;
                }
            } else if (next >= '0' && next <= '9') {
                
                i += 1;
                while (i + 1 < input.size() && input[i + 1] >= '0' &&
                       input[i + 1] <= '9') {
                    i++;
                }
                if (g_debug_mode) {
                    std::cerr << "DEBUG: Filtered numeric escape sequence: "
                              << to_debug_visible(
                                     input.substr(seq_start, i - seq_start + 1))
                              << std::endl;
                }
            } else {
                
                i += 1;
                if (g_debug_mode) {
                    std::cerr << "DEBUG: Filtered single-char escape sequence: "
                              << to_debug_visible(
                                     input.substr(seq_start, i - seq_start + 1))
                              << std::endl;
                }
            }
        } else if (ch == '\x07') {
            
            if (g_debug_mode) {
                std::cerr << "DEBUG: Filtered BEL character" << std::endl;
            }
        } else if (ch < 0x20 && ch != '\t' && ch != '\n' && ch != '\r') {
            
            
            if (g_debug_mode) {
                std::cerr << "DEBUG: Filtered control character: \\x"
                          << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(ch) << std::dec << std::endl;
            }
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
            std::cerr << "DEBUG: Found escape sequences in typeahead input, "
                         "filtering..."
                      << std::endl;
        }
        combined = filter_escape_sequences(combined);
        if (g_debug_mode) {
            std::cerr << "DEBUG: After filtering escape sequences: '"
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
    std::string pending_input = capture_available_input();
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


bool has_queued_commands() {
    return !g_typeahead_queue.empty();
}

std::string dequeue_command() {
    if (g_typeahead_queue.empty()) {
        return "";
    }

    std::string command = g_typeahead_queue.front();
    g_typeahead_queue.pop_front();

    if (g_debug_mode) {
        std::cerr << "DEBUG: Dequeued queued command: '" << command << "'"
                  << std::endl;
    }

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
        if (g_debug_mode) {
            std::cerr << "DEBUG: Typeahead capture skipped (not initialized)"
                      << std::endl;
        }
        return {};
    }

    if (!isatty(STDIN_FILENO)) {
        if (g_debug_mode) {
            std::cerr << "DEBUG: Typeahead capture skipped (stdin not a TTY)"
                      << std::endl;
        }
        return {};
    }

    if (JobManager::instance().foreground_job_reads_stdin()) {
        if (g_debug_mode) {
            std::cerr
                << "DEBUG: Typeahead capture skipped (foreground job reads"
                << " stdin)" << std::endl;
        }
        return {};
    }

    int fd_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (fd_flags == -1) {
        if (g_debug_mode) {
            std::cerr << "DEBUG: Typeahead capture failed to get file flags"
                      << std::endl;
        }
        return {};
    }

    bool restore_flags = false;
    if ((fd_flags & O_NONBLOCK) == 0) {
        if (fcntl(STDIN_FILENO, F_SETFL, fd_flags | O_NONBLOCK) == 0) {
            restore_flags = true;
            if (g_debug_mode) {
                std::cerr
                    << "DEBUG: Typeahead capture temporarily enabled O_NONBLOCK"
                    << std::endl;
            }
        } else if (g_debug_mode) {
            std::cerr << "DEBUG: Typeahead capture failed to enable O_NONBLOCK"
                      << std::endl;
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
            if (g_debug_mode) {
                std::cerr << "DEBUG: Typeahead capture temporarily disabled "
                             "ICANON/ECHO"
                          << std::endl;
            }
        } else if (g_debug_mode) {
            std::cerr << "DEBUG: Typeahead capture failed to apply raw termios"
                      << std::endl;
        }
    } else if (g_debug_mode) {
        std::cerr << "DEBUG: Typeahead capture failed to read termios"
                  << std::endl;
    }

    struct RestoreState {
        bool restore_termios;
        bool restore_flags;
        struct termios original_termios;
        int fd_flags;

        ~RestoreState() {
            if (restore_termios) {
                tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
                if (g_debug_mode) {
                    std::cerr << "DEBUG: Typeahead capture restored ICANON/ECHO"
                              << std::endl;
                }
            }
            if (restore_flags) {
                fcntl(STDIN_FILENO, F_SETFL, fd_flags);
                if (g_debug_mode) {
                    std::cerr
                        << "DEBUG: Typeahead capture restored O_NONBLOCK flag"
                        << std::endl;
                }
            }
        }
    } restore_state{restore_termios, restore_flags, original_termios, fd_flags};

    int queued_bytes = 0;
    if (ioctl(STDIN_FILENO, FIONREAD, &queued_bytes) == 0) {
        if (g_debug_mode) {
            std::cerr << "DEBUG: Typeahead ioctl(FIONREAD) queued_bytes="
                      << queued_bytes << std::endl;
        }
    } else if (g_debug_mode) {
        std::cerr << "DEBUG: Typeahead ioctl(FIONREAD) failed (errno=" << errno
                  << ")" << std::endl;
    }

    std::string captured_data;
    std::array<char, 256> buffer{};

    for (;;) {
        ssize_t bytes_read = ::read(STDIN_FILENO, buffer.data(), buffer.size());
        if (bytes_read > 0) {
            captured_data.append(buffer.data(),
                                 static_cast<size_t>(bytes_read));
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

        if (g_debug_mode) {
            std::cerr << "DEBUG: Typeahead read error (errno=" << errno << ")"
                      << std::endl;
        }
        break;
    }

    if (captured_data.empty()) {
        struct pollfd pfd{};
        pfd.fd = STDIN_FILENO;
        pfd.events = POLLIN;
        int poll_result = poll(&pfd, 1, 0);
        if (g_debug_mode) {
            std::cerr << "DEBUG: Typeahead poll after read returned "
                      << poll_result << " (revents=0x" << std::hex
                      << pfd.revents << std::dec << ")" << std::endl;
        }
    }

    for (char& c : captured_data) {
        if (c == '\r') {
            c = '\n';
        }
    }

    if (g_debug_mode) {
        if (captured_data.empty()) {
            std::cerr << "DEBUG: Typeahead capture read 0 bytes" << std::endl;
        } else {
            std::cerr << "DEBUG: Captured typeahead input (length="
                      << captured_data.length() << ", raw='"
                      << to_debug_visible(captured_data) << "')" << std::endl;
        }
    }

    return captured_data;
}

void initialize() {
    initialized = true;
    g_input_buffer.clear();
    g_typeahead_queue.clear();
    if (g_debug_mode) {
        std::cerr << "DEBUG: Typeahead capture initialized" << std::endl;
    }
}

void cleanup() {
    initialized = false;
    g_input_buffer.clear();
    g_typeahead_queue.clear();
    if (g_debug_mode) {
        std::cerr << "DEBUG: Typeahead capture cleaned up" << std::endl;
    }
}

}  
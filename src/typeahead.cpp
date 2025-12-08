#include "typeahead.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "isocline/isocline.h"
#include "job_control.h"

namespace typeahead {

namespace {

constexpr std::size_t kDefaultInputReserve = 256;
constexpr std::size_t kMaxInputReserve = 16 * 1024;
constexpr std::size_t kReserveSlack = 64;

bool initialized = false;
std::string g_input_buffer;
std::string g_pending_raw_bytes;

}  // namespace

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
                if (std::isprint(ch) != 0) {
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

void filter_escape_sequences_into(std::string_view input, std::string& output) {
    output.clear();
    if (input.empty()) {
        return;
    }

    if (output.capacity() < input.size()) {
        output.reserve(input.size());
    }

    for (std::size_t i = 0; i < input.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(input[i]);

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

                    if ((c < '0' || c > '9') && c != ';' && c != '?' && c != '!' && c != '=' &&
                        c != '>' && c != '<') {
                        break;
                    }
                    i++;
                }
            } else if (next == ']') {
                i += 2;
                while (i < input.size()) {
                    if (input[i] == '\x07') {
                        break;
                    }
                    if (input[i] == '\x1b' && i + 1 < input.size() && input[i + 1] == '\\') {
                        i++;
                        break;
                    }
                    i++;
                }
            } else if (next == '(' || next == ')') {
                i += (i + 2 < input.size()) ? 2 : 1;
            } else if (next >= '0' && next <= '9') {
                i += 1;
                while (i + 1 < input.size() && input[i + 1] >= '0' && input[i + 1] <= '9') {
                    i++;
                }
            } else {
                i += 1;
            }
        } else if (ch == '\x07' || (ch < 0x20 && ch != '\t' && ch != '\n' && ch != '\r')) {
        } else {
            output.push_back(static_cast<char>(ch));
        }
    }
}

std::string filter_escape_sequences(std::string_view input) {
    std::string result;
    filter_escape_sequences_into(input, result);
    return result;
}

void normalize_line_edit_sequences_into(std::string_view input, std::string& output) {
    output.clear();
    if (output.capacity() < input.size()) {
        output.reserve(input.size());
    }

    for (unsigned char ch : input) {
        switch (ch) {
            case '\b':
            case 0x7F: {
                if (!output.empty()) {
                    output.pop_back();
                }
                break;
            }
            case 0x15: {
                while (!output.empty() && output.back() != '\n') {
                    output.pop_back();
                }
                break;
            }
            case 0x17: {
                while (!output.empty() && (output.back() == ' ' || output.back() == '\t')) {
                    output.pop_back();
                }
                while (!output.empty() && output.back() != ' ' && output.back() != '\t' &&
                       output.back() != '\n') {
                    output.pop_back();
                }
                break;
            }
            default:
                output.push_back(static_cast<char>(ch));
                break;
        }
    }
}

std::string normalize_line_edit_sequences(std::string_view input) {
    std::string result;
    normalize_line_edit_sequences_into(input, result);
    return result;
}

void ingest_typeahead_input(const std::string& raw_input) {
    if (raw_input.empty()) {
        return;
    }

    g_pending_raw_bytes.append(raw_input);

    std::string combined;
    combined.reserve(g_input_buffer.size() + raw_input.size());
    combined.append(g_input_buffer);
    combined.append(raw_input);

    thread_local static std::string sanitized_temp;
    sanitized_temp.clear();
    std::size_t desired = std::clamp<std::size_t>(combined.size() + kReserveSlack,
                                                  kDefaultInputReserve, kMaxInputReserve);
    if (sanitized_temp.capacity() < desired) {
        sanitized_temp.reserve(desired);
    }
    filter_escape_sequences_into(combined, sanitized_temp);
    for (char& ch : sanitized_temp) {
        if (ch == '\r') {
            ch = '\n';
        }
    }

    thread_local static std::string normalized_temp;
    normalized_temp.clear();
    std::size_t normalized_desired = std::clamp<std::size_t>(
        sanitized_temp.size() + kReserveSlack, kDefaultInputReserve, kMaxInputReserve);
    if (normalized_temp.capacity() < normalized_desired) {
        normalized_temp.reserve(normalized_desired);
    }
    normalize_line_edit_sequences_into(sanitized_temp, normalized_temp);

    if (g_input_buffer.capacity() < kDefaultInputReserve) {
        g_input_buffer.reserve(kDefaultInputReserve);
    }
    g_input_buffer.clear();

    if (normalized_temp.empty()) {
        return;
    }

    auto assign_view = [&](const char* data, std::size_t length) {
        if (length == 0) {
            g_input_buffer.clear();
            return;
        }
        std::size_t needed =
            std::clamp<std::size_t>(length + kReserveSlack, kDefaultInputReserve, kMaxInputReserve);
        if (g_input_buffer.capacity() < needed) {
            g_input_buffer.reserve(needed);
        }
        g_input_buffer.assign(data, length);
    };

    if (normalized_temp.back() == '\n') {
        std::size_t last_non_newline = normalized_temp.find_last_not_of('\n');
        std::size_t segment_start = 0;
        if (last_non_newline != std::string::npos) {
            std::size_t previous_newline = normalized_temp.find_last_of('\n', last_non_newline);
            segment_start = (previous_newline == std::string::npos) ? 0 : previous_newline + 1;
        }
        assign_view(normalized_temp.data() + segment_start, normalized_temp.size() - segment_start);
    } else {
        std::size_t last_newline = normalized_temp.find_last_of('\n');
        if (last_newline == std::string::npos) {
            assign_view(normalized_temp.data(), normalized_temp.size());
        } else {
            std::size_t leftover_len = normalized_temp.size() - (last_newline + 1);
            assign_view(normalized_temp.data() + last_newline + 1, leftover_len);
        }
    }
}

void flush_pending_typeahead() {
    std::string pending_input = capture_available_input();
    if (!pending_input.empty()) {
        ingest_typeahead_input(pending_input);
    }

    if (!g_pending_raw_bytes.empty()) {
        const auto* data = reinterpret_cast<const uint8_t*>(g_pending_raw_bytes.data());
        if (ic_push_raw_input(data, g_pending_raw_bytes.size())) {
            g_pending_raw_bytes.clear();
            g_input_buffer.clear();
        }
    }
}

void clear_input_buffer() {
    g_input_buffer.clear();
}

const std::string& get_input_buffer() {
    return g_input_buffer;
}

std::string capture_available_input() {
    if (!initialized) {
        return {};
    }

    if (isatty(STDIN_FILENO) == 0) {
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

        RestoreState(bool should_restore_termios, bool should_restore_flags,
                     const struct termios& termios_snapshot, int original_flags)
            : restore_termios(should_restore_termios),
              restore_flags(should_restore_flags),
              original_termios(termios_snapshot),
              fd_flags(original_flags) {
        }

        RestoreState(const RestoreState&) = delete;
        RestoreState& operator=(const RestoreState&) = delete;
        RestoreState(RestoreState&&) = delete;
        RestoreState& operator=(RestoreState&&) = delete;

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

    thread_local static std::size_t capture_reserve = kDefaultInputReserve;
    std::string captured_data;
    std::size_t requested_capacity = capture_reserve;
    if (queued_bytes > 0) {
        requested_capacity =
            std::max<std::size_t>(requested_capacity, static_cast<std::size_t>(queued_bytes));
    }
    if (captured_data.capacity() < requested_capacity) {
        captured_data.reserve(
            std::min<std::size_t>(requested_capacity + kReserveSlack, kMaxInputReserve));
    }
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

    capture_reserve =
        std::clamp<std::size_t>(captured_data.capacity(), kDefaultInputReserve, kMaxInputReserve);

    return captured_data;
}

void initialize() {
    initialized = true;
    if (g_input_buffer.capacity() < kDefaultInputReserve) {
        g_input_buffer.reserve(kDefaultInputReserve);
    }
    g_input_buffer.clear();
    if (g_pending_raw_bytes.capacity() < kDefaultInputReserve) {
        g_pending_raw_bytes.reserve(kDefaultInputReserve);
    }
    g_pending_raw_bytes.clear();
}

void cleanup() {
    initialized = false;
    g_input_buffer.clear();
    g_input_buffer.shrink_to_fit();
    g_pending_raw_bytes.clear();
    g_pending_raw_bytes.shrink_to_fit();
}

}  // namespace typeahead

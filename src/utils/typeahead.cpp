#include "utils/typeahead.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <array>
#include <cctype>
#include <cerrno>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "cjsh.h"
#include "job_control.h"

namespace typeahead {
namespace {
bool initialized = false;

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
}  // namespace

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
    if (g_debug_mode) {
        std::cerr << "DEBUG: Typeahead capture initialized" << std::endl;
    }
}

void cleanup() {
    initialized = false;
    if (g_debug_mode) {
        std::cerr << "DEBUG: Typeahead capture cleaned up" << std::endl;
    }
}

}  // namespace typeahead
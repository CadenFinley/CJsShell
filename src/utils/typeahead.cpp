#include "utils/typeahead.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <array>
#include <string>

#include "cjsh.h"
#include "job_control.h"

namespace typeahead {

namespace {
bool initialized = false;
}

std::string capture_available_input() {
  if (!initialized) {
    return "";
  }

  // Don't capture if stdin is not a terminal
  if (!isatty(STDIN_FILENO)) {
    return "";
  }

  // Don't capture if there's a foreground job reading stdin
  if (JobManager::instance().foreground_job_reads_stdin()) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: Skipping typeahead capture (foreground job reading stdin)" << std::endl;
    }
    return "";
  }

  // Don't capture if terminal is controlled by another process group
  pid_t foreground_pgid = tcgetpgrp(STDIN_FILENO);
  if (foreground_pgid != -1 && foreground_pgid != getpgrp()) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: Skipping typeahead capture (terminal owned by pgid " 
                << foreground_pgid << ")" << std::endl;
    }
    return "";
  }

  // Use poll() to check if input is available (non-blocking)
  struct pollfd pfd{};
  pfd.fd = STDIN_FILENO;
  pfd.events = POLLIN;

  // Poll with 0 timeout (non-blocking check)
  int poll_result = poll(&pfd, 1, 0);
  if (poll_result <= 0) {
    return ""; // No input available or error
  }

  // Check for error conditions
  if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
    return "";
  }

  // No input ready
  if ((pfd.revents & POLLIN) == 0) {
    return "";
  }

  // Input is available, capture it
  // Set stdin to non-blocking mode temporarily
  int fd_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  if (fd_flags == -1) {
    return "";
  }

  bool restore_flags = (fd_flags & O_NONBLOCK) == 0;
  if (restore_flags) {
    if (fcntl(STDIN_FILENO, F_SETFL, fd_flags | O_NONBLOCK) == -1) {
      return "";
    }
  }

  std::string captured_data;
  std::array<char, 256> buffer{};

  // Read all available data without blocking
  for (;;) {
    ssize_t bytes_read = ::read(STDIN_FILENO, buffer.data(), buffer.size());
    if (bytes_read > 0) {
      captured_data.append(buffer.data(), static_cast<size_t>(bytes_read));
      // If we read less than buffer size, we've probably got everything
      if (bytes_read < static_cast<ssize_t>(buffer.size())) {
        break;
      }
    } else if (bytes_read == 0) {
      break; // EOF
    } else {
      if (errno == EINTR) {
        continue; // Interrupted, try again
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break; // No more data available
      }
      break; // Other error
    }
  }

  // Restore original flags
  if (restore_flags) {
    fcntl(STDIN_FILENO, F_SETFL, fd_flags);
  }

  // Normalize line endings (convert \r to \n)
  for (char& c : captured_data) {
    if (c == '\r') {
      c = '\n';
    }
  }

  if (!captured_data.empty() && g_debug_mode) {
    std::cerr << "DEBUG: Captured typeahead input (length=" << captured_data.length() << ")" << std::endl;
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

} // namespace typeahead
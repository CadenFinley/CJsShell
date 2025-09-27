#include "utils/input_monitor.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <array>
#include <cctype>
#include <cstdio>
#include <deque>
#include <iostream>
#include <mutex>
#include <string>

#include "cjsh.h"
#include "job_control.h"

namespace input_monitor {
namespace {
std::mutex monitor_mutex;
std::deque<std::string> queued_commands;
std::string partial_buffer;
bool initialized = false;

std::string normalized_copy(const std::string& raw) {
  std::string normalized;
  normalized.reserve(raw.size());
  for (unsigned char ch : raw) {
    if (ch == '\r') {
      normalized.push_back('\n');
    } else if (ch != '\0') {
      normalized.push_back(static_cast<char>(ch));
    }
  }
  return normalized;
}

std::string read_available_input() {
  if (!isatty(STDIN_FILENO)) {
    return "";
  }

  struct pollfd pfd {};
  pfd.fd = STDIN_FILENO;
  pfd.events = POLLIN;

  int poll_result = poll(&pfd, 1, 0);
  if (poll_result <= 0) {
    return "";
  }

  if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
    return "";
  }

  if ((pfd.revents & POLLIN) == 0) {
    return "";
  }

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

  std::string data;
  std::array<char, 256> buffer{};

  for (;;) {
    ssize_t bytes_read = ::read(STDIN_FILENO, buffer.data(), buffer.size());
    if (bytes_read > 0) {
      data.append(buffer.data(), static_cast<size_t>(bytes_read));
      if (bytes_read < static_cast<ssize_t>(buffer.size())) {
        break;
      }
    } else if (bytes_read == 0) {
      break;
    } else {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      break;
    }
  }

  if (restore_flags) {
    fcntl(STDIN_FILENO, F_SETFL, fd_flags);
  }

  return data;
}

void append_input(const std::string& raw_data) {
  if (raw_data.empty()) {
    return;
  }

  std::string normalized = normalized_copy(raw_data);
  if (normalized.empty()) {
    return;
  }

  std::lock_guard<std::mutex> lock(monitor_mutex);
  partial_buffer.append(normalized);

  size_t newline_pos = 0;
  while ((newline_pos = partial_buffer.find('\n')) != std::string::npos) {
    std::string line = partial_buffer.substr(0, newline_pos);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
      line.pop_back();
    }
    queued_commands.push_back(std::move(line));
    partial_buffer.erase(0, newline_pos + 1);
  }
}

void ensure_initialized() {
  if (initialized) {
    return;
  }
  std::lock_guard<std::mutex> lock(monitor_mutex);
  if (!initialized) {
    queued_commands.clear();
    partial_buffer.clear();
    initialized = true;
  }
}

std::string sanitize_for_debug(const std::string& data) {
  std::string sanitized;
  sanitized.reserve(data.size() * 4);
  for (unsigned char ch : data) {
    if (ch == '\n') {
      sanitized += "\\n";
    } else if (ch == '\r') {
      sanitized += "\\r";
    } else if (std::isprint(ch)) {
      sanitized.push_back(static_cast<char>(ch));
    } else {
      char buf[5];
      std::snprintf(buf, sizeof(buf), "\\x%02X", ch);
      sanitized.append(buf);
    }
  }
  return sanitized;
}

}  // namespace

void initialize() {
  std::lock_guard<std::mutex> lock(monitor_mutex);
  queued_commands.clear();
  partial_buffer.clear();
  initialized = true;
}

void clear() {
  std::lock_guard<std::mutex> lock(monitor_mutex);
  queued_commands.clear();
  partial_buffer.clear();
}

void collect_typeahead() {
  ensure_initialized();

  if (JobManager::instance().foreground_job_reads_stdin()) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: Skipping typeahead capture (foreground job reading stdin)"
                << std::endl;
    }
    return;
  }

  pid_t foreground_pgid = tcgetpgrp(STDIN_FILENO);
  if (foreground_pgid != -1 && foreground_pgid != getpgrp()) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: Skipping typeahead capture (terminal owned by pgid "
                << foreground_pgid << ")" << std::endl;
    }
    return;
  }

  std::string raw_data = read_available_input();
  if (raw_data.empty()) {
    return;
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: InputMonitor captured typeahead ('"
              << sanitize_for_debug(raw_data) << "')" << std::endl;
  }

  append_input(raw_data);
}

bool has_queued_command() {
  std::lock_guard<std::mutex> lock(monitor_mutex);
  return !queued_commands.empty();
}

std::string pop_queued_command() {
  std::lock_guard<std::mutex> lock(monitor_mutex);
  if (queued_commands.empty()) {
    return {};
  }
  std::string command = std::move(queued_commands.front());
  queued_commands.pop_front();
  return command;
}

std::string take_partial_input() {
  std::lock_guard<std::mutex> lock(monitor_mutex);
  std::string partial = std::move(partial_buffer);
  partial_buffer.clear();
  return partial;
}

}  // namespace input_monitor

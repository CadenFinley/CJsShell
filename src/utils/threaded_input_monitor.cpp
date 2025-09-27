#include "utils/threaded_input_monitor.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <iostream>
#include <memory>

#include "cjsh.h"
#include "job_control.h"
#include "shell.h"

namespace threaded_input_monitor {
namespace {
std::unique_ptr<ThreadedInputMonitor> g_monitor_instance;
std::mutex g_instance_mutex;
} // anonymous namespace

ThreadedInputMonitor::ThreadedInputMonitor()
    : running_(false), should_stop_(false), paused_(false) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: ThreadedInputMonitor created" << std::endl;
  }
}

ThreadedInputMonitor::~ThreadedInputMonitor() {
  stop();
  if (g_debug_mode) {
    std::cerr << "DEBUG: ThreadedInputMonitor destroyed" << std::endl;
  }
}

bool ThreadedInputMonitor::start() {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  
  if (running_) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: ThreadedInputMonitor already running" << std::endl;
    }
    return true;
  }
  
  if (!isatty(STDIN_FILENO)) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: ThreadedInputMonitor not starting - stdin is not a terminal" << std::endl;
    }
    return false;
  }
  
  should_stop_ = false;
  running_ = true;
  paused_ = false;
  
  try {
    monitor_thread_ = std::make_unique<std::thread>(&ThreadedInputMonitor::monitor_thread_func, this);
    
    if (g_debug_mode) {
      std::cerr << "DEBUG: ThreadedInputMonitor started successfully" << std::endl;
    }
    return true;
    
  } catch (const std::exception& e) {
    running_ = false;
    if (g_debug_mode) {
      std::cerr << "DEBUG: ThreadedInputMonitor failed to start: " << e.what() << std::endl;
    }
    return false;
  }
}

void ThreadedInputMonitor::stop() {
  if (g_debug_mode) {
    std::cerr << "DEBUG: ThreadedInputMonitor stopping..." << std::endl;
  }
  
  should_stop_ = true;
  
  if (monitor_thread_ && monitor_thread_->joinable()) {
    // Notify the condition variable to wake up any waiting threads
    queue_cv_.notify_all();
    monitor_thread_->join();
  }
  
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    running_ = false;
    // Clear all queues
    while (!input_queue_.empty()) {
      input_queue_.pop();
    }
    while (!command_queue_.empty()) {
      command_queue_.pop();
    }
    partial_input_buffer_.clear();
  }
  
  monitor_thread_.reset();
  
  if (g_debug_mode) {
    std::cerr << "DEBUG: ThreadedInputMonitor stopped" << std::endl;
  }
}

bool ThreadedInputMonitor::is_running() const {
  return running_;
}

size_t ThreadedInputMonitor::queue_size() const {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  return input_queue_.size();
}

size_t ThreadedInputMonitor::command_queue_size() const {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  return command_queue_.size();
}

bool ThreadedInputMonitor::has_queued_commands() const {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  return !command_queue_.empty();
}

std::unique_ptr<ParsedCommand> ThreadedInputMonitor::pop_queued_command(std::chrono::milliseconds timeout) {
  std::unique_lock<std::mutex> lock(queue_mutex_);
  
  if (!queue_cv_.wait_for(lock, timeout, [this] { return !command_queue_.empty() || should_stop_; })) {
    // Timeout occurred
    return nullptr;
  }
  
  if (should_stop_ || command_queue_.empty()) {
    return nullptr;
  }
  
  auto command = std::move(command_queue_.front());
  command_queue_.pop();
  return command;
}

std::string ThreadedInputMonitor::take_partial_input() {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  std::string partial = std::move(partial_input_buffer_);
  partial_input_buffer_.clear();
  return partial;
}

std::unique_ptr<InputEvent> ThreadedInputMonitor::pop_input_event(std::chrono::milliseconds timeout) {
  std::unique_lock<std::mutex> lock(queue_mutex_);
  
  if (!queue_cv_.wait_for(lock, timeout, [this] { return !input_queue_.empty() || should_stop_; })) {
    // Timeout occurred
    return nullptr;
  }
  
  if (should_stop_ || input_queue_.empty()) {
    return nullptr;
  }
  
  auto event = std::move(input_queue_.front());
  input_queue_.pop();
  return event;
}

std::unique_ptr<InputEvent> ThreadedInputMonitor::peek_input_event() const {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  
  if (input_queue_.empty()) {
    return nullptr;
  }
  
  // Create a copy of the front event
  const auto& front_event = input_queue_.front();
  return std::make_unique<InputEvent>(front_event->data);
}

void ThreadedInputMonitor::clear_queue() {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  
  size_t input_cleared = input_queue_.size();
  size_t command_cleared = command_queue_.size();
  
  while (!input_queue_.empty()) {
    input_queue_.pop();
  }
  
  while (!command_queue_.empty()) {
    command_queue_.pop();
  }
  
  partial_input_buffer_.clear();
  
  if (g_debug_mode && (input_cleared > 0 || command_cleared > 0)) {
    std::cerr << "DEBUG: ThreadedInputMonitor cleared " << input_cleared 
              << " input events and " << command_cleared << " commands" << std::endl;
  }
}

void ThreadedInputMonitor::pause_monitoring() {
  paused_ = true;
  if (g_debug_mode) {
    std::cerr << "DEBUG: ThreadedInputMonitor paused" << std::endl;
  }
}

void ThreadedInputMonitor::resume_monitoring() {
  paused_ = false;
  if (g_debug_mode) {
    std::cerr << "DEBUG: ThreadedInputMonitor resumed" << std::endl;
  }
}

bool ThreadedInputMonitor::is_paused() const {
  return paused_;
}

void ThreadedInputMonitor::monitor_thread_func() {
  if (g_debug_mode) {
    std::cerr << "DEBUG: ThreadedInputMonitor background thread started" << std::endl;
  }
  
  while (!should_stop_) {
    try {
      // Sleep for the monitoring interval
      std::this_thread::sleep_for(MONITOR_INTERVAL);
      
      if (should_stop_) break;
      
      // Skip monitoring if paused or if we shouldn't monitor
      if (paused_ || !should_monitor_input()) {
        continue;
      }
      
      // Try to read available input
      std::string input_data = read_available_input_data();
      
      if (!input_data.empty()) {
        process_input_data(input_data);
        
        if (g_debug_mode) {
          std::cerr << "DEBUG: ThreadedInputMonitor captured input (length=" << input_data.length() << ")" << std::endl;
        }
      }
      
    } catch (const std::exception& e) {
      if (g_debug_mode) {
        std::cerr << "DEBUG: ThreadedInputMonitor exception in background thread: " << e.what() << std::endl;
      }
      // Continue running even if we hit an exception
    }
  }
  
  if (g_debug_mode) {
    std::cerr << "DEBUG: ThreadedInputMonitor background thread exiting" << std::endl;
  }
}

bool ThreadedInputMonitor::should_monitor_input() const {
  // Don't monitor if there's a foreground job reading stdin
  if (JobManager::instance().foreground_job_reads_stdin()) {
    return false;
  }
  
  // Check if terminal is owned by us
  pid_t foreground_pgid = tcgetpgrp(STDIN_FILENO);
  if (foreground_pgid != -1 && foreground_pgid != getpgrp()) {
    return false;
  }
  
  return true;
}

std::string ThreadedInputMonitor::read_available_input_data() {
  if (!isatty(STDIN_FILENO)) {
    return "";
  }

  struct pollfd pfd{};
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

  // Set non-blocking mode temporarily
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

  // Read available data
  for (;;) {
    ssize_t bytes_read = ::read(STDIN_FILENO, buffer.data(), buffer.size());
    if (bytes_read > 0) {
      data.append(buffer.data(), static_cast<size_t>(bytes_read));
      if (bytes_read < static_cast<ssize_t>(buffer.size())) {
        break;  // Read all available data
      }
    } else if (bytes_read == 0) {
      break;  // EOF
    } else {
      if (errno == EINTR) {
        continue;  // Interrupted, try again
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;  // No more data available
      }
      break;  // Other error
    }
  }

  // Restore original flags
  if (restore_flags) {
    fcntl(STDIN_FILENO, F_SETFL, fd_flags);
  }

  return data;
}

void ThreadedInputMonitor::process_input_data(const std::string& data) {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  
  // Store raw input event if needed
  if (input_queue_.size() < MAX_QUEUE_SIZE) {
    input_queue_.push(std::make_unique<InputEvent>(data));
  } else if (g_debug_mode) {
    std::cerr << "DEBUG: ThreadedInputMonitor input queue full, dropping raw input" << std::endl;
  }
  
  // Add data to partial buffer and process into commands
  partial_input_buffer_ += data;
  parse_commands_from_buffer();
  
  // Notify waiting threads
  queue_cv_.notify_all();
}

void ThreadedInputMonitor::parse_commands_from_buffer() {
  // Process carriage returns and normalize line endings
  for (char& c : partial_input_buffer_) {
    if (c == '\r') {
      c = '\n';
    }
  }
  
  // Extract complete commands (lines ending with newline)
  size_t pos = 0;
  while ((pos = partial_input_buffer_.find('\n')) != std::string::npos) {
    std::string line = partial_input_buffer_.substr(0, pos);
    partial_input_buffer_.erase(0, pos + 1);
    
    // Remove any trailing carriage returns
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
      line.pop_back();
    }
    
    // Add command to queue if not empty
    if (!line.empty() || true) { // Keep empty lines for now
      if (command_queue_.size() < MAX_QUEUE_SIZE) {
        command_queue_.push(std::make_unique<ParsedCommand>(line, true));
      } else if (g_debug_mode) {
        std::cerr << "DEBUG: ThreadedInputMonitor command queue full, dropping command: " << line << std::endl;
      }
    }
  }
}

// Global instance management
ThreadedInputMonitor& get_instance() {
  std::lock_guard<std::mutex> lock(g_instance_mutex);
  if (!g_monitor_instance) {
    g_monitor_instance = std::make_unique<ThreadedInputMonitor>();
  }
  return *g_monitor_instance;
}

void initialize() {
  std::lock_guard<std::mutex> lock(g_instance_mutex);
  if (!g_monitor_instance) {
    g_monitor_instance = std::make_unique<ThreadedInputMonitor>();
    if (g_debug_mode) {
      std::cerr << "DEBUG: ThreadedInputMonitor global instance initialized" << std::endl;
    }
  }
}

void shutdown() {
  std::lock_guard<std::mutex> lock(g_instance_mutex);
  if (g_monitor_instance) {
    g_monitor_instance.reset();
    if (g_debug_mode) {
      std::cerr << "DEBUG: ThreadedInputMonitor global instance shut down" << std::endl;
    }
  }
}

// Convenience functions
bool start_monitoring() {
  return get_instance().start();
}

void stop_monitoring() {
  get_instance().stop();
}

bool is_monitoring_active() {
  return get_instance().is_running();
}

// Command processing functions
bool has_queued_commands() {
  return get_instance().has_queued_commands();
}

std::unique_ptr<ParsedCommand> get_next_command(std::chrono::milliseconds timeout) {
  return get_instance().pop_queued_command(timeout);
}

std::string get_partial_input() {
  return get_instance().take_partial_input();
}

// Raw input functions
std::unique_ptr<InputEvent> get_next_input(std::chrono::milliseconds timeout) {
  return get_instance().pop_input_event(timeout);
}

void clear_input_queue() {
  get_instance().clear_queue();
}

void pause_input_monitoring() {
  get_instance().pause_monitoring();
}

void resume_input_monitoring() {
  get_instance().resume_monitoring();
}

} // namespace threaded_input_monitor
#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <string>
#include <queue>

namespace threaded_input_monitor {

// Represents a captured input event with timing information
struct InputEvent {
  std::string data;
  std::chrono::steady_clock::time_point timestamp;
  
  InputEvent(const std::string& input_data) 
    : data(input_data), timestamp(std::chrono::steady_clock::now()) {}
};

// Represents a parsed command ready for execution
struct ParsedCommand {
  std::string command;
  std::chrono::steady_clock::time_point timestamp;
  bool is_complete;
  
  ParsedCommand(const std::string& cmd, bool complete = true)
    : command(cmd), timestamp(std::chrono::steady_clock::now()), is_complete(complete) {}
};

// Thread-safe input monitor that runs in background to collect typeahead
class ThreadedInputMonitor {
public:
  ThreadedInputMonitor();
  ~ThreadedInputMonitor();
  
  // Non-copyable, non-movable
  ThreadedInputMonitor(const ThreadedInputMonitor&) = delete;
  ThreadedInputMonitor& operator=(const ThreadedInputMonitor&) = delete;
  
  // Start background input monitoring thread
  bool start();
  
  // Stop background input monitoring thread
  void stop();
  
  // Check if monitoring is active
  bool is_running() const;
  
  // Get number of queued input events
  size_t queue_size() const;
  
  // Get number of queued parsed commands
  size_t command_queue_size() const;
  
  // Check if there are any complete commands ready
  bool has_queued_commands() const;
  
  // Pop the next complete command (blocks if none available)
  std::unique_ptr<ParsedCommand> pop_queued_command(std::chrono::milliseconds timeout = std::chrono::milliseconds(100));
  
  // Get any partial input that hasn't formed a complete command yet
  std::string take_partial_input();
  
  // Pop the next input event (blocks if none available)
  std::unique_ptr<InputEvent> pop_input_event(std::chrono::milliseconds timeout = std::chrono::milliseconds(100));
  
  // Peek at next input event without removing it
  std::unique_ptr<InputEvent> peek_input_event() const;
  
  // Clear all queued input events
  void clear_queue();
  
  // Pause/resume monitoring (useful when foreground job needs stdin)
  void pause_monitoring();
  void resume_monitoring();
  bool is_paused() const;

private:
  // Background thread function
  void monitor_thread_func();
  
  // Check if we should monitor input based on job state
  bool should_monitor_input() const;
  
  // Read available input data
  std::string read_available_input_data();
  
  // Process raw input data into commands
  void process_input_data(const std::string& data);
  
  // Parse input buffer into commands and update queues
  void parse_commands_from_buffer();
  
  mutable std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::queue<std::unique_ptr<InputEvent>> input_queue_;
  std::queue<std::unique_ptr<ParsedCommand>> command_queue_;
  std::string partial_input_buffer_;
  
  std::atomic<bool> running_;
  std::atomic<bool> should_stop_;
  std::atomic<bool> paused_;
  std::unique_ptr<std::thread> monitor_thread_;
  
  // Configuration
  static constexpr size_t MAX_QUEUE_SIZE = 1000;
  static constexpr std::chrono::milliseconds MONITOR_INTERVAL{10}; // 10ms polling
};

// Global instance management
ThreadedInputMonitor& get_instance();
void initialize();
void shutdown();

// Convenience functions that operate on the global instance
bool start_monitoring();
void stop_monitoring();
bool is_monitoring_active();

// Command processing functions
bool has_queued_commands();
std::unique_ptr<ParsedCommand> get_next_command(std::chrono::milliseconds timeout = std::chrono::milliseconds(100));
std::string get_partial_input();

// Raw input functions
std::unique_ptr<InputEvent> get_next_input(std::chrono::milliseconds timeout = std::chrono::milliseconds(100));
void clear_input_queue();
void pause_input_monitoring();
void resume_input_monitoring();

} // namespace threaded_input_monitor
#pragma once

#include <chrono>
#include <string>

class CommandInfo {
 private:
  int min_time_threshold = 0;  // milliseconds
  bool show_milliseconds = false;

  std::chrono::time_point<std::chrono::high_resolution_clock>
      last_command_start;
  std::chrono::time_point<std::chrono::high_resolution_clock> last_command_end;
  bool timing_active = false;

  std::string format_duration(long long milliseconds);
  std::string format_exit_code(int exit_code);

 public:
  CommandInfo();

  void start_command_timing();
  void end_command_timing(int exit_code);
  void reset_command_timing();
  long long get_last_command_duration_ms();
  std::string get_formatted_duration();
  bool should_show_duration();

  int get_last_exit_code();
  std::string get_exit_status_symbol();
  bool is_last_command_success();

  void set_min_time_threshold(int milliseconds);
  void set_show_milliseconds(bool show);
};
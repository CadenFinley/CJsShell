#include "command_info.h"

#include <iomanip>
#include <sstream>

CommandInfo::CommandInfo() : timing_active(false) {
}

void CommandInfo::start_command_timing() {
  last_command_start = std::chrono::high_resolution_clock::now();
  timing_active = true;
}

void CommandInfo::end_command_timing(int exit_code) {
  if (timing_active) {
    last_command_end = std::chrono::high_resolution_clock::now();
    last_exit_code = exit_code;
    timing_active = false;
  }
}

long long CommandInfo::get_last_command_duration_ms() {
  if (timing_active) {
    // Command is still running, return current duration
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_command_start).count();
  } else {
    // Command finished
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        last_command_end - last_command_start).count();
  }
}

std::string CommandInfo::get_formatted_duration() {
  long long ms = get_last_command_duration_ms();
  return format_duration(ms);
}

bool CommandInfo::should_show_duration() {
  return get_last_command_duration_ms() >= min_time_threshold;
}

int CommandInfo::get_last_exit_code() {
  return last_exit_code;
}

std::string CommandInfo::get_exit_status_symbol() {
  if (last_exit_code == 0) {
    return "✓";  // Success
  } else {
    return "✗";  // Failure
  }
}

bool CommandInfo::is_last_command_success() {
  return last_exit_code == 0;
}

void CommandInfo::set_min_time_threshold(int milliseconds) {
  min_time_threshold = milliseconds;
}

void CommandInfo::set_show_milliseconds(bool show) {
  show_milliseconds = show;
}

std::string CommandInfo::format_duration(long long milliseconds) {
  std::ostringstream oss;
  
  if (milliseconds < 1000) {
    // Less than 1 second
    if (show_milliseconds) {
      oss << milliseconds << "ms";
    } else {
      oss << "0s";
    }
  } else if (milliseconds < 60000) {
    // Less than 1 minute
    double seconds = milliseconds / 1000.0;
    if (show_milliseconds) {
      oss << std::fixed << std::setprecision(3) << seconds << "s";
    } else {
      oss << std::fixed << std::setprecision(1) << seconds << "s";
    }
  } else if (milliseconds < 3600000) {
    // Less than 1 hour
    int minutes = milliseconds / 60000;
    int seconds = (milliseconds % 60000) / 1000;
    oss << minutes << "m " << seconds << "s";
  } else {
    // 1 hour or more
    int hours = milliseconds / 3600000;
    int minutes = (milliseconds % 3600000) / 60000;
    int seconds = (milliseconds % 60000) / 1000;
    oss << hours << "h " << minutes << "m " << seconds << "s";
  }
  
  return oss.str();
}

std::string CommandInfo::format_exit_code(int exit_code) {
  if (exit_code == 0) {
    return "";  // Don't show anything for success
  } else {
    return "[" + std::to_string(exit_code) + "]";
  }
}
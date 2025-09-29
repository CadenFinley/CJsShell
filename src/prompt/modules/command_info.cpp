#include "command_info.h"

#include <cstdlib>
#include <iomanip>
#include <sstream>

CommandInfo::CommandInfo() : timing_active(false) {
}

void CommandInfo::start_command_timing() {
    last_command_start = std::chrono::high_resolution_clock::now();
    timing_active = true;
}

void CommandInfo::end_command_timing(int exit_code) {
    (void)exit_code;  // Unused - exit code now managed via STATUS environment
                      // variable
    if (timing_active) {
        last_command_end = std::chrono::high_resolution_clock::now();
        timing_active = false;
    }
}

void CommandInfo::reset_command_timing() {
    timing_active = false;
    // Reset to epoch time to ensure 0 duration
    last_command_start = std::chrono::high_resolution_clock::time_point{};
    last_command_end = std::chrono::high_resolution_clock::time_point{};
}

long long CommandInfo::get_last_command_duration_us() {
    if (timing_active) {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   now - last_command_start)
            .count();
    } else {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   last_command_end - last_command_start)
            .count();
    }
}

std::string CommandInfo::get_formatted_duration() {
    long long us = get_last_command_duration_us();
    return format_duration(us);
}

bool CommandInfo::should_show_duration() {
    return get_last_command_duration_us() >= min_time_threshold;
}

int CommandInfo::get_last_exit_code() {
    const char* status_env = getenv("?");
    return status_env ? std::atoi(status_env) : 0;
}

std::string CommandInfo::get_exit_status_symbol() {
    int exit_code = get_last_exit_code();
    if (exit_code == 0) {
        return "✓";
    } else {
        return "✗";
    }
}

bool CommandInfo::is_last_command_success() {
    return get_last_exit_code() == 0;
}

void CommandInfo::set_min_time_threshold(int microseconds) {
    min_time_threshold = microseconds;
}

void CommandInfo::set_show_microseconds(bool show) {
    show_microseconds = show;
}

void CommandInfo::set_initial_duration(long long microseconds) {
    // Set the timing to simulate a completed command with the given duration
    timing_active = false;
    auto now = std::chrono::high_resolution_clock::now();
    last_command_end = now;
    last_command_start = now - std::chrono::microseconds(microseconds);
}

std::string CommandInfo::format_duration(long long microseconds) {
    std::ostringstream oss;

    // For times less than 1 millisecond, show microseconds
    if (microseconds < 1000) {
        oss << microseconds << "μs";
    }
    // For times less than 1 second, show milliseconds
    else if (microseconds < 1000000) {
        double milliseconds_val = microseconds / 1000.0;
        oss << std::fixed << std::setprecision(2) << milliseconds_val << "ms";
    }
    // For times less than 10 seconds, show seconds with decimal precision
    else if (microseconds < 10000000) {
        double seconds = microseconds / 1000000.0;
        oss << std::fixed << std::setprecision(3) << seconds << "s";
    }
    // For times less than 1 minute, show seconds with 1 decimal place
    else if (microseconds < 60000000) {
        double seconds = microseconds / 1000000.0;
        oss << std::fixed << std::setprecision(1) << seconds << "s";
    }
    // For times less than 1 hour, show minutes and seconds
    else if (microseconds < 3600000000LL) {
        int minutes = microseconds / 60000000;
        int seconds = (microseconds % 60000000) / 1000000;
        oss << minutes << "m " << seconds << "s";
    }
    // For times 1 hour or longer, show hours, minutes, and seconds
    else {
        int hours = microseconds / 3600000000LL;
        int minutes = (microseconds % 3600000000LL) / 60000000;
        int seconds = (microseconds % 60000000) / 1000000;
        oss << hours << "h " << minutes << "m " << seconds << "s";
    }

    return oss.str();
}

std::string CommandInfo::format_exit_code(int exit_code) {
    if (exit_code == 0) {
        return "";
    } else {
        return "[" + std::to_string(exit_code) + "]";
    }
}
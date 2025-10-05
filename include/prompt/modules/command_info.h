#pragma once

#include <chrono>
#include <string>


extern int min_time_threshold;
extern bool show_microseconds;
extern std::chrono::time_point<std::chrono::high_resolution_clock> last_command_start;
extern std::chrono::time_point<std::chrono::high_resolution_clock> last_command_end;
extern bool timing_active;
extern int last_exit_code;


std::string format_duration(long long microseconds);
std::string format_exit_code(int exit_code);


void start_command_timing();
void end_command_timing(int exit_code);
void reset_command_timing();
long long get_last_command_duration_us();
std::string get_formatted_duration();
bool should_show_duration();

int get_last_exit_code();
std::string get_exit_status_symbol();
bool is_last_command_success();

void set_min_time_threshold(int microseconds);
void set_show_microseconds(bool show);
void set_initial_duration(long long microseconds);

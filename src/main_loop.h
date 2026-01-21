#pragma once

#include <chrono>

void initialize_isocline();
void main_process_loop();
void start_interactive_process();
std::chrono::steady_clock::time_point& startup_begin_time();

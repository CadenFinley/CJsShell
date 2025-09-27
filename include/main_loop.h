#pragma once

#include <string>

void main_process_loop();
void update_terminal_title();
void reprint_prompt();
void notify_plugins(const std::string& trigger, const std::string& data);
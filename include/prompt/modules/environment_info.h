#pragma once

#include <string>
#include <utility>


std::string get_terminal_type();
std::pair<int, int> get_terminal_dimensions();
std::string get_active_language_version(const std::string& language);
bool is_in_virtual_environment(std::string& env_name);
int get_background_jobs_count();
std::string get_shell();
std::string get_shell_version();

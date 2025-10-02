#pragma once

#include <string>
#include <vector>

#include <sys/types.h>

int umask_command(const std::vector<std::string>& args);

mode_t parse_octal_mode(const std::string& mode_str);
std::string format_octal_mode(mode_t mode);
mode_t parse_symbolic_mode(const std::string& mode_str, mode_t current_mask);

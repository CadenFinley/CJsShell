#pragma once

#include <string>
#include <vector>

// Function prototype for theme command
int theme_command(const std::vector<std::string>& args);
int update_theme_in_rc_file(const std::string& themeName);

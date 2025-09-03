#pragma once

#include <string>
#include <vector>

int theme_command(const std::vector<std::string>& args);
int update_theme_in_rc_file(const std::string& themeName);
int preview_theme(const std::string& theme_name);

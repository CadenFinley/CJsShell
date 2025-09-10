#pragma once

#include <string>
#include <vector>

int theme_command(const std::vector<std::string>& args);
int preview_theme(const std::string& theme_name);
int uninstall_theme(const std::string& themeName);
int list_available_themes();

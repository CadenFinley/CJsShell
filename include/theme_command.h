#pragma once

#include <string>
#include <vector>

int theme_command(const std::vector<std::string>& args);
int update_theme_in_rc_file(const std::string& themeName);
int preview_theme(const std::string& theme_name);
int preview_remote_theme(const std::string& themeName);
int install_theme(const std::string& themeName);
int uninstall_theme(const std::string& themeName);
int list_available_themes();

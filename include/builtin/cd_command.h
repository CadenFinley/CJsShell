#pragma once

#include <string>

int change_directory(const std::string& dir, std::string& current_directory,
                     std::string& previous_directory, std::string& last_terminal_output_error);

int change_directory_smart(const std::string& dir, std::string& current_directory,
                           std::string& previous_directory,
                           std::string& last_terminal_output_error);

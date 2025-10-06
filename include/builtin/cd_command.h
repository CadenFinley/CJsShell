#pragma once

#include <string>

class Shell;

int change_directory(const std::string& dir, std::string& current_directory,
                     std::string& previous_directory, std::string& last_terminal_output_error,
                     Shell* shell = nullptr);

int change_directory_smart(const std::string& dir, std::string& current_directory,
                           std::string& previous_directory, std::string& last_terminal_output_error,
                           Shell* shell = nullptr);

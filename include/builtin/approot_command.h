#pragma once

#include <string>

int change_to_approot(std::string& current_directory, std::string& previous_directory,
                      std::string& last_terminal_output_error);

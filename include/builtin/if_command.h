#pragma once

#include <string>
#include <vector>

class Shell;

int if_command(const std::vector<std::string>& args, Shell* shell,
               std::string& last_terminal_output_error);

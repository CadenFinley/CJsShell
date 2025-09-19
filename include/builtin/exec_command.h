#pragma once

#include <string>
#include <vector>

#include "shell.h"

int exec_command(const std::vector<std::string>& args, Shell* shell,
                 std::string& last_terminal_output_error);

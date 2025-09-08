#pragma once

#include <string>
#include <vector>

class Shell;

// POSIX getopts builtin command
int getopts_command(const std::vector<std::string>& args, Shell* shell);

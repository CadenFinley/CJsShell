#pragma once

#include <string>
#include <vector>

class Shell;

// POSIX type builtin command
int type_command(const std::vector<std::string>& args, Shell* shell);

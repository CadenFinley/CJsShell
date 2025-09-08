#pragma once

#include <string>
#include <vector>

class Shell;

// POSIX times builtin command
int times_command(const std::vector<std::string>& args, Shell* shell = nullptr);

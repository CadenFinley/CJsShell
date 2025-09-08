#pragma once

#include <string>
#include <vector>

class Shell;

// POSIX hash builtin command
int hash_command(const std::vector<std::string>& args, Shell* shell = nullptr);

#pragma once

#include <string>
#include <vector>

class Shell;

// Executes an internal subshell. Args format:
// ["__INTERNAL_SUBSHELL__", "<command string>"]
// Returns the exit status of the subshell execution or 1 on usage error.
int internal_subshell_command(const std::vector<std::string>& args, Shell* shell);

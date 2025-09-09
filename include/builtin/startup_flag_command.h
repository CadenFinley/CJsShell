#pragma once

#include <string>
#include <vector>

// Builtin command for setting startup flags from within shell scripts
// Usage: startup-flag [--flag-name]
// Example: startup-flag --no-plugins
int startup_flag_command(const std::vector<std::string>& args);

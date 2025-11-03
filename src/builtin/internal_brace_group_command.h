#pragma once

#include <string>
#include <vector>

class Shell;

int internal_brace_group_command(const std::vector<std::string>& args, Shell* shell);

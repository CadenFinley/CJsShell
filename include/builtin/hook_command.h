#pragma once

#include <string>
#include <vector>

class Shell;

int hook_command(const std::vector<std::string>& args, Shell* shell);

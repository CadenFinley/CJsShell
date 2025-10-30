#pragma once

#include <string>
#include <vector>

class Shell;

int generate_completions_command(const std::vector<std::string>& args, Shell* shell);

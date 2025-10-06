#pragma once

#include <string>
#include <vector>

class Shell;

int fc_command(const std::vector<std::string>& args, Shell* shell);

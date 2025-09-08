#pragma once

#include <string>
#include <vector>

#include "shell.h"

int set_command(const std::vector<std::string>& args, Shell* shell);
int shift_command(const std::vector<std::string>& args, Shell* shell);

#pragma once

#include <string>
#include <vector>

#include "shell.h"

int eval_command(const std::vector<std::string>& args, Shell* shell);

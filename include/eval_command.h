#pragma once

#include <vector>
#include <string>
#include "shell.h"

int eval_command(const std::vector<std::string>& args, Shell* shell);

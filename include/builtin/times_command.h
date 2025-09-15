#pragma once

#include <string>
#include <vector>

class Shell;

int times_command(const std::vector<std::string>& args, Shell* shell = nullptr);

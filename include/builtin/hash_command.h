#pragma once

#include <string>
#include <vector>

class Shell;

int hash_command(const std::vector<std::string>& args, Shell* shell = nullptr);

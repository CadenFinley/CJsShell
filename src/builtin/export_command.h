#pragma once

#include <string>
#include <vector>

class Shell;

int export_command(const std::vector<std::string>& args, Shell* shell);
int unset_command(const std::vector<std::string>& args, Shell* shell);

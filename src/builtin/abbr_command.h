#pragma once

#include <string>
#include <vector>

class Shell;

int abbr_command(const std::vector<std::string>& args, Shell* shell);
int unabbr_command(const std::vector<std::string>& args, Shell* shell);

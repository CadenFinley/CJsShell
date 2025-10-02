#pragma once

#include <string>
#include <unordered_map>
#include <vector>

class Shell;

int alias_command(const std::vector<std::string>& args, Shell* shell);
int unalias_command(const std::vector<std::string>& args, Shell* shell);
bool parse_assignment(const std::string& arg, std::string& name,
                      std::string& value);

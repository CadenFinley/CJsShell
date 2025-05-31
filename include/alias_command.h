#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "shell.h"

int alias_command(const std::vector<std::string>& args, Shell* shell);
int unalias_command(const std::vector<std::string>& args, Shell* shell);
int save_alias_to_file(const std::string& name, const std::string& value);
int remove_alias_from_file(const std::string& name);
bool parse_assignment(const std::string& arg, std::string& name,
                      std::string& value);

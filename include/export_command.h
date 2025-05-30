#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "shell.h"

int export_command(const std::vector<std::string>& args, Shell* shell);
int unset_command(const std::vector<std::string>& args, Shell* shell);
int save_env_var_to_file(const std::string& name, const std::string& value,
                         bool login_mode);
int remove_env_var_from_file(const std::string& name, bool login_mode);
bool parse_env_assignment(const std::string& arg, std::string& name,
                          std::string& value);

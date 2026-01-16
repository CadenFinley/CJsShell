#pragma once

#include <string>
#include <vector>

class Shell;

void readonly_manager_set(const std::string& name);

bool readonly_manager_is(const std::string& name);

std::vector<std::string> readonly_manager_list();

int readonly_command(const std::vector<std::string>& args, Shell* shell);

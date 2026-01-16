#pragma once

#include <string>
#include <vector>

class Shell;

void trap_manager_set_trap(int signal, const std::string& command);

void trap_manager_remove_trap(int signal);

void trap_manager_execute_trap(int signal);

std::vector<std::pair<int, std::string>> trap_manager_list_traps();

bool trap_manager_has_trap(int signal);

void trap_manager_set_shell(Shell* shell);

void trap_manager_execute_exit_trap();
void trap_manager_execute_debug_trap();

int signal_name_to_number(const std::string& signal_name);

std::string signal_number_to_name(int signal_number);

int trap_command(const std::vector<std::string>& args);

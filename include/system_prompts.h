#pragma once

#include <string>

// Returns the system prompt used for AI command
std::string build_system_prompt();

// Returns the system prompt used for AI help command
std::string create_help_system_prompt();

// Returns the common system prompt base for all AI functionality
std::string get_common_system_prompt();

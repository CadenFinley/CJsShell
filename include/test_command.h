#ifndef TEST_COMMAND_H
#define TEST_COMMAND_H

#include <string>
#include <vector>

// Built-in test command for shell conditionals
// Implements the POSIX test command functionality
int test_command(const std::vector<std::string>& args);

#endif // TEST_COMMAND_H

#ifndef DOUBLE_BRACKET_TEST_COMMAND_H
#define DOUBLE_BRACKET_TEST_COMMAND_H

#include <string>
#include <vector>

/**
 * Double bracket test command implementation [[ ]]
 * This is a bash-compatible extension that supports:
 * - Pattern matching with == and !=
 * - Regular expression matching with =~
 * - Logical operators && and ||
 * - All standard test operations from single bracket test
 */
int double_bracket_test_command(const std::vector<std::string>& args);

#endif  // DOUBLE_BRACKET_TEST_COMMAND_H
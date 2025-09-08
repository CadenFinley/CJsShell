#ifndef READ_COMMAND_H
#define READ_COMMAND_H

#include <string>
#include <vector>

class Shell;

int read_command(const std::vector<std::string>& args, Shell* shell);

#endif  // READ_COMMAND_H

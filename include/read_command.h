#ifndef READ_COMMAND_H
#define READ_COMMAND_H

#include <vector>
#include <string>

class Shell;

int read_command(const std::vector<std::string>& args, Shell* shell);

#endif // READ_COMMAND_H

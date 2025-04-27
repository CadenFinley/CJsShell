#pragma once
#include "parser.h"
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <fcntl.h>

class Exec {
private:

public:
  Exec();
  ~Exec();

  void execute_command_sync(const std::vector<std::string>& args);
  void execute_command_async(const std::vector<std::string>& args);

  std::string last_terminal_output_error;
};
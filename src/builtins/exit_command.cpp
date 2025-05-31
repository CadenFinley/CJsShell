#include "exit_command.h"

#include "main.h"

int exit_command(const std::vector<std::string>& args) {
  (void)args;
  g_exit_flag = true;
  return 0;
}

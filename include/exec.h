#pragma once
#include "parser.h"
#include "built_ins.h"
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <fcntl.h>

class Exec {
private:
  Parser parser;
  Built_ins built_ins;
  std::string result;

public:
  Exec();
  ~Exec();

  void execute_command_sync(const std::string& command);
  void execute_command_async(const std::string& command);
};
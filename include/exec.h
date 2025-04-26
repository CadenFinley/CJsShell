#pragma once
#include <string>
#include <iostream>

class Exec {

public:

  Exec();
  ~Exec();

  void execute_command_sync(const std::string& command);
  void execute_command_async(const std::string& command);
};
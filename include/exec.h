#pragma once
#include "parser.h"
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <fcntl.h>
#include <mutex>

class Exec {
private:
  std::mutex error_mutex;

public:
  Exec();
  ~Exec();

  void execute_command_sync(const std::vector<std::string>& args);
  void execute_command_async(const std::vector<std::string>& args);
  
  // Thread-safe methods for error handling
  void set_error(const std::string& error);
  std::string get_error();

  std::string last_terminal_output_error;
};
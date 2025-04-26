#pragma once
#include "parser.h"
#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include "hash.h"

// Forward declaration
class Shell;

class Exec {
private:
  Parser parser;
  std::string result;
  std::string current_directory;
  Shell* shell; // Reference to the shell instance

public:
  Exec(Shell* shell_instance);
  ~Exec();

  void execute_command_sync(const std::string& command);
  void execute_command_async(const std::string& command);
  bool builtin_command(const std::vector<std::string>& args);

  bool change_directory(const std::string& dir, std::string& result);
};
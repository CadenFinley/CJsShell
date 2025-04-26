#pragma once

#include <iostream>
#include "hash.h"
#include <filesystem>
#include <unistd.h>
// Removed unnecessary include for "main.h"


class Built_ins {

public:
  Built_ins() = default;
  ~Built_ins() = default;


  bool builtin_command(const std::vector<std::string>& args);
  bool change_directory(const std::string& dir, std::string& result);

  std::string get_current_directory() const {
    return current_directory;
  }

  void set_current_directory() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
      current_directory = cwd;
    } else {
      current_directory = "/";
    }
  }

private:
  std::string current_directory;

};
#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>

// in the future this will handle the parsing of the input, splitting args, applying envvars and aliases

class Parser {
public:
  std::vector<std::string> parse_command(const std::string& command);

  void set_aliases(const std::unordered_map<std::string, std::string>& aliases) {
    this->aliases = aliases;
  }

  void set_env_vars(const std::unordered_map<std::string, std::string>& env_vars) {
    this->env_vars = env_vars;
  }

private:
  std::unordered_map<std::string, std::string> aliases;
  std::unordered_map<std::string, std::string> env_vars;
};
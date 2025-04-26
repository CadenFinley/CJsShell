#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

// this will handle the parsing of the input, splitting args, applying envvars and aliases

class Parser {
public:
  std::vector<std::string> parse_command(const std::string& command);
};
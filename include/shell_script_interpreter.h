#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "cjsh.h"

class Shell;

enum class DebugLevel { NONE = 0, BASIC = 1, VERBOSE = 2, TRACE = 3 };

class ShellScriptInterpreter {
 public:
  ShellScriptInterpreter();
  ~ShellScriptInterpreter();

  void set_debug_level(DebugLevel level);
  DebugLevel get_debug_level() const;

  // Provide parser dependency explicitly to avoid relying on global g_shell
  void set_parser(Parser* parser) { this->shell_parser = parser; }

  int execute_block(const std::vector<std::string>& lines);
  std::vector<std::string> parse_into_lines(const std::string& script) {
    return shell_parser->parse_into_lines(script);
  }

 private:
  DebugLevel debug_level;
  Parser* shell_parser = nullptr;
};

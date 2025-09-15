#include "source_command.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "shell.h"
#include "shell_script_interpreter.h"

extern std::unique_ptr<Shell> g_shell;

int source_command(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    std::cerr << "source: missing file operand" << std::endl;
    return 1;
  }

  if (!g_shell) {
    std::cerr << "source: shell not initialized" << std::endl;
    return 1;
  }

  auto* interpreter = g_shell->get_shell_script_interpreter();
  if (!interpreter) {
    std::cerr << "source: script interpreter not available" << std::endl;
    return 1;
  }

  const std::string& path = args[1];
  std::ifstream file(path);
  if (!file) {
    std::cerr << "source: cannot open file '" << path << "'" << std::endl;
    return 1;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  auto lines = interpreter->parse_into_lines(buffer.str());
  return interpreter->execute_block(lines);
}

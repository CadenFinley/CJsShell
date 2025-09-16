#include "source_command.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "error_out.h"
#include "shell.h"
#include "shell_script_interpreter.h"

extern std::unique_ptr<Shell> g_shell;

int source_command(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    print_error({ErrorType::INVALID_ARGUMENT, "source", "missing file operand", {}});
    return 1;
  }

  if (!g_shell) {
    print_error({ErrorType::RUNTIME_ERROR, "source", "shell not initialized", {}});
    return 1;
  }

  auto* interpreter = g_shell->get_shell_script_interpreter();
  if (!interpreter) {
    print_error({ErrorType::RUNTIME_ERROR, "source", "script interpreter not available", {}});
    return 1;
  }

  const std::string& path = args[1];
  std::ifstream file(path);
  if (!file) {
    print_error({ErrorType::FILE_NOT_FOUND, "source", "cannot open file '" + path + "'", {}});
    return 1;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  auto lines = interpreter->parse_into_lines(buffer.str());
  return interpreter->execute_block(lines);
}

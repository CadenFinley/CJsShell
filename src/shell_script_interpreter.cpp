#include "shell_script_interpreter.h"

#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "cjsh.h"
#include "shell.h"

ShellScriptInterpreter::ShellScriptInterpreter() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing ShellScriptInterpreter" << std::endl;
  debug_level = DebugLevel::NONE;
  // Parser will be provided by Shell after construction.
  shell_parser = nullptr;
}

ShellScriptInterpreter::~ShellScriptInterpreter() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Destroying ShellScriptInterpreter" << std::endl;
}

void ShellScriptInterpreter::set_debug_level(DebugLevel level) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Setting script interpreter debug level to "
              << static_cast<int>(level) << std::endl;
  debug_level = level;
}

DebugLevel ShellScriptInterpreter::get_debug_level() const {
  return debug_level;
}

int ShellScriptInterpreter::execute_block(const std::vector<std::string>& lines) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Executing script block with " << lines.size()
              << " lines" << std::endl;

  if (g_shell == nullptr) {
    std::cerr << "Error: No shell instance available" << std::endl;
    return 1;
  }

  if (!shell_parser) {
    std::cerr << "Error: Script interpreter not initialized with a Parser" << std::endl;
    return 1;
  }

  for (const auto& line : lines) {

    std::vector<std::string> args = shell_parser->parse_command(line);
        if (g_debug_mode)
      std::cerr << "DEBUG: Executing line: " << line << std::endl;
    if (g_debug_mode)
      std::cerr << "DEBUG: Parsed into " << args.size() << " args" << std::endl;

    if (args.empty() || args[0].empty() || args[0][0] == '#') {
      if (g_debug_mode)
        std::cerr << "DEBUG: skipping line: " << line << std::endl;
      continue;
    }

    if (g_debug_mode) {
      std::cerr << "DEBUG: Executing command with args:";
      for (const auto& arg : args) {
        std::cerr << " [" << arg << "]";
      }
      std::cerr << std::endl;
    }

    // add detection for if an arg is a shell script file and execute it back through the interpreter


    int code = g_shell->execute_command(args);
    if (code != 0) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Command failed with exit code " << code
                  << ", stopping script execution" << std::endl;
      return code;
    }
  }

  return 0;
}

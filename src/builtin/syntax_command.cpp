#include "syntax_command.h"

#include <fstream>
#include <iostream>

#include "cjsh.h"

int syntax_command(const std::vector<std::string>& args, Shell* shell) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: syntax_command called with " << args.size()
              << " arguments" << std::endl;
  }

  if (args.size() < 2) {
    std::cout << "Usage: syntax <script_file>" << std::endl;
    std::cout << "       syntax -c <command_string>" << std::endl;
    std::cout << "Check syntax of shell scripts or commands" << std::endl;
    return 1;
  }

  if (!shell) {
    std::cerr << "syntax: shell not initialized" << std::endl;
    return 1;
  }

  std::vector<std::string> lines;
  
  if (args[1] == "-c" && args.size() >= 3) {
    // Check syntax of a command string
    std::string command;
    for (size_t i = 2; i < args.size(); ++i) {
      if (i > 2) command += " ";
      command += args[i];
    }
    
    auto script_interpreter = shell->get_shell_script_interpreter();
    if (!script_interpreter) {
      std::cerr << "syntax: script interpreter not available" << std::endl;
      return 1;
    }
    
    lines = script_interpreter->parse_into_lines(command);
  } else {
    // Check syntax of a script file
    const std::string& filename = args[1];
    std::ifstream file(filename);
    
    if (!file.is_open()) {
      std::cerr << "syntax: cannot open file '" << filename << "'" << std::endl;
      return 1;
    }
    
    std::string line;
    while (std::getline(file, line)) {
      lines.push_back(line);
    }
    file.close();
  }

  auto script_interpreter = shell->get_shell_script_interpreter();
  if (!script_interpreter) {
    std::cerr << "syntax: script interpreter not available" << std::endl;
    return 1;
  }

  auto errors = script_interpreter->validate_script_syntax(lines);
  
  if (errors.empty()) {
    std::cout << "No syntax errors found." << std::endl;
    return 0;
  } else {
    std::cout << "Syntax errors found:" << std::endl;
    for (const auto& error : errors) {
      std::cout << "  Line " << error.line_number << ": " << error.message << std::endl;
      if (!error.line_content.empty()) {
        std::cout << "    " << error.line_content << std::endl;
      }
    }
    return 1;
  }
}

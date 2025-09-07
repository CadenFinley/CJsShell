#include "eval_command.h"

#include <iostream>

#include "cjsh.h"

int eval_command(const std::vector<std::string>& args, Shell* shell) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: eval_command called with " << args.size()
              << " arguments" << std::endl;
  }

  if (args.size() < 2) {
    std::cerr << "eval: missing arguments" << std::endl;
    return 1;
  }

  std::string command_to_eval;
  for (size_t i = 1; i < args.size(); ++i) {
    if (i > 1)
      command_to_eval += " ";
    command_to_eval += args[i];
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: Evaluating command: " << command_to_eval << std::endl;
  }

  if (shell) {
    int result = shell->execute(command_to_eval);
    if (g_debug_mode) {
      std::cerr << "DEBUG: eval command returned: " << result << std::endl;
    }
    return result;
  } else {
    std::cerr << "eval: shell not initialized" << std::endl;
    return 1;
  }
}

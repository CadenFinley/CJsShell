#include "aihelp_command.h"

#include <cstdlib>
#include <iostream>
#include <string>

#include "main.h"

//make better system prompt for ai help that includes cjsh documentation

int aihelp_command(const std::vector<std::string>& args) {
  if (!g_ai || g_ai->get_api_key().empty()) {
    std::cerr << "Please set your OpenAI API key first." << std::endl;
    return 1;
  }
  const char* status_env = getenv("STATUS");
  if (!status_env) {
    std::cerr << "The last executed command status is unavailable" << std::endl;
    return 0;
  }
  int status = std::atoi(status_env);
  if (status == 0) {
    std::cerr << "The last executed command returned exitcode 0" << std::endl;
    return 0;
  }
  std::string message;
  if (args.size() > 1) {
    for (size_t i = 1; i < args.size(); ++i) {
      message += args[i] + " ";
    }
  } else {
    message =
        "I am encountering some issues with a cjsh command and would like some "
        "help. This is the most recent output: " +
        g_shell->last_terminal_output_error +
        " Here is the command I used: " + g_shell->last_command;
  }

  if (g_debug_mode) {
    std::cout << "Sending to AI: " << message << std::endl;
  }

  std::cout << g_ai->force_direct_chat_gpt(message, false) << std::endl;
  return 0;
}

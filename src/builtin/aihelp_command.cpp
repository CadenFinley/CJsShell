#include "aihelp_command.h"

#include <cstdlib>
#include <iostream>
#include <string>

#include "cjsh.h"
#include "error_out.h"
#include "system_prompts.h"

int aihelp_command(const std::vector<std::string>& args) {
  if (g_ai == nullptr) {
    print_error({ErrorType::RUNTIME_ERROR, "aihelp", 
                 "AI is not initialized - API configuration required", {}});
    return 1;
  }

  if (!g_ai->is_enabled()) {
    print_error({ErrorType::RUNTIME_ERROR, "aihelp", "AI is disabled", {}});
    return 1;
  }

  if (!g_ai || g_ai->get_api_key().empty()) {
    print_error({ErrorType::RUNTIME_ERROR, "aihelp", 
                 "Please set your OpenAI API key first", {}});
    return 1;
  }

  bool force_mode = false;
  std::string custom_prompt;
  std::string custom_model = g_ai->get_model();
  std::vector<std::string> remaining_args;

  for (size_t i = 1; i < args.size(); ++i) {
    if (args[i] == "-f") {
      force_mode = true;
    } else if (args[i] == "-p" && i + 1 < args.size()) {
      custom_prompt = args[++i];
    } else if (args[i] == "-m" && i + 1 < args.size()) {
      custom_model = args[++i];
    } else {
      remaining_args.push_back(args[i]);
    }
  }

  if (!force_mode) {
    const char* status_env = getenv("STATUS");
    if (!status_env) {
      print_error({ErrorType::RUNTIME_ERROR, "aihelp", 
                   "The last executed command status is unavailable", {}});
      return 0;
    }
    int status = std::atoi(status_env);
    if (status == 0) {
      print_error({ErrorType::RUNTIME_ERROR, "aihelp", 
                   "The last executed command returned exitcode 0", {}});
      return 0;
    }
  }

  std::string message;

  if (!custom_prompt.empty()) {
    message = custom_prompt;
  } else if (!remaining_args.empty()) {
    message = remaining_args[0];
    for (size_t i = 1; i < remaining_args.size(); ++i) {
      message += " " + remaining_args[i];
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
    std::cout << "Using model: " << custom_model << std::endl;
  }

  std::cout << g_ai->force_direct_chat_gpt(message +
                                               create_help_system_prompt() +
                                               "\n" + build_system_prompt(),
                                           false)
            << std::endl;
  return 0;
}

#include "aihelp_command.h"

#include <cstdlib>
#include <iostream>
#include <string>

#include "main.h"

std::string create_system_prompt() {
  return " You are an AI assistant for CJ's Shell (cjsh), a Unix-like shell "
         "with special features. "
         "Help users troubleshoot and fix issues with their commands or shell "
         "usage. "
         "\n\nABOUT CJ'S SHELL:"
         "\n- CJ's Shell is a custom shell with built-in AI capabilities, "
         "theming, plugins, and job control"
         "\n- It supports standard Unix commands and shell features like "
         "pipes, redirection, and background jobs"
         "\n- Configuration files: ~/.cjprofile (login mode), ~/.cjshrc "
         "(interactive mode)"
         "\n- Main directories: ~/.config/cjsh/, with subdirectories for "
         "plugins, themes, and colors"

         "\n\nKEY FEATURES:"
         "\n1. AI Integration - Commands: ai (chat mode), aihelp "
         "(troubleshooting)"
         "\n2. Plugin System - Managed via 'plugin' command (enable, disable, "
         "settings)"
         "\n3. Theming - Visual customization via 'theme' command"
         "\n4. Job Control - Standard fg, bg, jobs commands with process group "
         "management"
         "\n5. Environment - Uses STATUS variable for last command exit code"

         "\n\nCOMMON ISSUES:"
         "\n- Path issues: Check PATH variable using 'export' without arguments"
         "\n- Permission errors: Check file permissions with 'ls -la'"
         "\n- Command not found: May need to install package or check typos"
         "\n- Plugin errors: Try 'plugin disable NAME' to see if a plugin is "
         "causing issues"
         "\n- AI features unavailable: Check API key configuration with 'ai "
         "apikey'"

         "\n\nWhen responding:"
         "\n1. Be concise and clear with your explanations"
         "\n2. Provide commands the user can run to fix their issues"
         "\n3. Explain why the error occurred when possible"
         "\n4. Focus on practical solutions specific to cjsh when relevant";
}

int aihelp_command(const std::vector<std::string>& args) {
  if (!g_ai || g_ai->get_api_key().empty()) {
    std::cerr << "Please set your OpenAI API key first." << std::endl;
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
      std::cerr << "The last executed command status is unavailable"
                << std::endl;
      return 0;
    }
    int status = std::atoi(status_env);
    if (status == 0) {
      std::cerr << "The last executed command returned exitcode 0" << std::endl;
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

  std::cout << g_ai->force_direct_chat_gpt(message + create_system_prompt(),
                                           false)
            << std::endl;
  return 0;
}

#include "system_prompts.h"

#include <ctime>
#include <sstream>
#include <string>

std::string get_common_system_prompt() {
  return "You are an AI assistant for CJ's Shell (cjsh), a Unix-like shell "
         "with special features. "
         "CJ's Shell is a custom shell with built-in AI capabilities, "
         "theming, plugins, and job control. "
         "It supports standard Unix commands and shell features like "
         "pipes, redirection, and background jobs.";
}

std::string build_system_prompt() {
  std::stringstream prompt;

  const char* username = getenv("USER");
  const char* hostname = getenv("HOSTNAME");
  if (!hostname) {
    hostname = getenv("HOST");
  }

  std::time_t now = std::time(nullptr);
  char time_buffer[80];
  std::strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S",
                std::localtime(&now));

  char date_buffer[80];
  std::strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d",
                std::localtime(&now));

  prompt << get_common_system_prompt() << " ";
  prompt << "Current context: ";

  if (username) {
    prompt << "User: " << username << " ";
  }

  if (hostname) {
    prompt << "Machine: " << hostname << " ";
  }

  prompt << "Time: " << time_buffer << " ";
  prompt << "Date: " << date_buffer << " ";

  prompt << "Shell: cjsh ";

  const char* pwd = getenv("PWD");
  if (pwd) {
    prompt << "Directory: " << pwd << " ";
  }

  const char* status = getenv("STATUS");
  if (status) {
    prompt << "Last command status: " << status << " ";
  }

  return prompt.str();
}

std::string create_help_system_prompt() {
  std::stringstream prompt;

  prompt << get_common_system_prompt() << "\n";
  prompt << "Help users troubleshoot and fix issues with their commands or "
            "shell usage.\n\n";

  prompt
      << "ABOUT CJ'S SHELL:"
      << "\n- Configuration files: ~/.cjprofile (login mode), ~/.cjshrc "
      << "(interactive mode)"
      << "\n- Main directories: ~/.config/cjsh/, with subdirectories for "
      << "plugins, themes, and colors"

      << "\n\nKEY FEATURES:"
      << "\n1. AI Integration - Commands: ai (chat mode), aihelp "
      << "(troubleshooting)"
      << "\n2. Plugin System - Managed via 'plugin' command (enable, disable, "
      << "settings)"
      << "\n3. Theming - Visual customization via 'theme' command"
      << "\n4. Job Control - Standard fg, bg, jobs commands with process group "
      << "management"
      << "\n5. Environment - Uses STATUS variable for last command exit code"

      << "\n\nCOMMON ISSUES:"
      << "\n- Path issues: Check PATH variable using 'export' without arguments"
      << "\n- Permission errors: Check file permissions with 'ls -la'"
      << "\n- Command not found: May need to install package or check typos"
      << "\n- Plugin errors: Try 'plugin disable NAME' to see if a plugin is "
      << "causing issues"
      << "\n- AI features unavailable: Check API key configuration with 'ai "
      << "apikey'"

      << "\n\nWhen responding:"
      << "\n1. Be concise and clear with your explanations"
      << "\n2. Provide commands the user can run to fix their issues"
      << "\n3. Explain why the error occurred when possible"
      << "\n4. Focus on practical solutions specific to cjsh when relevant";

  return prompt.str();
}

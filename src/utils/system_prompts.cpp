#include "system_prompts.h"

#include <ctime>
#include <sstream>
#include <string>
#include <string_view>

inline constexpr std::string_view COMMON_SYSTEM_PROMPT =
    "You are an expert AI assistant for CJ's Shell (cjsh), a powerful "
    "Unix-like shell. "
    "You have deep knowledge of shell commands, scripting, system "
    "administration, and development workflows. "
    "CJ's Shell supports standard Unix commands plus AI-powered features, "
    "theming, plugins, and advanced job control. "
    "When helping users, provide practical, actionable solutions with specific "
    "commands they can run. "
    "Always consider the user's current directory context and suggest the most "
    "efficient approach. "
    "You can generate shell scripts, analyze errors, suggest optimizations, "
    "and explain complex command sequences.";

std::string get_common_system_prompt() {
    return std::string(COMMON_SYSTEM_PROMPT);
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

    const char* status = getenv("?");
    if (status) {
        prompt << "Last command status: " << status << " ";
    }

    return prompt.str();
}

inline constexpr std::string_view HELP_SYSTEM_PROMPT = R"(
ABOUT CJ'S SHELL:
- Configuration files: ~/.cjprofile (login mode), ~/.cjshrc (interactive mode)
- Main directories: ~/.config/cjsh/, with subdirectories for plugins, themes, and colors

KEY FEATURES:
1. AI Integration - Commands: ai (chat mode), aihelp (troubleshooting)
2. Plugin System - Managed via 'plugin' command (enable, disable, settings)
3. Theming - Visual customization via 'theme' command
4. Job Control - Standard fg, bg, jobs commands with process group management
5. Environment - Uses STATUS variable for last command exit code

COMMON ISSUES:
- Path issues: Check PATH variable using 'export' without arguments
- Permission errors: Check file permissions with 'ls -la'
- Command not found: May need to install package or check typos
- Plugin errors: Try 'plugin disable NAME' to see if a plugin is causing issues
- AI features unavailable: Check API key configuration with 'ai apikey'

When responding:
1. Be concise and clear with your explanations
2. Provide commands the user can run to fix their issues
3. Explain why the error occurred when possible
4. Focus on practical solutions specific to cjsh when relevant
)";

std::string create_help_system_prompt() {
    std::stringstream prompt;

    prompt << get_common_system_prompt() << "\n";
    prompt << "Help users troubleshoot and fix issues with their commands or "
              "shell usage.\n\n";
    prompt << HELP_SYSTEM_PROMPT;

    return prompt.str();
}

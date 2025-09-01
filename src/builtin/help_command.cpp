#include "help_command.h"

#include <iostream>
#include <string>

#include "main.h"

int help_command() {
  if (g_debug_mode) {
    std::cerr << "DEBUG: help_command called" << std::endl;
  }

  const std::string section_separator = "\n" + std::string(80, '-') + "\n";
  std::cout << "\nCJ'S SHELL COMMAND REFERENCE" << section_separator;

  std::cout << "AI COMMANDS:\n\n";

  std::cout << "  ai                      Access AI assistant features and "
               "settings\n";
  std::cout << "    Usage: ai [subcommand] [options]\n";
  std::cout << "    Examples: 'ai' (enters AI chat mode), 'ai apikey set "
               "YOUR_KEY', 'ai chat history'\n";
  std::cout << "    Subcommands:\n";
  std::cout
      << "      log                 Save the recent chat exchange to a file\n";
  std::cout << "      apikey              View or set the OpenAI API key\n";
  std::cout
      << "      chat                Access AI chat commands (history, cache)\n";
  std::cout << "      get [KEY]           Retrieve specific response data\n";
  std::cout << "      dump                Display all response data and last "
               "prompt\n";
  std::cout << "      mode [TYPE]         Set or view the assistant mode\n";
  std::cout << "      file                Manage context files (add, remove, "
               "active, available)\n";
  std::cout << "      directory           Manage save directory for "
               "AI-generated files\n";
  std::cout
      << "      model [MODEL]       Set or view the AI model being used\n";
  std::cout << "      rejectchanges       Reject AI suggested code changes\n";
  std::cout
      << "      timeoutflag [SECS]  Set timeout duration for AI requests\n\n";

  std::cout << "  aihelp [QUERY]          Get troubleshooting help from AI\n";
  std::cout << "    Usage: aihelp [optional error description]\n";
  std::cout << "    Example: 'aihelp why is my command failing?'\n";
  std::cout
      << "    Note: Without arguments, will analyze the most recent error\n\n";

  std::cout << "USER SETTINGS:\n\n";

  std::cout << "  user                    Access and manage user settings\n";
  std::cout << "    Usage: user [subcommand] [options]\n";
  std::cout << "    Subcommands:\n";
  std::cout << "      testing             Toggle debug mode (enable/disable)\n";
  std::cout
      << "      checkforupdates     Control whether updates are checked\n";
  std::cout << "      silentupdatecheck   Toggle silent update checking\n";
  std::cout << "      titleline           Toggle title line display\n";
  std::cout << "      update              Manage update settings and perform "
               "manual update checks\n";
  std::cout << "    Example: 'user update check', 'user testing enable'\n\n";

  std::cout << "THEME MANAGEMENT:\n\n";

  std::cout << "  theme [NAME]            View current theme or switch to a "
               "new theme\n";
  std::cout << "    Usage: theme [name] or theme load [name]\n";
  std::cout << "    Example: 'theme dark', 'theme load light'\n";
  std::cout << "    Note: Without arguments, displays the current theme and "
               "available themes\n\n";

  std::cout << "PLUGIN MANAGEMENT:\n\n";

  std::cout << "  plugin                  Manage shell plugins\n";
  std::cout << "    Usage: plugin [subcommand] [options]\n";
  std::cout << "    Subcommands:\n";
  std::cout << "      available           List all available plugins\n";
  std::cout << "      enabled             List currently enabled plugins\n";
  std::cout << "      enableall           Enable all available plugins\n";
  std::cout << "      disableall          Disable all enabled plugins\n";
  std::cout << "      enable [NAME]       Enable a specific plugin\n";
  std::cout << "      disable [NAME]      Disable a specific plugin\n";
  std::cout << "      info [NAME]         Show information about a plugin\n";
  std::cout << "      commands [NAME]     List commands provided by a plugin\n";
  std::cout << "      settings [NAME]     View or modify plugin settings\n";
  std::cout
      << "      install [PATH]      Install a new plugin from the given path\n";
  std::cout << "      uninstall [NAME]    Remove an installed plugin"
            << std::endl;
  std::cout
      << "    Example: 'plugin enable git_tools', 'plugin info markdown'\n\n";

  std::cout << "BUILT-IN SHELL COMMANDS:\n\n";

  std::cout << "  cd [DIR]                Change the current directory\n";
  std::cout << "    Usage: cd [directory]\n";
  std::cout << "    Examples: 'cd /path/to/dir', 'cd ~', 'cd ..' (parent "
               "directory), 'cd' (home directory)\n\n";

  std::cout << "  alias [NAME=VALUE]      Create or display command aliases\n";
  std::cout << "    Usage: alias [name=value]\n";
  std::cout << "    Examples: 'alias' (show all), 'alias ll=\"ls -la\"', "
               "'alias gs=\"git status\"'\n\n";

  std::cout << "  unalias [NAME]          Remove a command alias\n";
  std::cout << "    Usage: unalias name\n";
  std::cout << "    Example: 'unalias ll'\n\n";

  std::cout
      << "  export [NAME=VALUE]     Set or display environment variables\n";
  std::cout << "    Usage: export [name=value]\n";
  std::cout << "    Examples: 'export' (show all), 'export "
               "PATH=\"$PATH:/new/path\"'\n\n";

  std::cout << "  unset [NAME]            Remove an environment variable\n";
  std::cout << "    Usage: unset name\n";
  std::cout << "    Example: 'unset TEMP_VAR'\n\n";

  std::cout << "  source [FILE]           Execute commands from a file\n";
  std::cout << "    Usage: source path/to/file\n";
  std::cout << "    Example: 'source ~/.cjshrc'\n\n";

  std::cout << "  eval [EXPRESSION]       Evaluate a shell expression\n";
  std::cout << "    Usage: eval expression\n";
  std::cout << "    Example: 'eval echo Hello, World!'\n\n";

  std::cout << "COMMON SYSTEM COMMANDS:\n\n";

  std::cout << "  clear                   Clear the terminal screen\n";
  std::cout << "    Usage: clear\n\n";

  std::cout << "  exit or quit            Exit the application\n";
  std::cout << "    Usage: exit or quit\n\n";

  std::cout << "  help                    Display this help message\n";
  std::cout << "    Usage: help\n";

  std::cout << section_separator;
  std::cout << "FILESYSTEM AND CONFIGURATION:\n\n";

  std::cout << "  Configuration Files:\n";
  std::cout << "    ~/.cjprofile          Environment variable, PATH setup, "
               "and optional statup args (login mode)\n";
  std::cout << "    ~/.cjshrc             Aliases, functions, themes, plugins "
               "(interactive mode)\n\n";

  std::cout << "  Primary Directories:\n";
  std::cout << "    ~/.config/cjsh               Main data directory for CJ's "
               "Shell\n";
  std::cout << "    cjsh/plugins       Where plugins are stored\n";
  std::cout << "    cjsh/themes        Where themes are stored\n";
  std::cout
      << "    cjsh/colors        Where color configurations are stored\n\n";

  std::cout << "  File Sourcing Order:\n";
  std::cout << "    1. ~/.profile         (if exists, login mode only)\n";
  std::cout << "    2. ~/.cjprofile       (login mode only)\n";
  std::cout << "    3. ~/.cjshrc          (interactive mode only, unless "
               "--no-source specified)\n\n";

  std::cout << "STARTUP ARGUMENTS:\n\n";

  std::cout << "  Login and Execution:\n";
  std::cout << "    -l, --login           Start shell in login mode\n";
  std::cout << "    -c, --command CMD     Execute CMD and exit\n";
  std::cout << "    --set-as-shell        Show instructions to set as default "
               "shell\n\n";

  std::cout << "  Feature Toggles:\n";
  std::cout << "    --no-plugins          Disable plugins\n";
  std::cout << "    --no-themes           Disable themes\n";
  std::cout << "    --no-ai               Disable AI features\n";
  std::cout << "    --no-colors           Disable colors\n";
  std::cout << "    --no-titleline        Disable title line display\n";
  std::cout << "    --no-source           Don't source the ~/.cjshrc file\n\n";

  std::cout << "  Updates and Information:\n";
  std::cout << "    -v, --version         Display version and exit\n";
  std::cout << "    -h, --help            Display help and exit\n";
  std::cout << "    --update              Check for updates and install if "
               "available\n";
  std::cout << "    --check-update        Check for update\n";
  std::cout << "    --no-update           Do not check for update on launch\n";
  std::cout << "    --silent-updates      Enable silent update checks\n";
  std::cout << "    --splash              Display splash screen and exit\n";
  std::cout << "    -d, --debug           Enable debug mode\n";

  std::cout << section_separator;
  std::cout << "NOTE: Many commands have their own help. Try [command] help "
               "for details.\n";
  std::cout << "Examples: 'ai help', 'user help', 'plugin help', etc.\n";
  std::cout << section_separator;

  return 0;
}

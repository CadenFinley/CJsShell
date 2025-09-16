#include "help_command.h"

#include <iostream>
#include <string>

#include "cjsh.h"
#include "usage.h"

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
      << "      timeoutflag [SECS]  Set timeout duration for AI requests\n";
  std::cout << "      initialinstruction [TEXT] Set or view initial system "
               "instruction\n";
  std::cout << "      name [NAME]         Set or view assistant name\n";
  std::cout << "      saveconfig          Save current AI configuration\n";
  std::cout << "      config              Manage AI configurations (list, "
               "switch, save)\n";
  std::cout << "      voice [VOICE]       Set or view voice for dictation\n";
  std::cout << "      voicedictation [enable|disable] Enable/disable voice "
               "dictation\n";
  std::cout << "      voicedictationinstructions [TEXT] Set voice dictation "
               "instructions\n";
  std::cout << "      help                Show detailed AI command help\n\n";

  std::cout << "  aihelp [QUERY]          Get troubleshooting help from AI\n";
  std::cout << "    Usage: aihelp [optional error description]\n";
  std::cout << "    Example: 'aihelp why is my command failing?'\n";
  std::cout
      << "    Note: Without arguments, will analyze the most recent error\n\n";

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

  std::cout << "Core Shell Commands:\n";
  std::cout << "  cd [DIR]                Change the current directory\n";
  std::cout << "    Usage: cd [directory]\n";
  std::cout << "    Examples: 'cd /path/to/dir', 'cd ~', 'cd ..' (parent "
               "directory), 'cd' (home directory)\n\n";

  std::cout
      << "  pwd                     Print the current working directory\n";
  std::cout << "    Usage: pwd\n\n";

  std::cout
      << "  echo [ARGS...]          Display arguments to standard output\n";
  std::cout << "    Usage: echo [text...]\n";
  std::cout << "    Example: 'echo Hello, World!'\n\n";

  std::cout << "  printf FORMAT [ARGS...] Format and print arguments\n";
  std::cout << "    Usage: printf format [arguments...]\n";
  std::cout << "    Example: 'printf \"Hello %s\\n\" World'\n\n";

  std::cout << "  ls [OPTIONS] [FILES...] List directory contents\n";
  std::cout << "    Usage: ls [options] [files/directories]\n";
  std::cout << "    Example: 'ls -la', 'ls ~/Documents'\n\n";

  std::cout << "  help                    Display this help message\n";
  std::cout << "    Usage: help\n\n";

  std::cout
      << "  exit [N], quit [N]      Exit the shell with optional status\n";
  std::cout << "    Usage: exit [exit_code] or quit [exit_code]\n";
  std::cout << "    Example: 'exit 0', 'quit'\n\n";

  std::cout << "Variable and Environment Management:\n";
  std::cout
      << "  export [NAME=VALUE]     Set or display environment variables\n";
  std::cout << "    Usage: export [name=value]\n";
  std::cout << "    Examples: 'export' (show all), 'export "
               "PATH=\"$PATH:/new/path\"'\n\n";

  std::cout << "  unset [NAME]            Remove an environment variable\n";
  std::cout << "    Usage: unset name\n";
  std::cout << "    Example: 'unset TEMP_VAR'\n\n";

  std::cout << "  set [OPTIONS] [ARGS...] Set shell options and positional "
               "parameters\n";
  std::cout << "    Usage: set [options] [arguments]\n";
  std::cout << "    Example: 'set -x' (enable debug mode)\n\n";

  std::cout << "  readonly [NAME=VALUE]   Mark variables as read-only\n";
  std::cout << "    Usage: readonly [name[=value]]\n";
  std::cout << "    Example: 'readonly PATH'\n\n";

  std::cout << "  alias [NAME=VALUE]      Create or display command aliases\n";
  std::cout << "    Usage: alias [name=value]\n";
  std::cout << "    Examples: 'alias' (show all), 'alias ll=\"ls -la\"', "
               "'alias gs=\"git status\"'\n\n";

  std::cout << "  unalias [NAME]          Remove a command alias\n";
  std::cout << "    Usage: unalias name\n";
  std::cout << "    Example: 'unalias ll'\n\n";

  std::cout << "Script and Command Execution:\n";
  std::cout << "  source [FILE], . [FILE] Execute commands from a file\n";
  std::cout << "    Usage: source path/to/file or . path/to/file\n";
  std::cout << "    Example: 'source ~/.cjshrc', '. ~/.bashrc'\n\n";

  std::cout << "  eval [EXPRESSION]       Evaluate a shell expression\n";
  std::cout << "    Usage: eval expression\n";
  std::cout << "    Example: 'eval echo Hello, World!'\n\n";

  std::cout << "  exec [COMMAND [ARGS...]] Replace shell with command\n";
  std::cout << "    Usage: exec command [arguments]\n";
  std::cout << "    Example: 'exec bash' (replace shell with bash)\n\n";

  std::cout << "  test [EXPR], [ EXPR ]   Evaluate conditional expressions\n";
  std::cout << "    Usage: test expression or [ expression ]\n";
  std::cout << "    Examples: 'test -f file.txt', '[ -d directory ]'\n\n";

  std::cout << "  :                       Null command (always succeeds)\n";
  std::cout << "    Usage: :\n";
  std::cout << "    Note: Returns exit status 0\n\n";

  std::cout << "Flow Control:\n";
  std::cout << "  break [N]               Break out of loops\n";
  std::cout << "    Usage: break [levels]\n";
  std::cout << "    Example: 'break', 'break 2'\n\n";

  std::cout << "  continue [N]            Continue loop iteration\n";
  std::cout << "    Usage: continue [levels]\n";
  std::cout << "    Example: 'continue', 'continue 2'\n\n";

  std::cout << "  return [N]              Return from function\n";
  std::cout << "    Usage: return [exit_status]\n";
  std::cout << "    Example: 'return 0', 'return 1'\n\n";

  std::cout << "  if [CONDITION]          Conditional command execution\n";
  std::cout << "    Usage: if [condition]; then [commands]; fi\n";
  std::cout << "    Example: 'if [ -f file ]; then echo found; fi'\n\n";

  std::cout << "Job Control:\n";
  std::cout << "  jobs                    List active jobs\n";
  std::cout << "    Usage: jobs\n\n";

  std::cout << "  fg [JOBSPEC]            Bring job to foreground\n";
  std::cout << "    Usage: fg [job_id]\n";
  std::cout << "    Example: 'fg', 'fg %1'\n\n";

  std::cout << "  bg [JOBSPEC]            Resume job in background\n";
  std::cout << "    Usage: bg [job_id]\n";
  std::cout << "    Example: 'bg', 'bg %1'\n\n";

  std::cout << "  wait [JOBSPEC...]       Wait for jobs to complete\n";
  std::cout << "    Usage: wait [job_ids...]\n";
  std::cout << "    Example: 'wait', 'wait %1 %2'\n\n";

  std::cout << "  kill [SIGNAL] JOBSPEC   Send signal to job\n";
  std::cout << "    Usage: kill [-signal] job_id\n";
  std::cout << "    Example: 'kill %1', 'kill -9 %1'\n\n";

  std::cout << "Input/Output and Options:\n";
  std::cout << "  read [OPTIONS] [NAME...] Read input into variables\n";
  std::cout << "    Usage: read [options] variable_name\n";
  std::cout << "    Example: 'read name', 'read -p \"Enter: \" value'\n\n";

  std::cout << "  getopts OPTSTRING NAME  Parse command options\n";
  std::cout << "    Usage: getopts optstring variable\n";
  std::cout << "    Example: 'getopts \"abc:\" opt'\n\n";

  std::cout << "  shift [N]               Shift positional parameters\n";
  std::cout << "    Usage: shift [count]\n";
  std::cout << "    Example: 'shift', 'shift 2'\n\n";

  std::cout << "System and Process Information:\n";
  std::cout << "  type [NAME...]          Display command type information\n";
  std::cout << "    Usage: type command_name\n";
  std::cout << "    Example: 'type ls', 'type cd'\n\n";

  std::cout << "  hash [COMMAND...]       Manage command hash table\n";
  std::cout << "    Usage: hash [command]\n";
  std::cout << "    Example: 'hash', 'hash ls'\n\n";

  std::cout << "  times                   Display process times\n";
  std::cout << "    Usage: times\n\n";

  std::cout << "  umask [MODE]            Set file creation mask\n";
  std::cout << "    Usage: umask [mode]\n";
  std::cout << "    Example: 'umask', 'umask 022'\n\n";

  std::cout << "  history [OPTIONS]       Command history management\n";
  std::cout << "    Usage: history [options]\n";
  std::cout << "    Example: 'history', 'history 10'\n\n";

  std::cout << "Signal Handling:\n";
  std::cout << "  trap [ACTION] [SIGNAL...] Set signal handlers\n";
  std::cout << "    Usage: trap [action] [signals]\n";
  std::cout << "    Example: 'trap \"echo caught\" INT'\n\n";

  std::cout << "Shell-Specific Commands:\n";
  std::cout << "  version [OPTIONS]       Display version information\n";
  std::cout << "    Usage: version [options]\n\n";

  std::cout << "  restart [OPTIONS]       Restart the shell\n";
  std::cout << "    Usage: restart [options]\n\n";

  std::cout << "  uninstall               Uninstall the shell\n";
  std::cout << "    Usage: uninstall\n\n";

  std::cout << "  approot                 Change to application root\n";
  std::cout << "    Usage: approot\n\n";

  std::cout << "  terminal                Activate terminal menu\n";
  std::cout << "    Usage: terminal\n\n";

  std::cout << "  prompt_test [OPTIONS]   Test prompt configurations\n";
  std::cout << "    Usage: prompt_test [options]\n\n";

  std::cout << "COMMON SYSTEM COMMANDS:\n\n";

  std::cout << "  clear                   Clear the terminal screen\n";
  std::cout << "    Usage: clear\n";
  std::cout
      << "    Note: This is typically an external command, not a builtin\n\n";

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

  print_usage();

  std::cout << section_separator;
  std::cout << "KEYBOARD SHORTCUTS:\n\n";
  std::cout << "  F1                      Show isocline key bindings and shortcuts\n";
  std::cout << "    Note: Displays available readline-style editing commands and navigation shortcuts\n\n";
  std::cout << section_separator;
  std::cout << "NOTE: Many commands have their own help. Try [command] help "
               "for details.\n";
  std::cout << "Examples: 'ai help', 'plugin help', 'theme help', etc.\n";
  std::cout << section_separator;

  return 0;
}

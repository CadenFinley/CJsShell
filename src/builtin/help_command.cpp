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
  std::cout << "    Chatting:\n";
  std::cout << "      ai chat <message>            Send a prompt to the "
               "assistant\n";
  std::cout << "      ai chat history [clear]      Show or clear cached "
               "history\n";
  std::cout << "      ai chat help                 Show chat-specific help\n";
  std::cout << "      ai log                       Save the most recent "
               "exchange\n";
  std::cout << "    Context files:\n";
  std::cout << "      ai file                      Summarize active and "
               "available files\n";
  std::cout << "      ai file add <file>|all       Add file(s) from the current "
               "directory\n";
  std::cout << "      ai file remove <file>|all    Remove file(s) from "
               "context\n";
  std::cout << "      ai file active               List files currently "
               "attached\n";
  std::cout << "      ai file available            List files in the current "
               "directory\n";
  std::cout << "      ai file refresh              Re-read attached files from "
               "disk\n";
  std::cout << "      ai file clear                Remove all attached files\n";
  std::cout << "    Configuration & storage:\n";
  std::cout << "      ai directory                 Show where generated files "
               "are saved\n";
  std::cout << "      ai directory set             Use the present working "
               "directory\n";
  std::cout << "      ai directory clear           Reset the save directory to "
               "default\n";
  std::cout << "      ai saveconfig                Write current settings to "
               "disk\n";
  std::cout << "      ai config                    Show the active "
               "configuration name\n";
  std::cout << "      ai config list               List saved configurations\n";
  std::cout << "      ai config switch <name>      Switch (alias: load) to a "
               "configuration\n";
  std::cout << "      ai config load <name>        Alias for 'config switch'\n";
  std::cout << "      ai config save <name>        Overwrite an existing "
               "configuration\n";
  std::cout << "      ai config saveas <name>      Save current settings under "
               "a new name\n";
  std::cout << "    Personalization & limits:\n";
  std::cout << "      ai mode [type]               Get or set assistant mode\n";
  std::cout << "      ai model [name]              Get or set the model ID\n";
  std::cout << "      ai initialinstruction [text] Get or set the system "
               "instruction\n";
  std::cout << "      ai name [name]               Get or set the assistant "
               "name\n";
  std::cout << "      ai timeoutflag [seconds]     Get or set the request "
               "timeout\n";
  std::cout << "      ai voice [voice]             Get or set dictation voice\n";
  std::cout << "      ai voicedictation enable|disable  Toggle voice "
               "dictation\n";
  std::cout << "      ai voicedictationinstructions [text] Set dictation "
               "instructions\n";
  std::cout << "    Diagnostics:\n";
  std::cout << "      ai get <key>                 Show a specific response "
               "field\n";
  std::cout << "      ai dump                      Dump all response data and "
               "last prompt\n";
  std::cout << "      ai rejectchanges             Reject pending AI edits\n";
  std::cout << "      ai help                      Show this summary\n\n";

  std::cout << "  aihelp [QUERY]          Get troubleshooting help from AI\n";
  std::cout << "    Usage: aihelp [-f] [-p prompt] [-m model] [error "
               "description]\n";
  std::cout << "    Flags:\n";
  std::cout << "      -f                   Ignore last exit status and force "
               "assistance\n";
  std::cout << "      -p <prompt>          Provide a custom opening prompt\n";
  std::cout << "      -m <model>           Override the model for this request\n";
  std::cout << "    Note: With no description, the last failing command is "
               "analyzed.\n\n";

  std::cout << "THEME MANAGEMENT:\n\n";

  std::cout << "  theme [NAME]            View current theme or switch to a "
               "new one\n";
  std::cout << "    Usage: theme [name]\n";
  std::cout << "    Subcommands:\n";
  std::cout << "      theme load <name>            Load a theme by name\n";
  std::cout << "      theme info <name>            Show theme metadata and "
               "requirements\n";
  std::cout << "      theme preview <name|all>     Preview one or all local "
               "themes\n";
  std::cout << "      theme reload                 Reload the active theme from "
               "disk\n";
  std::cout << "      theme uninstall <name>       Remove an installed theme\n";
  std::cout << "    Note: Without arguments, lists the active and available "
               "themes.\n\n";

  std::cout << "PLUGIN MANAGEMENT:\n\n";

  std::cout << "  plugin                  Manage shell plugins\n";
  std::cout << "    Usage: plugin <subcommand> [options]\n";
  std::cout << "    Subcommands:\n";
  std::cout << "      available                 List all available plugins\n";
  std::cout << "      enabled                   List currently enabled plugins\n";
  std::cout << "      enable <name>             Enable a plugin\n";
  std::cout << "      disable <name>            Disable a plugin\n";
  std::cout << "      enableall / disableall    Toggle all plugins at once\n";
  std::cout << "      info <name>               Show plugin information\n";
  std::cout << "      commands <name>           List commands provided by a "
               "plugin\n";
  std::cout << "      settings                  Show settings for every plugin\n";
  std::cout << "      settings <name>           Show settings for one plugin\n";
  std::cout << "      settings <name> set <key> <value>  Update a plugin "
               "setting\n";
  std::cout << "      stats                     Display plugin system "
               "statistics\n";
  std::cout << "      uninstall <name>          Remove an installed plugin\n";
  std::cout << "    Tip: Run 'plugin help' for plugin-specific guidance.\n\n";

  std::cout << "BUILT-IN SHELL COMMANDS:\n\n";

  std::cout << "Core Shell Commands:\n";
  std::cout << "  cd [DIR]                Change the current directory (smart "
               "cd by default)\n";
  std::cout << "    Usage: cd [directory]\n";
  std::cout << "  pwd [-L|-P]             Print the current working "
               "directory\n";
  std::cout << "    Usage: pwd [-L|-P]\n";
  std::cout << "  echo [ARGS...]          Display arguments to standard "
               "output\n";
  std::cout << "    Usage: echo [text...]\n";
  std::cout << "  printf FORMAT [ARGS...] Format and print arguments\n";
  std::cout << "    Usage: printf format [arguments...]\n";
  std::cout << "  ls [OPTIONS] [PATH...]  List directory contents (cjsh "
               "variant)\n";
  std::cout << "    Usage: ls [options] [files/directories] (see 'ls --help' "
               "for supported flags)\n";
  std::cout << "  help                    Display this help message\n";
  std::cout << "    Usage: help\n";
  std::cout << "  exit [N], quit [N]      Exit the shell with optional status\n";
  std::cout << "    Usage: exit [code]\n\n";

  std::cout << "Variables and Functions:\n";
  std::cout << "  export [NAME=VALUE]     Set or display environment "
               "variables\n";
  std::cout << "    Usage: export [name=value]\n";
  std::cout << "  unset NAME              Remove an environment variable\n";
  std::cout << "    Usage: unset name\n";
  std::cout << "  set [OPTIONS] [ARGS...] Set shell options and positional "
               "parameters\n";
  std::cout << "    Usage: set [options] [arguments]\n";
  std::cout << "  readonly [NAME[=VALUE]] Mark variables as read-only\n";
  std::cout << "    Usage: readonly [name[=value]]\n";
  std::cout << "  alias [NAME=VALUE]      Create or display command aliases\n";
  std::cout << "    Usage: alias [name=value]\n";
  std::cout << "  unalias NAME            Remove a command alias\n";
  std::cout << "    Usage: unalias name\n";
  std::cout << "  local NAME[=VALUE]      Declare local variables inside "
               "functions\n";
  std::cout << "    Usage: local name[=value] [...]\n\n";

  std::cout << "Scripting and Evaluation:\n";
  std::cout << "  source FILE             Execute commands from a file (alias "
               "'.')\n";
  std::cout << "    Usage: source path/to/file\n";
  std::cout << "  eval STRING             Evaluate a shell expression\n";
  std::cout << "    Usage: eval expression\n";
  std::cout << "  exec COMMAND [ARGS...]  Replace the shell with another "
               "command\n";
  std::cout << "    Usage: exec command [arguments]\n";
  std::cout << "  test EXPR, [ EXPR ]     Evaluate conditional expressions\n";
  std::cout << "    Usage: test expression | [ expression ]\n";
  std::cout << "  [[ EXPR ]]              Evaluate extended test expressions\n";
  std::cout << "  shift [N]               Shift positional parameters left by "
               "N\n";
  std::cout << "    Usage: shift [count]\n";
  std::cout << "  getopts OPTSTRING NAME  Parse positional parameters as "
               "options\n";
  std::cout << "    Usage: getopts optstring name [args...]\n";
  std::cout << "  syntax [OPTIONS] <FILE> Check scripts for syntax/style "
               "issues\n";
  std::cout << "    Usage: syntax [options] <script_file>\n";
  std::cout << "           syntax [options] -c <command_string>\n\n";

  std::cout << "Flow Control:\n";
  std::cout << "  :                       Null command (always succeeds)\n";
  std::cout << "  break [N]               Break out of loops\n";
  std::cout << "  continue [N]            Continue loop iteration\n";
  std::cout << "  return [N]              Return from function\n";
  std::cout << "  if [condition]          Conditional execution (requires "
               "then/fi)\n\n";

  std::cout << "Job Control:\n";
  std::cout << "  jobs                    List active jobs\n";
  std::cout << "  fg [JOBSPEC]            Bring a job to the foreground\n";
  std::cout << "  bg [JOBSPEC]            Resume a job in the background\n";
  std::cout << "  wait [JOBSPEC...]       Wait for jobs to complete\n";
  std::cout << "  kill [-signal] TARGET   Send a signal to a job or process\n";
  std::cout << "    Usage: kill [-s sigspec | -n signum | -sigspec] pid | "
               "jobspec ...\n\n";

  std::cout << "Input, History, and Diagnostics:\n";
  std::cout << "  read [OPTIONS] [NAME...] Read input into shell variables\n";
  std::cout << "    Usage: read [-r] [-p prompt] [-n nchars] [-d delim] [name "
               "...]\n";
  std::cout << "    Note: Timeout (-t) is parsed but not yet implemented and "
               "will return an error.\n";
  std::cout << "  history [COUNT]         Show command history (or first "
               "COUNT entries)\n";
  std::cout << "  type [-afptP] NAME...    Describe how a command name is "
               "resolved\n";
  std::cout << "  hash [-r|-d] [NAME...]  Cache, display, or clear hashed "
               "command paths\n";
  std::cout << "  times                   Display process times for shell and "
               "children\n";
  std::cout << "  umask [-S] [MODE]       Show or set the file creation mask\n";
  std::cout << "  trap [-lp] [ACTION] [SIGNAL...]  Set or query signal "
               "handlers\n\n";

  std::cout << "Shell-Specific Commands:\n";
  std::cout << "  version                 Display cjsh version information\n";
  std::cout << "  login-startup-arg FLAG  Persist a startup flag in ~/.cjprofile\n";
  std::cout << "    Example flags: --no-plugins, --no-themes, --minimal, "
               "--debug\n";
  std::cout << "  approot                 Change to the cjsh data directory\n";
  std::cout << "  prompt_test             Print available prompt variables and "
               "metrics\n\n";

  std::cout << "COMMON SYSTEM COMMANDS:\n\n";

  std::cout << "  clear                   Clear the terminal screen (external "
               "command)\n";
  std::cout << "    Note: This is typically provided by the system, not cjsh.\n\n";

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
  std::cout
      << "  F1                      Show isocline key bindings and shortcuts\n";
  std::cout << "    Note: Displays available readline-style editing commands "
               "and navigation shortcuts\n\n";
  std::cout << section_separator;
  std::cout << "NOTE: Many commands have their own help. Try [command] help "
               "for details.\n";
  std::cout << "Examples: 'ai help', 'plugin help', 'theme help', etc.\n";
  std::cout << section_separator;

  return 0;
}

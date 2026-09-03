/*
  help_command.cpp

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "help_command.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "cjsh_filesystem.h"
#include "usage.h"
#include "version_command.h"

std::string get_help() {
    const std::string separator(80, '-');
    std::ostringstream output;

    auto heading = [&](const std::string& title) {
        output << "\n" << title << "\n" << separator << "\n";
    };

    output << "\nCJSH QUICK REFERENCE\n" << separator << "\n";
    output << get_version_message();
    output << "POSIX shell scripting meets modern shell features\n";

    heading("Project source");
    output << "  Git repository:  https://github.com/CadenFinley/CJsShell\n";
    output << "  Documentation:   https://cadenfinley.github.io/CJsShell/\n";

    heading("Built-in commands");
    struct BuiltinInfo {
        const char* name;
        const char* description;
    };

    std::vector<BuiltinInfo> builtins = {
        // Navigation and file system
        {"cd", "Change the current directory (smart cd by default)"},
        {"approot",
         "Jump to or print cjsh config/cache/history/firstboot/completion/executable roots"},
        {"pushd", "Push the current directory onto a stack"},
        {"popd", "Pop the top directory from the stack"},
        {"dirs", "Display the directory stack"},
        {"pwd", "Print the current working directory"},

        // Output and formatting
        {"echo", "Print arguments separated by spaces"},
        {"printf", "Format and print data using printf-style specifiers"},

        // Shell control
        {"help", "Display this overview"},
        {"version", "Show cjsh version and build information"},
        {"exit / quit / bye", "Leave the shell with an optional exit status"},
        {"restart", "Re-exec cjsh in place (use --no-flags for a fresh launch)"},

        // Script execution
        {"eval", "Evaluate a string as shell code"},
        {"exec", "Replace the shell process with another program"},
        {"source / .", "Execute commands from a file in the current shell"},
        {"command", "Execute command bypassing functions and aliases"},
        {"builtin", "Run a builtin directly, bypassing functions and PATH"},

        // Variables and environment
        {"set", "Adjust shell options or positional parameters"},
        {"shift", "Rotate positional parameters to the left"},
        {"export", "Set or display environment variables"},
        {"unset", "Remove environment variables"},
        {"local", "Declare local variables inside functions"},
        {"declare / typeset", "Set variable attributes and values"},
        {"readonly", "Mark variables as read-only"},

        // Input/output
        {"read", "Read user input into variables"},
        {"getopts", "Parse positional parameters as short options"},

        // Aliases and abbreviations
        {"alias", "Create or list command aliases"},
        {"unalias", "Remove command aliases"},
        {"abbr", "Create or list command abbreviations"},
        {"unabbr", "Remove command abbreviations"},

        // Command lookup and caching
        {"type", "Explain how a command name will be resolved"},
        {"which", "Locate executables in PATH"},
        {"hash", "Cache command lookups or display the cache"},
        {"generate-completions", "Regenerate cached completion metadata"},

        // History
        {"history", "Display command history"},
        {"fc", "Fix command - edit and re-execute commands from history"},

        // Job control
        {"jobs", "List background jobs"},
        {"jobname", "Assign a friendly display name to a job"},
        {"fg", "Bring a job to the foreground"},
        {"bg", "Resume a job in the background"},
        {"wait", "Wait for jobs or processes to finish"},
        {"kill", "Send signals to jobs or processes"},
        {"disown", "Detach jobs so they survive after cjsh exits"},

        // System
        {"umask", "Show or set the file creation mask"},
        {"ulimit", "Set or show resource limits"},
        {"trap", "Set signal handlers or list existing traps"},
        {"times", "Show CPU usage for the shell and its children"},

        // Flow control
        {"break", "Exit the current loop"},
        {"continue", "Skip to the next loop iteration"},
        {"return", "Exit the current function with an optional status"},

        // Testing and conditionals
        {"test / [", "Evaluate POSIX test expressions"},
        {":", "No-op command that always succeeds"},
        {"true", "Return success (exit code 0)"},
        {"false", "Return failure (exit code 1)"},

        // Shell customization
        {"cjshopt", "Generate config files and adjust cjsh options"},
        {"cjsh-widget", "Drive the line editor from shell code"},
        {"hook", "Manage shell hooks (precmd, preexec, chpwd)"}};

    if (cjsh_filesystem::is_first_boot()) {
        builtins.insert(builtins.begin() + 12,
                        {"firstboot", "Suppress the welcome banner by creating its marker"});
    }

    output << std::left;
    constexpr int column_width = 20;
    for (const auto& item : builtins) {
        output << "  " << std::setw(column_width) << item.name << item.description << "\n";
    }
    output << "\n  Note: Use '<command> --help' to see detailed usage.\n";

    heading("Shell scripting features");
    output << "  - POSIX-style functions with local variables and return codes.\n";
    output << "  - Conditionals with if/elif/else/fi plus test, [, and [[ expressions.\n";
    output << "  - Loop constructs (for/while/until) and loop controls (break/continue).\n";
    output << "  - Command substitution $(...), pipelines, redirection, and here-strings.\n";
    output << "  - Script tooling: source plus built-in inspection utilities like 'type',\n"
              "    'which', and 'hash' for verifying commands before execution.\n";

    heading("Startup and shutdown");
    output << "  Startup sequence:\n";
    output << "    1. ~/.cjshenv is sourced if present (or CJSH_ENV if set).\n";
    output << "    2. Login shells load ~/.cjprofile (if present).\n";
    output << "    3. Stored startup flags from 'cjshopt login-startup-arg' are applied.\n";
    output << "    4. Interactive mode initializes colors, completions, and sources ~/.cjshrc\n"
              "       unless disabled with --no-source or secure mode.\n";
    output << "  Shutdown sequence:\n";
    output << "    - Registered EXIT traps run before teardown.\n";
    output << "    - ~/.cjlogout is sourced for login shells (when it exists).\n";
    output << "    - History and themes are flushed before exit.\n";

    heading("Primary cjsh directories");
    output << "  ~/.cjshenv          Optional environment script sourced at startup.\n";
    output << "  ~/.cjprofile        Login configuration and persisted startup flags.\n";
    output << "  ~/.cjshrc           Interactive configuration (aliases, themes).\n";
    output << "  ~/.cjlogout         Optional logout script sourced on exit.\n";
    output << "  ~/.config/cjsh/     Optional alternate config root for generated files.\n";
    output << "  ~/.cache/cjsh/      Cache directory (history.txt, exec cache).\n";
    output << "  ~/.cache/cjsh/.first_boot  Marker used to suppress the first-run banner.\n";
    output << "  approot [target]    Jump directly to cjsh config/cache/history/"
              "firstboot/completion/bin dirs,\n"
              "                      or print one with approot --print/--file.\n";

    heading("cjsh invocation and startup flags");
    output << get_usage();

    heading("Isocline line editing");
    output << "  - cjsh embeds the isocline line editor for multiline input, highlighting,\n"
              "    and completion popups.\n";
    output << "  - Press <Tab> for context-aware completions and suggestions.\n";
    output << "  - Press F1 to open isocline's interactive cheat sheet of key bindings.\n";
    output << "  - Incremental history search (Ctrl+R) and other readline-style shortcuts are "
              "available.\n";
    output << "  - Configuration such as syntax colors can be adjusted via 'cjshopt style_def'.\n";

    output << "\n" << separator << "\n";
    return output.str();
}

int help_command() {
    std::cout << get_help();
    return 0;
}

/*
  usage.cpp

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

#include "usage.h"

#include <iostream>

#include "version_command.h"

void print_usage(bool print_version, bool print_hook, bool print_footer) {
    if (print_version) {
        (void)version_command({});
    }
    if (print_hook)
        std::cout << "POSIX shell scripting meets modern shell features\n";
    std::cout
        << "Usage: cjsh [options] [script_file [args...]]\n"
        << "       cjsh -c command_string [args...]\n"
        << "\n"
        << "\n"
        << "Options:\n"
        << "  -h, --help                 Display this help message and exit\n"
        << "  -v, --version              Print version information and exit\n"
        << "  -l, --login                Start as a login shell (load profile)\n"
        << "  -i, --interactive          Force interactive mode\n"
        << "  -c, --command=COMMAND      Execute the specified command and exit\n"
        << "                             (disables history expansion)\n"
        << "      --no-exec             Read commands but do not execute them\n"
        << "      --posix               Enable POSIX mode and reject non-POSIX syntax\n"
        << "\n"
        << "Feature Control Options:\n"
        << "  -m, --minimal              Disable cjsh extras (prompt themes,\n"
        << "                             colors, completions, syntax highlighting,\n"
        << "                             smart cd, rc sourcing, title line, history expansion,\n"
        << "                             multiline line numbers, auto indentation,\n"
        << "                             startup time banner, error suggestions, prompt vars)\n"
        << "      --no-prompt-vars      Ignore PS1/PS2 and use fixed prompts\n"
        << "  -C, --no-colors            Disable color output\n"
        << "  -N, --no-source            Don't source the ~/.cjshrc file\n"
        << "  -O, --no-completions       Disable tab completions\n"
        << "      --no-completion-learning Disable on-demand completion learning\n"
        << "      --no-smart-cd          Disable smart cd auto-jumps\n"
        << "      --no-script-extension-interpreter Disable extension-based script runners\n"
        << "  -S, --no-syntax-highlighting Disable syntax highlighting\n"
        << "      --no-error-suggestions Disable error suggestions\n"
        << "  -H, --no-history-expansion Disable history expansion (!commands)\n"
        << "  -W, --no-sh-warning       Suppress the sh invocation warning\n"
        << "\n"
        << "Display Options:\n"
        << "  -L, --no-titleline         Disable title line on startup\n"
        << "  -U, --show-startup-time    Display shell startup time\n"

        << "\n"
        << "Security and Testing:\n"
        << "  -s, --secure               Secure mode: skip ~/.cjprofile, ~/.cjshrc,\n"
        << "                             and ~/.cjsh_logout entirely\n"
        << "  -X, --startup-test         Enable startup test mode (internal)\n"
        << "\n"
        << "Persisting flags:\n"
        << "  Add 'cjshopt login-startup-arg <flag>' inside ~/.cjprofile to\n"
        << "  apply the same switches automatically on login shells.\n"
        << "\n"
        << "Examples:\n"
        << "  cjsh                       Start interactive shell\n"
        << "  cjsh script.sh arg1 arg2   Run script with arguments\n"
        << "  cjsh -c 'echo hello'       Execute command and exit\n"
        << "  cjsh -l                    Start login shell\n"
        << "  cjsh -m                    Start with minimal features\n"
        << "\n";
    if (print_footer)
        std::cout << "For more information:\n"
                  << "  Documentation: https://cadenfinley.github.io/CJsShell/\n"
                  << "  Repository:    https://github.com/CadenFinley/CJsShell\n"
                  << "  Run 'help' inside cjsh for built-in command reference\n";
}

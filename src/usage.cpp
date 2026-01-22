#include "usage.h"

#include <iostream>

#include "version_command.h"

void print_usage(bool print_version, bool print_hook, bool print_footer) {
    if (print_version) {
        (void)version_command({});
    }
    if (print_hook)
        std::cout << "POSIX shell scripting meets modern shell features\n";
    std::cout << "Usage: cjsh [options] [script_file [args...]]\n"
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
              << "\n"
              << "Feature Control Options:\n"
              << "  -m, --minimal              Disable cjsh extras (prompt themes,\n"
              << "                             colors, completions, syntax highlighting,\n"
              << "                             smart cd, rc sourcing, title line, history\n"
              << "                             expansion, multiline line numbers, auto\n"
              << "                             indentation, startup time banner)\n"
              << "  -C, --no-colors            Disable color output\n"
              << "  -N, --no-source            Don't source the ~/.cjshrc file\n"
              << "  -O, --no-completions       Disable tab completions\n"
              << "  -S, --no-syntax-highlighting Disable syntax highlighting\n"
              << "  -M, --no-smart-cd          Disable smart cd functionality\n"
              << "  -H, --no-history-expansion Disable history expansion (!commands)\n"
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

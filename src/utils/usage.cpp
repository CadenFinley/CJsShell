#include "usage.h"

#include <iostream>

#include "cjsh.h"

void print_usage() {
    std::cout << "Usage: cjsh [options]\n"
              << "CJ's Shell version " << c_version << "\n\n"
              << "Options:\n"
              << "  -l, --login                Start as a login shell\n"
              << "  -i, --interactive          Force interactive mode\n"
              << "  -d, --debug                Enable debug output\n"
              << "  -c, --command=COMMAND      Execute the specified command and "
                 "exit\n"
              << "  -v, --version              Print version information and exit\n"
              << "  -h, --help                 Display this help message and exit\n"
              << "  -m, --minimal              Disable all unique cjsh features "
                 "(plugins, themes, AI, colors, completions, syntax highlighting, "
                 "smart cd, sourcing, custom ls colors, startup time display)\n"
              << "  -P, --no-plugins           Disable plugin system\n"
              << "  -T, --no-themes            Disable theme system\n"
              << "  -A, --no-ai                Disable AI features\n"
              << "  -C, --no-colors            Disable color output\n"
              << "  -L, --no-titleline         Disable title line\n"
              << "  -N, --no-source            Don't source the .cjshrc file\n"
              << "  -O, --no-completions       Disable tab completions\n"
              << "  -S, --no-syntax-highlighting Disable syntax highlighting\n"
              << "  -M, --no-smart-cd          Disable smart cd functionality\n"
              << "  -D, --disable-custom-ls    Use system ls command instead of "
                 "builtin\n"
              << "  -U, --show-startup-time    Display shell startup time\n"
              << "  -X, --startup-test          Enable startup test mode\n"
              << "  -s, --secure               Disable reading profile and source "
                 "files\n\n"
              << "For more information, visit: "
                 "https://github.com/CadenFinley/CJsShell\n"
              << " Or run cjsh --help" << std::endl;
}

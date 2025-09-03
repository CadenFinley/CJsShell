#include "usage.h"

#include <iostream>

#include "cjsh.h"

void print_usage() {
  std::cout
      << "Usage: cjsh [options]\n"
      << "CJ's Shell version " << c_version << "\n\n"
      << "Options:\n"
      << "  -l, --login                Start as a login shell\n"
      << "  -i, --interactive          Force interactive mode\n"
      << "  -d, --debug                Enable debug output\n"
      << "  -c, --command=COMMAND      Execute the specified command and exit\n"
      << "  -v, --version              Print version information and exit\n"
      << "  -h, --help                 Display this help message and exit\n"
      << "  -s, --set-as-shell         Set cjsh as your default shell\n"
      << "  -u, --update               Check for updates and install if "
         "available\n"
      << "  -S, --silent-updates       Don't show update notifications\n"
      << "  -P, --no-plugins           Disable plugin system\n"
      << "  -T, --no-themes            Disable theme system\n"
      << "  -A, --no-ai                Disable AI features\n"
      << "  -C, --no-colors            Disable color output\n"
      << "  -U, --no-update            Disable update checks\n"
      << "  -V, --check-update         Enable update checks\n"
      << "  -L, --no-titleline         Disable title line\n"
      << "  -N, --no-source            Don't source the .cjshrc file\n\n"
      << "For more information, visit: " << c_github_url
      << " Or run cjsh --help" << std::endl;
}

#include "exit_command.h"

#include "main.h"


int exit_command(const std::vector<std::string>& args) {
    if (std::find(args.begin(), args.end(), "-f") != args.end() ||
        std::find(args.begin(), args.end(), "--force") != args.end()) {
        std::exit(0);  // exit immediately
    }

    g_exit_flag = true;
    return 0;
}
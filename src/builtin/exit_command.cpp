#include "exit_command.h"

#include "cjsh.h"
#include "trap_command.h"

int exit_command(const std::vector<std::string>& args) {
    int exit_code = 0;
    bool force_exit = false;

    force_exit = std::find(args.begin(), args.end(), "-f") != args.end() ||
                 std::find(args.begin(), args.end(), "--force") != args.end();

    for (size_t i = 1; i < args.size(); i++) {
        const std::string& val = args[i];
        if (val != "-f" && val != "--force") {
            char* endptr = nullptr;
            long code = std::strtol(val.c_str(), &endptr, 10);
            if (endptr && *endptr == '\0') {
                exit_code = static_cast<int>(code) & 0xFF;
                break;
            }
        }
    }

    if (force_exit) {
        cleanup_resources();
        std::exit(exit_code);
    }

    g_exit_flag = true;
    setenv("EXIT_CODE", std::to_string(exit_code).c_str(), 1);
    return 0;
}
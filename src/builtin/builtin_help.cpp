#include "builtin_help.h"

#include <iostream>

bool builtin_handle_help(const std::vector<std::string>& args,
                         const std::vector<std::string>& help_lines) {
    if (args.size() > 1) {
        const std::string& flag = args[1];
        if (flag == "--help" || flag == "-h") {
            for (const auto& line : help_lines) {
                std::cout << line << std::endl;
            }
            return true;
        }
    }
    return false;
}

#include "pwd_command.h"
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <system_error>

#include "error_out.h"

int pwd_command(const std::vector<std::string>& args) {
    bool logical = true;

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "-L") {
            logical = true;
        } else if (arg == "-P") {
            logical = false;
        } else if (arg == "--help") {
            std::cout << "Usage: pwd [-L | -P]\n";
            std::cout << "Print the current working directory.\n\n";
            std::cout << "Options:\n";
            std::cout << "  -L  print the logical current working directory "
                         "(default)\n";
            std::cout << "  -P  print the physical current working directory\n";
            return 0;
        } else {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "pwd",
                         "invalid option -- '" + arg + "'",
                         {"Try 'pwd --help' for more information"}});
            return 1;
        }
    }

    std::string path;

    if (logical) {
        const char* pwd_env = getenv("PWD");
        if ((pwd_env != nullptr) && pwd_env[0] == '/') {
            path = pwd_env;
        } else {
            std::unique_ptr<char, decltype(&free)> cwd(getcwd(nullptr, 0), free);
            if (cwd) {
                path = cwd.get();
            } else {
                const auto error_text = std::system_category().message(errno);
                print_error({ErrorType::RUNTIME_ERROR, "pwd", "getcwd failed: " + error_text, {}});
                return 1;
            }
        }
    } else {
        std::unique_ptr<char, decltype(&free)> cwd(getcwd(nullptr, 0), free);
        if (cwd) {
            path = cwd.get();
        } else {
            const auto error_text = std::system_category().message(errno);
            print_error({ErrorType::RUNTIME_ERROR, "pwd", "getcwd failed: " + error_text, {}});
            return 1;
        }
    }

    std::cout << path << '\n';
    std::cout.flush();
    return 0;
}

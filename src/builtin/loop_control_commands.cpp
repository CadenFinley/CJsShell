#include "loop_control_commands.h"

#include <iostream>
#include <string>
#include <vector>
#include "error_out.h"

int break_command(const std::vector<std::string>& args) {
    int level = 1;
    if (args.size() > 1) {
        try {
            level = std::stoi(args[1]);
            if (level < 1) {
                print_error(
                    {ErrorType::INVALID_ARGUMENT, "break", "invalid level: " + args[1], {}});
                return 1;
            }
        } catch (const std::exception&) {
            print_error({ErrorType::INVALID_ARGUMENT, "break", "invalid level: " + args[1], {}});
            return 1;
        }
    }

    setenv("CJSH_BREAK_LEVEL", std::to_string(level).c_str(), 1);

    return 255;
}

int continue_command(const std::vector<std::string>& args) {
    int level = 1;
    if (args.size() > 1) {
        try {
            level = std::stoi(args[1]);
            if (level < 1) {
                print_error(
                    {ErrorType::INVALID_ARGUMENT, "continue", "invalid level: " + args[1], {}});
                return 1;
            }
        } catch (const std::exception&) {
            print_error({ErrorType::INVALID_ARGUMENT, "continue", "invalid level: " + args[1], {}});
            return 1;
        }
    }

    setenv("CJSH_CONTINUE_LEVEL", std::to_string(level).c_str(), 1);

    return 254;
}

int return_command(const std::vector<std::string>& args) {
    int exit_code = 0;
    if (args.size() > 1) {
        try {
            exit_code = std::stoi(args[1]);

            if (exit_code < 0 || exit_code > 255) {
                print_error(
                    {ErrorType::INVALID_ARGUMENT, "return", "invalid exit code: " + args[1], {}});
                return 1;
            }
        } catch (const std::exception&) {
            print_error(
                {ErrorType::INVALID_ARGUMENT, "return", "invalid exit code: " + args[1], {}});
            return 1;
        }
    }

    setenv("CJSH_RETURN_CODE", std::to_string(exit_code).c_str(), 1);

    return 253;
}

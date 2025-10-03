#include "loop_control_commands.h"

#include "builtin_help.h"

#include <string>
#include <vector>
#include "error_out.h"

int break_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(
            args, {"Usage: break [N]", "Exit N levels of enclosing loops (default 1)."})) {
        return 0;
    }
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
    if (builtin_handle_help(
            args, {"Usage: continue [N]",
                   "Skip to the next iteration of the current loop or Nth enclosing loop."})) {
        return 0;
    }
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
    if (builtin_handle_help(
            args, {"Usage: return [N]",
                   "Exit a function with status N (default uses last command status)."})) {
        return 0;
    }
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

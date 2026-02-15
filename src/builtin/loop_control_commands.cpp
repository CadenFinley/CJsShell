/*
  loop_control_commands.cpp

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

#include "loop_control_commands.h"

#include "builtin_help.h"

#include <string>
#include <vector>
#include "error_out.h"
#include "shell_env.h"

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

    cjsh_env::set_shell_variable_value("CJSH_BREAK_LEVEL", std::to_string(level));

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

    cjsh_env::set_shell_variable_value("CJSH_CONTINUE_LEVEL", std::to_string(level));

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

    cjsh_env::set_shell_variable_value("CJSH_RETURN_CODE", std::to_string(exit_code));

    return 253;
}

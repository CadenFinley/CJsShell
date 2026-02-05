/*
  pushd_command.cpp

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

#include "pushd_command.h"

#include "builtin_help.h"
#include "cd_command.h"
#include "error_out.h"
#include "shell.h"

int pushd_command(const std::vector<std::string>& args, std::string& current_directory,
                  std::string& previous_directory, Shell* shell) {
    if (builtin_handle_help(args,
                            {"Usage: pushd [DIR]", "Push the current directory on a stack.",
                             "With no arguments, swap the current directory with the top of the "
                             "stack."})) {
        return 0;
    }

    if (args.size() > 2) {
        ErrorInfo error = {ErrorType::INVALID_ARGUMENT,
                           "pushd",
                           "too many arguments",
                           {"Usage: pushd [directory]"}};
        print_error(error);
        return 2;
    }

    if (!shell) {
        ErrorInfo error = {ErrorType::RUNTIME_ERROR, "pushd", "directory stack unavailable", {}};
        print_error(error);
        return 1;
    }

    auto& stack = shell->get_directory_stack();

    if (args.size() == 1) {
        if (stack.empty()) {
            ErrorInfo error = {ErrorType::RUNTIME_ERROR, "pushd", "directory stack empty", {}};
            print_error(error);
            return 1;
        }

        std::string target = stack.back();
        stack.back() = current_directory;
        return change_directory(target, current_directory, previous_directory, shell);
    }

    const std::string target = args[1];
    stack.push_back(current_directory);
    int status = change_directory(target, current_directory, previous_directory, shell);
    if (status != 0) {
        stack.pop_back();
    }
    return status;
}

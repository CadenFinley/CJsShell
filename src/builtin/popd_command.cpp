/*
  popd_command.cpp

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

#include "popd_command.h"

#include "builtin_help.h"
#include "cd_command.h"
#include "error_out.h"
#include "shell.h"

int popd_command(const std::vector<std::string>& args, std::string& current_directory,
                 std::string& previous_directory, Shell* shell) {
    if (builtin_handle_help(args, {"Usage: popd", "Pop the top entry from the directory stack."})) {
        return 0;
    }

    if (args.size() > 1) {
        ErrorInfo error = {
            ErrorType::INVALID_ARGUMENT, "popd", "too many arguments", {"Usage: popd"}};
        print_error(error);
        return 2;
    }

    if (!shell) {
        ErrorInfo error = {ErrorType::RUNTIME_ERROR, "popd", "directory stack unavailable", {}};
        print_error(error);
        return 1;
    }

    auto& stack = shell->get_directory_stack();
    if (stack.empty()) {
        ErrorInfo error = {ErrorType::RUNTIME_ERROR, "popd", "directory stack empty", {}};
        print_error(error);
        return 1;
    }

    std::string target = stack.back();
    stack.pop_back();
    int status = change_directory(target, current_directory, previous_directory, shell);
    if (status != 0) {
        stack.push_back(target);
    }
    return status;
}

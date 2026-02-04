/*
  dirs_command.cpp

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

#include "dirs_command.h"

#include <iostream>

#include "builtin_help.h"
#include "error_out.h"
#include "shell.h"

int dirs_command(const std::vector<std::string>& args, const std::string& current_directory,
                 Shell* shell) {
    if (builtin_handle_help(args, {"Usage: dirs", "Display the directory stack."})) {
        return 0;
    }

    if (args.size() > 1) {
        ErrorInfo error = {
            ErrorType::INVALID_ARGUMENT, "dirs", "too many arguments", {"Usage: dirs"}};
        print_error(error);
        return 2;
    }

    if (!shell) {
        ErrorInfo error = {ErrorType::RUNTIME_ERROR, "dirs", "directory stack unavailable", {}};
        print_error(error);
        return 1;
    }

    const auto& stack = shell->get_directory_stack();
    std::cout << current_directory;
    for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
        std::cout << ' ' << *it;
    }
    std::cout << '\n';
    return 0;
}

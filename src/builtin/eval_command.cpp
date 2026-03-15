/*
  eval_command.cpp

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

#include "eval_command.h"

#include "builtin_help.h"

#include "cjsh.h"
#include "error_out.h"
#include "shell.h"

int eval_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(
            args, {"Usage: eval STRING", "Evaluate STRING in the current shell context."})) {
        return 0;
    }
    if (args.size() < 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "eval", "missing arguments", {}});
        return 1;
    }

    std::string command_to_eval;
    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1) {
            command_to_eval += " ";
        }
        command_to_eval += args[i];
    }
    if (shell) {
        int result = shell->execute(command_to_eval);
        return result;
    } else {
        print_error({ErrorType::FATAL_ERROR, "eval", "shell not initialized properly", {}});
        return 1;
    }
}

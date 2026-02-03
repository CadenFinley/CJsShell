/*
  if_command.cpp

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

#include "if_command.h"

#include "builtin_help.h"

#include "error_out.h"
#include "shell.h"

int if_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(args, {"Usage: if CONDITION; then COMMAND; fi",
                                   "Evaluate CONDITION and run COMMAND when it succeeds.",
                                   "Supports standard cjsh command syntax."})) {
        return 0;
    }
    auto record_error = [&](const ErrorInfo& info) { print_error(info); };

    if (args.size() < 2) {
        record_error({ErrorType::INVALID_ARGUMENT, "if", "missing arguments", {}});
        return 2;
    }

    std::string full_cmd;
    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1)
            full_cmd += " ";
        full_cmd += args[i];
    }

    size_t then_pos = full_cmd.find("; then ");
    size_t fi_pos = full_cmd.rfind("; fi");

    if (then_pos == std::string::npos || fi_pos == std::string::npos) {
        record_error(
            {ErrorType::SYNTAX_ERROR, "if", "syntax error: expected '; then' and '; fi'", {}});
        return 2;
    }

    std::string condition = full_cmd.substr(0, then_pos);
    std::string then_cmd = full_cmd.substr(then_pos + 7, fi_pos - (then_pos + 7));

    if (!shell) {
        record_error({ErrorType::RUNTIME_ERROR, "if", "shell context is null", {}});
        return 1;
    }

    int cond_result = shell->execute(condition);

    if (cond_result == 0) {
        return shell->execute(then_cmd);
    }

    return 0;
}

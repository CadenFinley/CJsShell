/*
  local_command.cpp

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

#include "local_command.h"

#include "builtin_help.h"

#include "cjsh.h"
#include "error_out.h"
#include "interpreter.h"
#include "shell.h"
#include "shell_env.h"

int local_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(args, {"Usage: local NAME[=VALUE] ...",
                                   "Define local variables within a function scope."})) {
        return 0;
    }

    if (config::posix_mode) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "local",
                     "'local' is disabled in POSIX mode",
                     {"Declare variables without 'local'"}});
        return 1;
    }

    auto* script_interpreter = shell->get_shell_script_interpreter();
    if (script_interpreter == nullptr || !script_interpreter->in_function_scope()) {
        print_error({ErrorType::RUNTIME_ERROR, "local", "not available outside of functions", {}});
        return 1;
    }

    if (args.size() == 1) {
        return 0;
    }

    bool all_successful = true;

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];

        size_t eq_pos = arg.find('=');
        if (eq_pos != std::string::npos) {
            std::string name = arg.substr(0, eq_pos);
            std::string value = arg.substr(eq_pos + 1);

            if (name.empty()) {
                print_error({ErrorType::INVALID_ARGUMENT, "local", "invalid variable name", {}});
                all_successful = false;
                continue;
            }

            script_interpreter->set_local_variable(name, value);
        } else {
            const std::string& name = arg;

            if (name.empty()) {
                print_error({ErrorType::INVALID_ARGUMENT, "local", "invalid variable name", {}});
                all_successful = false;
                continue;
            }

            script_interpreter->set_local_variable(name, "");
        }
    }

    return all_successful ? 0 : 1;
}

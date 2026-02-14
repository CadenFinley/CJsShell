/*
  readonly_command.cpp

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

#include "readonly_command.h"

#include "builtin_help.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <unordered_set>
#include "cjsh.h"
#include "error_out.h"
#include "interpreter.h"
#include "shell.h"
#include "shell_env.h"

namespace {
struct ReadonlyState {
    std::unordered_set<std::string> readonly_vars;
};

struct ReadonlyFunctionState {
    std::unordered_set<std::string> readonly_functions;
};

bool is_builtin_readonly_var(const std::string& name) {
    if (name.size() == 1 && std::isdigit(static_cast<unsigned char>(name[0])) != 0) {
        return true;
    }

    static const std::unordered_set<std::string> kBuiltinReadonlyVars = {
        "?", "$", "#", "*", "@", "!", "PIPESTATUS", "CJSH_VERSION", "EXIT_CODE"};
    return kBuiltinReadonlyVars.find(name) != kBuiltinReadonlyVars.end();
}

ReadonlyState& readonly_state() {
    static ReadonlyState state;
    return state;
}

ReadonlyFunctionState& readonly_function_state() {
    static ReadonlyFunctionState state;
    return state;
}
}  // namespace

void readonly_manager_set(const std::string& name) {
    readonly_state().readonly_vars.insert(name);
}

bool readonly_manager_is(const std::string& name) {
    if (is_builtin_readonly_var(name)) {
        return true;
    }
    const auto& vars = readonly_state().readonly_vars;
    return vars.find(name) != vars.end();
}

std::vector<std::string> readonly_manager_list() {
    const auto& vars = readonly_state().readonly_vars;
    std::vector<std::string> result(vars.begin(), vars.end());
    std::sort(result.begin(), result.end());
    return result;
}

void readonly_function_manager_set(const std::string& name) {
    readonly_function_state().readonly_functions.insert(name);
}

bool readonly_function_manager_is(const std::string& name) {
    const auto& funcs = readonly_function_state().readonly_functions;
    return funcs.find(name) != funcs.end();
}

std::vector<std::string> readonly_function_manager_list() {
    const auto& funcs = readonly_function_state().readonly_functions;
    std::vector<std::string> result(funcs.begin(), funcs.end());
    std::sort(result.begin(), result.end());
    return result;
}

int readonly_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args, {"Usage: readonly [-p] NAME[=VALUE] ...",
                                   "Mark shell variables as readonly and optionally assign values.",
                                   "-p prints readonly variables.",
                                   "-f operates on shell functions instead of variables."})) {
        return 0;
    }
    if (args.size() == 1) {
        auto readonly_vars = readonly_manager_list();

        for (const std::string& var : readonly_vars) {
            if (cjsh_env::shell_variable_is_set(var)) {
                std::cout << "readonly " << var << "=" << cjsh_env::get_shell_variable_value(var)
                          << '\n';
            } else {
                std::cout << "readonly " << var << '\n';
            }
        }
        return 0;
    }

    bool print_mode = false;
    bool function_mode = false;
    size_t start_index = 1;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-p") {
            print_mode = true;
            start_index = i + 1;
        } else if (args[i] == "-f") {
            function_mode = true;
            start_index = i + 1;
        } else if (args[i].substr(0, 1) == "-") {
            print_error(
                {ErrorType::INVALID_ARGUMENT, "readonly", args[i] + ": invalid option", {}});
            return 2;
        } else {
            break;
        }
    }

    if (function_mode) {
        if (print_mode || start_index >= args.size()) {
            auto readonly_funcs = readonly_function_manager_list();
            for (const std::string& func : readonly_funcs) {
                std::cout << "readonly -f " << func << '\n';
            }
            return 0;
        }

        if (!g_shell || !g_shell->get_shell_script_interpreter()) {
            print_error(
                {ErrorType::RUNTIME_ERROR, "readonly", "shell interpreter not available", {}});
            return 1;
        }

        auto* interpreter = g_shell->get_shell_script_interpreter();
        for (size_t i = start_index; i < args.size(); ++i) {
            const std::string& func_name = args[i];
            if (!interpreter->has_function(func_name)) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             "readonly",
                             func_name + ": function not found",
                             {}});
                return 1;
            }
            readonly_function_manager_set(func_name);
        }
        return 0;
    }

    if (print_mode) {
        auto readonly_vars = readonly_manager_list();

        for (const std::string& var : readonly_vars) {
            if (cjsh_env::shell_variable_is_set(var)) {
                std::cout << "readonly " << var << "='" << cjsh_env::get_shell_variable_value(var)
                          << "'" << '\n';
            } else {
                std::cout << "readonly " << var << '\n';
            }
        }
        return 0;
    }

    for (size_t i = start_index; i < args.size(); ++i) {
        const std::string& arg = args[i];

        size_t eq_pos = arg.find('=');
        if (eq_pos != std::string::npos) {
            std::string name = arg.substr(0, eq_pos);
            std::string value = arg.substr(eq_pos + 1);

            if (readonly_manager_is(name)) {
                print_error(
                    {ErrorType::RUNTIME_ERROR, "readonly", name + ": readonly variable", {}});
                return 1;
            }

            if (setenv(name.c_str(), value.c_str(), 1) != 0) {
                print_error({ErrorType::RUNTIME_ERROR,
                             "readonly",
                             "setenv failed: " + std::string(strerror(errno)),
                             {}});
                return 1;
            }

            readonly_manager_set(name);
        } else {
            if (!cjsh_env::shell_variable_is_set(arg)) {
                if (setenv(arg.c_str(), "", 1) != 0) {
                    print_error({ErrorType::RUNTIME_ERROR,
                                 "readonly",
                                 "setenv failed: " + std::string(strerror(errno)),
                                 {}});
                    return 1;
                }
            }

            readonly_manager_set(arg);
        }
    }

    return 0;
}

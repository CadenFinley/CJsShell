/*
  hook_command.cpp

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

#include "hook_command.h"

#include <algorithm>
#include <array>
#include <iostream>

#include "builtin_help.h"
#include "error_out.h"
#include "shell.h"

namespace {

constexpr std::array<std::string_view, 3> kValidHookTypes = {"precmd", "preexec", "chpwd"};

bool is_valid_hook_type(const std::string& hook_type) {
    return std::any_of(kValidHookTypes.begin(), kValidHookTypes.end(),
                       [&hook_type](std::string_view valid) { return hook_type == valid; });
}

}  // namespace

int hook_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(
            args, {"Usage: hook add|remove|list|clear [hook_type] [function_name]",
                   "Manage shell hooks for custom behavior.", "",
                   "Hook types:", "  precmd   - Run before displaying the prompt",
                   "  preexec  - Run before executing a command",
                   "  chpwd    - Run after changing directory", "",
                   "Commands:", "  hook add <type> <function>    - Register a function as a hook",
                   "  hook remove <type> <function> - Unregister a function",
                   "  hook list [type]              - List registered hooks",
                   "  hook clear <type>             - Clear all hooks of a type", "",
                   "Example:", "  function my_precmd() { echo \"Ready for command\"; }",
                   "  hook add precmd my_precmd"})) {
        return 0;
    }

    if (shell == nullptr) {
        ErrorInfo error = {ErrorType::RUNTIME_ERROR, "hook", "Shell context not available", {}};
        print_error(error);
        return 1;
    }

    if (args.size() < 2) {
        ErrorInfo error = {ErrorType::INVALID_ARGUMENT,
                           "hook",
                           "missing command",
                           {"Usage: hook add|remove|list|clear [hook_type] [function_name]"}};
        print_error(error);
        return 1;
    }

    const std::string& command = args[1];

    if (command == "list") {
        if (args.size() == 2) {
            bool found_any = false;
            for (std::string_view hook_type_view : kValidHookTypes) {
                std::string hook_type(hook_type_view);
                auto hooks = shell->get_hooks(hook_type);
                if (!hooks.empty()) {
                    found_any = true;
                    std::cout << hook_type << ":\n";
                    for (const auto& func : hooks) {
                        std::cout << "  " << func << "\n";
                    }
                }
            }
            if (!found_any) {
                std::cout << "No hooks registered.\n";
            }
            return 0;
        }

        const std::string& hook_type = args[2];
        if (!is_valid_hook_type(hook_type)) {
            ErrorInfo error = {ErrorType::INVALID_ARGUMENT,
                               "hook",
                               "invalid hook type '" + hook_type + "'",
                               {"Valid hook types: precmd, preexec, chpwd"}};
            print_error(error);
            return 1;
        }

        auto hooks = shell->get_hooks(hook_type);
        if (hooks.empty()) {
            std::cout << "No " << hook_type << " hooks registered.\n";
        } else {
            std::cout << hook_type << ":\n";
            for (const auto& func : hooks) {
                std::cout << "  " << func << "\n";
            }
        }
        return 0;
    }

    if (command == "clear") {
        if (args.size() < 3) {
            ErrorInfo error = {ErrorType::INVALID_ARGUMENT,
                               "hook",
                               "missing hook type for clear command",
                               {"Usage: hook clear <hook_type>"}};
            print_error(error);
            return 1;
        }

        const std::string& hook_type = args[2];
        if (!is_valid_hook_type(hook_type)) {
            ErrorInfo error = {ErrorType::INVALID_ARGUMENT,
                               "hook",
                               "invalid hook type '" + hook_type + "'",
                               {"Valid hook types: precmd, preexec, chpwd"}};
            print_error(error);
            return 1;
        }

        shell->clear_hooks(hook_type);
        return 0;
    }

    if (command == "add") {
        if (args.size() < 4) {
            ErrorInfo error = {ErrorType::INVALID_ARGUMENT,
                               "hook",
                               "missing arguments for add command",
                               {"Usage: hook add <hook_type> <function_name>"}};
            print_error(error);
            return 1;
        }

        const std::string& hook_type = args[2];
        const std::string& function_name = args[3];

        if (!is_valid_hook_type(hook_type)) {
            ErrorInfo error = {ErrorType::INVALID_ARGUMENT,
                               "hook",
                               "invalid hook type '" + hook_type + "'",
                               {"Valid hook types: precmd, preexec, chpwd"}};
            print_error(error);
            return 1;
        }

        shell->register_hook(hook_type, function_name);
        return 0;
    }

    if (command == "remove") {
        if (args.size() < 4) {
            ErrorInfo error = {ErrorType::INVALID_ARGUMENT,
                               "hook",
                               "missing arguments for remove command",
                               {"Usage: hook remove <hook_type> <function_name>"}};
            print_error(error);
            return 1;
        }

        const std::string& hook_type = args[2];
        const std::string& function_name = args[3];

        if (!is_valid_hook_type(hook_type)) {
            ErrorInfo error = {ErrorType::INVALID_ARGUMENT,
                               "hook",
                               "invalid hook type '" + hook_type + "'",
                               {"Valid hook types: precmd, preexec, chpwd"}};
            print_error(error);
            return 1;
        }

        shell->unregister_hook(hook_type, function_name);
        return 0;
    }

    ErrorInfo error = {ErrorType::INVALID_ARGUMENT,
                       "hook",
                       "unknown command '" + command + "'",
                       {"Valid commands: add, remove, list, clear"}};
    print_error(error);
    return 1;
}

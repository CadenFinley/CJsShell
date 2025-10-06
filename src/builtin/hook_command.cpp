#include "hook_command.h"

#include <algorithm>
#include <iostream>

#include "builtin_help.h"
#include "error_out.h"
#include "shell.h"

static const std::vector<std::string> g_valid_hook_types = {"precmd", "preexec", "chpwd"};

static bool is_valid_hook_type(const std::string& hook_type) {
    return std::any_of(g_valid_hook_types.begin(), g_valid_hook_types.end(),
                       [&hook_type](const std::string& valid) { return hook_type == valid; });
}

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

    // hook list [type]
    if (command == "list") {
        if (args.size() == 2) {
            // List all hooks
            bool found_any = false;
            for (const auto& hook_type : g_valid_hook_types) {
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

        // List specific hook type
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

    // hook clear <type>
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

    // hook add <type> <function>
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

    // hook remove <type> <function>
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

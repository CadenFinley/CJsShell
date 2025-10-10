#include "export_command.h"

#include "builtin_help.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <vector>

#include "cjsh.h"
#include "error_out.h"
#include "readonly_command.h"
#include "shell.h"
#include "shell_script_interpreter.h"

int export_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(args, {"Usage: export [NAME[=VALUE] ...]",
                                   "Set environment variables for the shell and subprocesses.",
                                   "Without operands, list exported variables."})) {
        return 0;
    }
    if (args.size() == 1) {
        extern char** environ;
        for (char** env = environ; *env != nullptr; ++env) {
            std::cout << "export " << *env << '\n';
        }
        return 0;
    }

    bool all_successful = true;
    auto& env_vars = shell->get_env_vars();

    for (size_t i = 1; i < args.size(); ++i) {
        std::string name;
        std::string value;
        if (parse_env_assignment(args[i], name, value)) {
            if (readonly_manager_is(name)) {
                print_error(
                    {ErrorType::INVALID_ARGUMENT, "export", name + ": readonly variable", {}});
                all_successful = false;
                continue;
            }

            if (shell != nullptr) {
                shell->expand_env_vars(value);
            }

            env_vars[name] = value;

            setenv(name.c_str(), value.c_str(), 1);

            if ((shell != nullptr) && (shell->get_parser() != nullptr)) {
                shell->get_parser()->set_env_vars(env_vars);
            }
        } else {
            // No assignment, export existing variable
            std::string var_value;
            bool found = false;
            bool is_local = false;

            // First check if it's a local variable
            auto* script_interpreter = shell->get_shell_script_interpreter();
            if (script_interpreter != nullptr && script_interpreter->is_local_variable(args[i])) {
                // Get the local variable value and export it
                var_value = script_interpreter->get_variable_value(args[i]);
                found = true;
                is_local = true;

                // Mark it as exported so it can be cleaned up when scope ends
                script_interpreter->mark_local_as_exported(args[i]);
            } else {
                // Check environment variables
                const char* env_val = getenv(args[i].c_str());
                if (env_val != nullptr) {
                    var_value = env_val;
                    found = true;
                } else {
                    // Check shell variables
                    auto it = env_vars.find(args[i]);
                    if (it != env_vars.end()) {
                        var_value = it->second;
                        found = true;
                    }
                }
            }

            if (found) {
                // Only add to env_vars if it's not a local variable
                // (locals should only exist in the environment temporarily)
                if (!is_local) {
                    env_vars[args[i]] = var_value;
                }
                setenv(args[i].c_str(), var_value.c_str(), 1);
            } else {
                print_error({ErrorType::INVALID_ARGUMENT, "export", args[i] + ": not found", {}});
                all_successful = false;
            }
        }
    }

    if (shell != nullptr) {
        shell->set_env_vars(env_vars);
    }

    return all_successful ? 0 : 1;
}

int unset_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(args, {"Usage: unset NAME [NAME ...]",
                                   "Remove variables from the environment and shell state."})) {
        return 0;
    }
    if (args.size() < 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "unset", "not enough arguments", {}});
        return 1;
    }

    bool success = true;
    auto& env_vars = shell->get_env_vars();

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& name = args[i];

        if (readonly_manager_is(name)) {
            print_error({ErrorType::INVALID_ARGUMENT, "unset", name + ": readonly variable", {}});
            success = false;
            continue;
        }

        // First, try to unset local variable if we're in a function
        auto* script_interpreter = shell->get_shell_script_interpreter();
        if (script_interpreter != nullptr && script_interpreter->is_local_variable(name)) {
            script_interpreter->unset_local_variable(name);
            continue;
        }

        // Otherwise, unset environment variable
        env_vars.erase(name);

        if (unsetenv(name.c_str()) != 0) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "unset",
                         std::string("error unsetting ") + name + ": " + strerror(errno),
                         {}});
            success = false;
        }
    }

    if (shell != nullptr) {
        shell->set_env_vars(env_vars);
    }

    return success ? 0 : 1;
}

bool parse_env_assignment(const std::string& arg, std::string& name, std::string& value) {
    size_t equals_pos = arg.find('=');
    if (equals_pos == std::string::npos || equals_pos == 0) {
        return false;
    }

    name = arg.substr(0, equals_pos);
    value = arg.substr(equals_pos + 1);

    if (value.size() >= 2) {
        if ((value.front() == '"' && value.back() == '"') ||
            (value.front() == '\'' && value.back() == '\'')) {
            value = value.substr(1, value.size() - 2);
        }
    }

    return true;
}

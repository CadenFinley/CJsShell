#include "export_command.h"

#include "builtin_help.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "readonly_command.h"
#include "shell.h"

int export_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(args, {"Usage: export [NAME[=VALUE] ...]",
                                   "Set environment variables for the shell and subprocesses.",
                                   "Without operands, list exported variables."})) {
        return 0;
    }
    if (args.size() == 1) {
        extern char** environ;
        for (char** env = environ; *env; ++env) {
            std::cout << "export " << *env << std::endl;
        }
        return 0;
    }

    bool all_successful = true;
    auto& env_vars = shell->get_env_vars();

    for (size_t i = 1; i < args.size(); ++i) {
        std::string name, value;
        if (parse_env_assignment(args[i], name, value)) {
            if (ReadonlyManager::instance().is_readonly(name)) {
                print_error(
                    {ErrorType::INVALID_ARGUMENT, "export", name + ": readonly variable", {}});
                all_successful = false;
                continue;
            }

            if (shell) {
                shell->expand_env_vars(value);
            }

            env_vars[name] = value;

            setenv(name.c_str(), value.c_str(), 1);

            if (shell && shell->get_parser()) {
                shell->get_parser()->set_env_vars(env_vars);
            }
        } else {
            const char* env_val = getenv(args[i].c_str());
            if (env_val) {
                env_vars[args[i]] = env_val;
            } else {
                print_error({ErrorType::INVALID_ARGUMENT, "export", args[i] + ": not found", {}});
                all_successful = false;
            }
        }
    }

    if (shell) {
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

        if (ReadonlyManager::instance().is_readonly(name)) {
            print_error({ErrorType::INVALID_ARGUMENT, "unset", name + ": readonly variable", {}});
            success = false;
            continue;
        }

        env_vars.erase(name);

        if (unsetenv(name.c_str()) != 0) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "unset",
                         std::string("error unsetting ") + name + ": " + strerror(errno),
                         {}});
            success = false;
        }
    }

    if (shell) {
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
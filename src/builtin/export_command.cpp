/*
  export_command.cpp

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

#include "export_command.h"

#include "builtin_help.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <vector>

#include "cjsh.h"
#include "error_out.h"
#include "interpreter.h"
#include "parser_utils.h"
#include "readonly_command.h"
#include "shell.h"
#include "shell_env.h"

namespace {

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

bool is_special_parameter_name(const std::string& name) {
    if (name.size() != 2 || name[0] != '$') {
        return false;
    }

    const char param = name[1];
    if (std::isdigit(static_cast<unsigned char>(param)) != 0) {
        return true;
    }

    return param == '?' || param == '$' || param == '#' || param == '*' || param == '@' ||
           param == '!';
}

bool validate_export_name(const std::string& name, bool& is_readonly) {
    is_readonly = false;
    if (name.empty()) {
        return false;
    }

    if (name[0] == '$') {
        if (is_special_parameter_name(name)) {
            is_readonly = true;
        }
        return false;
    }

    if (!is_valid_identifier(name)) {
        return false;
    }

    if (readonly_manager_is(name)) {
        is_readonly = true;
        return false;
    }

    return true;
}

}  // namespace

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
    auto& env_vars = cjsh_env::env_vars();

    for (size_t i = 1; i < args.size(); ++i) {
        std::string name;
        std::string value;
        if (parse_env_assignment(args[i], name, value)) {
            bool is_readonly = false;
            if (!validate_export_name(name, is_readonly)) {
                if (is_readonly) {
                    print_error(
                        {ErrorType::INVALID_ARGUMENT, "export", name + ": readonly variable", {}});
                } else {
                    print_error(
                        {ErrorType::INVALID_ARGUMENT, "export", name + ": invalid identifier", {}});
                }
                all_successful = false;
                continue;
            }

            if (shell != nullptr) {
                if (auto* parser = shell->get_parser()) {
                    parser->expand_env_vars(value);
                }
            }

            env_vars[name] = value;

            setenv(name.c_str(), value.c_str(), 1);

            if ((shell != nullptr) && (shell->get_parser() != nullptr)) {
                shell->get_parser()->set_env_vars(env_vars);
            }
        } else {
            bool is_readonly = false;
            if (!validate_export_name(args[i], is_readonly)) {
                if (is_readonly) {
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 "export",
                                 args[i] + ": readonly variable",
                                 {}});
                } else {
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 "export",
                                 args[i] + ": invalid identifier",
                                 {}});
                }
                all_successful = false;
                continue;
            }

            std::string var_value;
            bool found = false;
            bool is_local = false;

            auto* script_interpreter = shell->get_shell_script_interpreter();
            if (script_interpreter != nullptr && script_interpreter->is_local_variable(args[i])) {
                var_value = script_interpreter->get_variable_value(args[i]);
                found = true;
                is_local = true;

                script_interpreter->mark_local_as_exported(args[i]);
            } else {
                if (cjsh_env::shell_variable_is_set(args[i])) {
                    var_value = cjsh_env::get_shell_variable_value(args[i]);
                    found = true;
                } else {
                    auto it = env_vars.find(args[i]);
                    if (it != env_vars.end()) {
                        var_value = it->second;
                        found = true;
                    }
                }
            }

            if (found) {
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
        cjsh_env::sync_parser_env_vars(shell);
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
    auto& env_vars = cjsh_env::env_vars();

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& name = args[i];

        if (!cjsh_env::is_valid_env_name(name)) {
            print_error({ErrorType::INVALID_ARGUMENT, "unset", "invalid name: " + name, {}});
            success = false;
            continue;
        }

        if (readonly_manager_is(name)) {
            print_error({ErrorType::INVALID_ARGUMENT, "unset", name + ": readonly variable", {}});
            success = false;
            continue;
        }

        auto* script_interpreter = shell->get_shell_script_interpreter();
        if (script_interpreter != nullptr && script_interpreter->is_local_variable(name)) {
            script_interpreter->unset_local_variable(name);
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

    if (shell != nullptr) {
        cjsh_env::sync_parser_env_vars(shell);
    }

    return success ? 0 : 1;
}

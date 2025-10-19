#include "readonly_command.h"

#include "builtin_help.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <unordered_set>
#include "error_out.h"
#include "shell.h"

namespace {
struct ReadonlyState {
    std::unordered_set<std::string> readonly_vars;
};

ReadonlyState& readonly_state() {
    static ReadonlyState state;
    return state;
}
}  

void readonly_manager_set(const std::string& name) {
    readonly_state().readonly_vars.insert(name);
}

bool readonly_manager_is(const std::string& name) {
    const auto& vars = readonly_state().readonly_vars;
    return vars.find(name) != vars.end();
}

void readonly_manager_remove(const std::string& name) {
    readonly_state().readonly_vars.erase(name);
}

std::vector<std::string> readonly_manager_list() {
    const auto& vars = readonly_state().readonly_vars;
    std::vector<std::string> result(vars.begin(), vars.end());
    std::sort(result.begin(), result.end());
    return result;
}

void readonly_manager_clear() {
    readonly_state().readonly_vars.clear();
}

int readonly_command(const std::vector<std::string>& args, Shell* shell) {
    (void)shell;
    if (builtin_handle_help(args, {"Usage: readonly [-p] NAME[=VALUE] ...",
                                   "Mark shell variables as readonly and optionally assign values.",
                                   "-p prints readonly variables."})) {
        return 0;
    }
    if (args.size() == 1) {
        auto readonly_vars = readonly_manager_list();

        for (const std::string& var : readonly_vars) {
            const char* value = getenv(var.c_str());
            if (value != nullptr) {
                std::cout << "readonly " << var << "=" << value << '\n';
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

    if (print_mode) {
        auto readonly_vars = readonly_manager_list();

        for (const std::string& var : readonly_vars) {
            const char* value = getenv(var.c_str());
            if (value != nullptr) {
                std::cout << "readonly " << var << "='" << value << "'" << '\n';
            } else {
                std::cout << "readonly " << var << '\n';
            }
        }
        return 0;
    }

    if (function_mode) {
        print_error({ErrorType::RUNTIME_ERROR, "readonly", "-f option not implemented", {}});
        return 1;
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
            const char* value = getenv(arg.c_str());
            if (value == nullptr) {
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

bool check_readonly_assignment(const std::string& name) {
    return readonly_manager_is(name);
}

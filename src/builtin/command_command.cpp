#include "command_command.h"

#include "builtin_help.h"

#include <cstdlib>
#include <iostream>

#include "builtin.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "shell.h"

int command_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(args, {"Usage: command [-pVv] COMMAND [ARG ...]",
                                   "Execute COMMAND with arguments, bypassing shell functions.", "",
                                   "Options:", "  -p    Use a default PATH value",
                                   "  -v    Print a description of COMMAND (similar to type)",
                                   "  -V    Print a more verbose description of COMMAND"})) {
        return 0;
    }

    if (args.size() < 2) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "command",
                     "usage: command [-pVv] command [arg ...]",
                     {}});
        return 2;
    }

    bool use_default_path = false;
    bool describe_command = false;
    bool verbose_description = false;
    size_t start_index = 1;

    for (size_t i = 1; i < args.size() && args[i][0] == '-' && args[i].length() > 1; ++i) {
        const std::string& option = args[i];

        if (option == "--") {
            start_index = i + 1;
            break;
        }

        for (size_t j = 1; j < option.length(); ++j) {
            switch (option[j]) {
                case 'p':
                    use_default_path = true;
                    break;
                case 'v':
                    describe_command = true;
                    break;
                case 'V':
                    verbose_description = true;
                    break;
                default:
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 "command",
                                 "invalid option: -" + std::string(1, option[j]),
                                 {}});
                    return 2;
            }
        }
        start_index = i + 1;
    }

    if (start_index >= args.size()) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "command",
                     "usage: command [-pVv] command [arg ...]",
                     {}});
        return 2;
    }

    const std::string& command_name = args[start_index];

    if (describe_command || verbose_description) {
        if (shell != nullptr && shell->get_built_ins()->is_builtin_command(command_name) != 0) {
            if (verbose_description) {
                std::cout << command_name << " is a shell builtin\n";
            } else {
                std::cout << command_name << "\n";
            }
            return 0;
        }

        std::string saved_path;
        if (use_default_path) {
            const char* current_path = std::getenv("PATH");
            if (current_path != nullptr) {
                saved_path = current_path;
            }

            setenv("PATH", "/usr/bin:/bin", 1);
        }

        std::string full_path = cjsh_filesystem::find_executable_in_path(command_name);

        if (use_default_path && !saved_path.empty()) {
            setenv("PATH", saved_path.c_str(), 1);
        }

        if (!full_path.empty()) {
            if (verbose_description) {
                std::cout << command_name << " is " << full_path << "\n";
            } else {
                std::cout << full_path << "\n";
            }
            return 0;
        }

        if (verbose_description) {
            std::cout << command_name << ": not found\n";
        }
        return 1;
    }

    if (shell == nullptr) {
        print_error({ErrorType::RUNTIME_ERROR, "command", "shell context not available", {}});
        return 2;
    }

    std::vector<std::string> exec_args(args.begin() + static_cast<long>(start_index), args.end());

    std::string saved_path;
    if (use_default_path) {
        const char* current_path = std::getenv("PATH");
        if (current_path != nullptr) {
            saved_path = current_path;
        }

        setenv("PATH", "/usr/bin:/bin", 1);
    }

    int exit_code = shell->execute_command(exec_args, false);

    if (use_default_path) {
        if (!saved_path.empty()) {
            setenv("PATH", saved_path.c_str(), 1);
        } else {
            unsetenv("PATH");
        }
    }

    return exit_code;
}

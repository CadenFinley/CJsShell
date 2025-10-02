#include "which_command.h"
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <iostream>
#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "shell.h"
#include "shell_script_interpreter.h"

int which_command(const std::vector<std::string>& args, Shell* shell) {
    if (args.size() < 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "which", "usage: which [-as] name [name ...]", {}});
        return 1;
    }

    bool show_all = false;
    bool silent = false;

    size_t start_index = 1;

    for (size_t i = 1; i < args.size() && args[i][0] == '-'; ++i) {
        const std::string& option = args[i];
        if (option == "--") {
            start_index = i + 1;
            break;
        }

        for (size_t j = 1; j < option.length(); ++j) {
            switch (option[j]) {
                case 'a':
                    show_all = true;
                    break;
                case 's':
                    silent = true;
                    break;
                default:
                    print_error({ErrorType::INVALID_ARGUMENT, "which", "invalid option: -" + std::string(1, option[j]), {}});
                    return 1;
            }
        }
        start_index = i + 1;
    }

    int return_code = 0;

    for (size_t i = start_index; i < args.size(); ++i) {
        const std::string& name = args[i];
        bool found = false;
        bool found_executable = false;

        const std::vector<std::string> cjsh_custom_commands = {"echo", "printf", "pwd", "cd", "ls"};

        bool is_cjsh_custom = std::find(cjsh_custom_commands.begin(), cjsh_custom_commands.end(), name) != cjsh_custom_commands.end();

        if (name == "ls" && config::disable_custom_ls) {
            is_cjsh_custom = false;
        }

        if (is_cjsh_custom && shell && shell->get_built_ins()->is_builtin_command(name)) {
            if (!silent) {
                std::cout << name << " is a cjsh builtin (custom implementation)" << std::endl;
            }
            found = true;
            if (!show_all) {
                continue;
            }
        }

        std::string path = cjsh_filesystem::find_executable_in_path(name);
        if (!path.empty()) {
            if (!silent) {
                std::cout << path << std::endl;
            }
            found = true;
            found_executable = true;
            if (!show_all && !is_cjsh_custom) {
                continue;
            }
        }

        if (!found_executable && (name.find('/') != std::string::npos)) {
            struct stat st;
            if (stat(name.c_str(), &st) == 0 && (st.st_mode & S_IXUSR)) {
                if (!silent) {
                    if (name[0] != '/') {
                        char cwd[PATH_MAX];
                        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
                            std::cout << cwd << "/" << name << std::endl;
                        } else {
                            std::cout << name << std::endl;
                        }
                    } else {
                        std::cout << name << std::endl;
                    }
                }
                found = true;
                found_executable = true;
                if (!show_all) {
                    continue;
                }
            }
        }

        if (show_all || (!found_executable && !is_cjsh_custom)) {
            if (shell && shell->get_built_ins()->is_builtin_command(name)) {
                if (!silent) {
                    std::cout << "which: " << name << " is a shell builtin" << std::endl;
                }
                found = true;
            }

            if (shell && (show_all || !found)) {
                auto aliases = shell->get_aliases();
                auto alias_it = aliases.find(name);
                if (alias_it != aliases.end()) {
                    if (!silent) {
                        std::cout << "which: " << name << " is aliased to `" << alias_it->second << "'" << std::endl;
                    }
                    found = true;
                }
            }

            if (shell && (show_all || !found)) {
                auto* interpreter = shell->get_shell_script_interpreter();
                if (interpreter && interpreter->has_function(name)) {
                    if (!silent) {
                        std::cout << "which: " << name << " is a function" << std::endl;
                    }
                    found = true;
                }
            }
        }

        if (!found) {
            if (!silent) {
                std::cout << "which: " << name << " not found" << std::endl;
            }
            return_code = 1;
        }
    }

    return return_code;
}
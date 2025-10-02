#include "type_command.h"
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include "builtin.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "shell.h"
#include "shell_script_interpreter.h"

int type_command(const std::vector<std::string>& args, Shell* shell) {
    if (args.size() < 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "type", "usage: type [-afptP] name [name ...]", {}});
        return 1;
    }

    bool show_all = false;
    bool force_path = false;
    bool show_type_only = false;
    bool inhibit_functions = false;
    bool no_path_search = false;

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
                case 'f':
                    inhibit_functions = true;
                    break;
                case 'p':
                    force_path = true;
                    break;
                case 't':
                    show_type_only = true;
                    break;
                case 'P':
                    no_path_search = true;
                    break;
                default:
                    print_error({ErrorType::INVALID_ARGUMENT, "type", "invalid option: -" + std::string(1, option[j]), {}});
                    return 1;
            }
        }
        start_index = i + 1;
    }

    int return_code = 0;

    for (size_t i = start_index; i < args.size(); ++i) {
        const std::string& name = args[i];
        bool found = false;

        if (!force_path && !inhibit_functions) {
            const std::vector<std::string> keywords = {"if",  "then",   "else",  "elif",  "fi",   "case", "esac",
                                                       "for", "select", "while", "until", "do",   "done", "function",
                                                       "{",   "}",      "[[",    "]]",    "time", "!",    "in"};

            for (const auto& keyword : keywords) {
                if (name == keyword) {
                    if (show_type_only) {
                        std::cout << "keyword" << std::endl;
                    } else {
                        std::cout << name << " is a shell keyword" << std::endl;
                    }
                    found = true;
                    if (!show_all)
                        break;
                }
            }
        }

        if (!found || show_all) {
            if (!force_path && shell && shell->get_built_ins()->is_builtin_command(name)) {
                if (show_type_only) {
                    std::cout << "builtin" << std::endl;
                } else {
                    std::cout << name << " is a shell builtin" << std::endl;
                }
                found = true;
                if (!show_all && found)
                    continue;
            }
        }

        if (!found || show_all) {
            if (!force_path && !inhibit_functions && shell) {
                auto aliases = shell->get_aliases();
                auto alias_it = aliases.find(name);
                if (alias_it != aliases.end()) {
                    if (show_type_only) {
                        std::cout << "alias" << std::endl;
                    } else {
                        std::cout << name << " is aliased to `" << alias_it->second << "'" << std::endl;
                    }
                    found = true;
                    if (!show_all)
                        continue;
                }
            }
        }

        if (!found || show_all) {
            if (!force_path && !inhibit_functions && shell) {
                auto* interpreter = shell->get_shell_script_interpreter();
                if (interpreter && interpreter->has_function(name)) {
                    if (show_type_only) {
                        std::cout << "function" << std::endl;
                    } else {
                        std::cout << name << " is a function" << std::endl;
                    }
                    found = true;
                    if (!show_all)
                        continue;
                }
            }
        }

        if (!found || show_all || force_path) {
            if (!no_path_search) {
                std::string path = cjsh_filesystem::find_executable_in_path(name);
                if (!path.empty()) {
                    if (show_type_only) {
                        std::cout << "file" << std::endl;
                    } else {
                        std::cout << name << " is " << path << std::endl;
                    }
                    found = true;
                }
            }
        }

        if (!found) {
            if (show_type_only) {
            } else {
                std::cout << name << ": not found" << std::endl;
            }
            return_code = 1;
        }
    }

    return return_code;
}

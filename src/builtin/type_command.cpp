/*
  type_command.cpp

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

#include "type_command.h"

#include "builtin_help.h"

#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include "builtin.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "interpreter.h"
#include "shell.h"

int type_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(args, {"Usage: type [-afptP] NAME [NAME ...]",
                                   "Display how the shell resolves each NAME."})) {
        return 0;
    }
    if (args.size() < 2) {
        print_error(
            {ErrorType::INVALID_ARGUMENT, "type", "usage: type [-afptP] name [name ...]", {}});
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
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 "type",
                                 "invalid option: -" + std::string(1, option[j]),
                                 {}});
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
            const std::vector<std::string> keywords = {
                "if",  "then",   "else",  "elif",  "fi",   "case", "esac",
                "for", "select", "while", "until", "do",   "done", "function",
                "{",   "}",      "[[",    "]]",    "time", "!",    "in"};

            for (const auto& keyword : keywords) {
                if (name == keyword) {
                    if (show_type_only) {
                        std::cout << "keyword\n";
                    } else {
                        std::cout << name << " is a shell keyword\n";
                    }
                    found = true;
                    if (!show_all)
                        break;
                }
            }
        }

        if (!found || show_all) {
            if (!force_path && (shell != nullptr) &&
                (shell->get_built_ins()->is_builtin_command(name) != 0)) {
                if (show_type_only) {
                    std::cout << "builtin\n";
                } else {
                    std::cout << name << " is a shell builtin\n";
                }
                found = true;
                if (!show_all && found)
                    continue;
            }
        }

        if (!found || show_all) {
            if (!force_path && !inhibit_functions && (shell != nullptr)) {
                auto aliases = shell->get_aliases();
                auto alias_it = aliases.find(name);
                if (alias_it != aliases.end()) {
                    if (show_type_only) {
                        std::cout << "alias\n";
                    } else {
                        std::cout << name << " is aliased to `" << alias_it->second << "'" << '\n';
                    }
                    found = true;
                    if (!show_all)
                        continue;
                }
            }
        }

        if (!found || show_all) {
            if (!force_path && !inhibit_functions && (shell != nullptr)) {
                auto* interpreter = shell->get_shell_script_interpreter();
                if ((interpreter != nullptr) && interpreter->has_function(name)) {
                    if (show_type_only) {
                        std::cout << "function\n";
                    } else {
                        std::cout << name << " is a function\n";
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
                        std::cout << "file\n";
                    } else {
                        std::cout << name << " is " << path << '\n';
                    }
                    found = true;
                }
            }
        }

        if (!found) {
            if (show_type_only) {
            } else {
                print_error({ErrorType::UNKNOWN_ERROR,  // as to not output an error type message
                             ErrorSeverity::ERROR,
                             "type",
                             name + ": not found",
                             {}});
            }
            return_code = 1;
        }
    }

    return return_code;
}

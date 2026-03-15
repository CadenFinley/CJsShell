/*
  which_command.cpp

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

#include "which_command.h"

#include "builtin_help.h"
#include "builtin_option_parser.h"

#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <iostream>
#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "command_lookup.h"
#include "error_out.h"
#include "interpreter.h"
#include "shell.h"

int which_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(args,
                            {"Usage: which [-as] NAME [NAME ...]",
                             "Show how commands would be resolved in the current environment."})) {
        return 0;
    }
    if (args.size() < 2) {
        print_error(
            {ErrorType::INVALID_ARGUMENT, "which", "usage: which [-as] name [name ...]", {}});
        return 1;
    }

    bool show_all = false;
    bool silent = false;

    size_t start_index = 1;
    const bool options_ok =
        builtin_parse_short_options(args, start_index, "which", [&](char option) {
            switch (option) {
                case 'a':
                    show_all = true;
                    return true;
                case 's':
                    silent = true;
                    return true;
                default:
                    return false;
            }
        });
    if (!options_ok) {
        return 1;
    }

    int return_code = 0;

    for (size_t i = start_index; i < args.size(); ++i) {
        const std::string& name = args[i];
        bool found = false;
        bool found_executable = false;

        const auto resolution = command_lookup::resolve_command(name, shell, true);

        const std::vector<std::string> cjsh_custom_commands = {"echo", "printf", "pwd", "cd"};

        bool is_cjsh_custom = std::find(cjsh_custom_commands.begin(), cjsh_custom_commands.end(),
                                        name) != cjsh_custom_commands.end();

        if (is_cjsh_custom && resolution.is_builtin) {
            if (!silent) {
                std::cout << name << " is a cjsh builtin (custom implementation)\n";
            }
            found = true;
            if (!show_all) {
                continue;
            }
        }

        if (resolution.has_path) {
            if (!silent) {
                std::cout << resolution.path << '\n';
            }
            found = true;
            found_executable = true;
            if (!show_all && !is_cjsh_custom) {
                continue;
            }
        }

        if (!found_executable && (name.find('/') != std::string::npos)) {
            struct stat st{};
            if (stat(name.c_str(), &st) == 0 && ((st.st_mode & S_IXUSR) != 0)) {
                if (!silent) {
                    if (name[0] != '/') {
                        char cwd[PATH_MAX];
                        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
                            std::cout << cwd << "/" << name << '\n';
                        } else {
                            std::cout << name << '\n';
                        }
                    } else {
                        std::cout << name << '\n';
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
            if (resolution.is_builtin) {
                if (!silent) {
                    std::cout << "which: " << name << " is a shell builtin\n";
                }
                found = true;
            }

            if ((shell != nullptr) && (show_all || !found)) {
                if (resolution.has_alias) {
                    if (!silent) {
                        std::cout << "which: " << name << " is aliased to `"
                                  << resolution.alias_value << "'" << '\n';
                    }
                    found = true;
                }
            }

            if ((shell != nullptr) && (show_all || !found)) {
                if (resolution.has_function) {
                    if (!silent) {
                        std::cout << "which: " << name << " is a function\n";
                    }
                    found = true;
                }
            }
        }

        if (!found) {
            if (!silent) {
                print_error({ErrorType::UNKNOWN_ERROR,  // as to not output an error type message
                             ErrorSeverity::ERROR,
                             "which",
                             name + " not found",
                             {}});
            }
            return_code = 1;
        }
    }

    return return_code;
}

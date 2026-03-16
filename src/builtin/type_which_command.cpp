/*
  type_which_command.cpp

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

#include "type_which_command.h"

#include "builtin_help.h"
#include "builtin_option_parser.h"

#include <sys/stat.h>
#include <algorithm>
#include <iostream>
#include "cjsh_filesystem.h"
#include "command_lookup.h"
#include "error_out.h"
#include "shell.h"

namespace {

bool has_kind(const std::vector<command_lookup::CommandResolutionEntry>& entries,
              command_lookup::CommandResolutionKind kind) {
    return std::any_of(
        entries.begin(), entries.end(),
        [&](const command_lookup::CommandResolutionEntry& entry) { return entry.kind == kind; });
}

std::string value_for_kind(const std::vector<command_lookup::CommandResolutionEntry>& entries,
                           command_lookup::CommandResolutionKind kind) {
    auto it = std::find_if(
        entries.begin(), entries.end(),
        [&](const command_lookup::CommandResolutionEntry& entry) { return entry.kind == kind; });
    return it == entries.end() ? std::string{} : it->value;
}

}  // namespace

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
    const bool options_ok =
        builtin_parse_short_options(args, start_index, "type", [&](char option) {
            switch (option) {
                case 'a':
                    show_all = true;
                    return true;
                case 'f':
                    inhibit_functions = true;
                    return true;
                case 'p':
                    force_path = true;
                    return true;
                case 't':
                    show_type_only = true;
                    return true;
                case 'P':
                    no_path_search = true;
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

        const auto entries = command_lookup::list_resolution_entries(name, shell, !no_path_search);

        if (!force_path && !inhibit_functions) {
            if (has_kind(entries, command_lookup::CommandResolutionKind::Keyword)) {
                if (show_type_only) {
                    std::cout << "keyword\n";
                } else {
                    std::cout << name << " is a shell keyword\n";
                }
                found = true;
                if (!show_all)
                    continue;
            }
        }

        if (!found || show_all) {
            if (!force_path && has_kind(entries, command_lookup::CommandResolutionKind::Builtin)) {
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
                if (has_kind(entries, command_lookup::CommandResolutionKind::Alias)) {
                    if (show_type_only) {
                        std::cout << "alias\n";
                    } else {
                        std::cout << name << " is aliased to `"
                                  << value_for_kind(entries,
                                                    command_lookup::CommandResolutionKind::Alias)
                                  << "'\n";
                    }
                    found = true;
                    if (!show_all)
                        continue;
                }
            }
        }

        if (!found || show_all) {
            if (!force_path && !inhibit_functions &&
                has_kind(entries, command_lookup::CommandResolutionKind::Function)) {
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

        if (!found || show_all || force_path) {
            if (!no_path_search) {
                if (has_kind(entries, command_lookup::CommandResolutionKind::Path)) {
                    if (show_type_only) {
                        std::cout << "file\n";
                    } else {
                        std::cout << name << " is "
                                  << value_for_kind(entries,
                                                    command_lookup::CommandResolutionKind::Path)
                                  << '\n';
                    }
                    found = true;
                }
            }
        }

        if (!found) {
            if (!show_type_only) {
                print_error({ErrorType::UNKNOWN_ERROR,
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

        const auto entries = command_lookup::list_resolution_entries(name, shell, true);
        const std::vector<std::string> cjsh_custom_commands = {"echo", "printf", "pwd", "cd"};

        bool is_cjsh_custom = std::find(cjsh_custom_commands.begin(), cjsh_custom_commands.end(),
                                        name) != cjsh_custom_commands.end();

        if (is_cjsh_custom && has_kind(entries, command_lookup::CommandResolutionKind::Builtin)) {
            if (!silent) {
                std::cout << name << " is a cjsh builtin (custom implementation)\n";
            }
            found = true;
            if (!show_all) {
                continue;
            }
        }

        if (has_kind(entries, command_lookup::CommandResolutionKind::Path)) {
            if (!silent) {
                std::cout << value_for_kind(entries, command_lookup::CommandResolutionKind::Path)
                          << '\n';
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
                        const std::string cwd = cjsh_filesystem::safe_current_directory();
                        std::cout << cwd << "/" << name << '\n';
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
            if (has_kind(entries, command_lookup::CommandResolutionKind::Builtin)) {
                if (!silent) {
                    std::cout << "which: " << name << " is a shell builtin\n";
                }
                found = true;
            }

            if ((shell != nullptr) && (show_all || !found)) {
                if (has_kind(entries, command_lookup::CommandResolutionKind::Alias)) {
                    if (!silent) {
                        std::cout << "which: " << name << " is aliased to `"
                                  << value_for_kind(entries,
                                                    command_lookup::CommandResolutionKind::Alias)
                                  << "'\n";
                    }
                    found = true;
                }
            }

            if ((shell != nullptr) && (show_all || !found)) {
                if (has_kind(entries, command_lookup::CommandResolutionKind::Function)) {
                    if (!silent) {
                        std::cout << "which: " << name << " is a function\n";
                    }
                    found = true;
                }
            }
        }

        if (!found) {
            if (!silent) {
                print_error({ErrorType::UNKNOWN_ERROR,
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

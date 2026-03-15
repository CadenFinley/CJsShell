/*
  alias_abbr_commands.cpp

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

#include "alias_abbr_commands.h"

#include <cctype>
#include <iostream>
#include <vector>

#include "builtin_help.h"
#include "cjsh.h"
#include "error_out.h"
#include "parser_utils.h"
#include "shell.h"

namespace {

template <typename Mapping>
void print_named_mapping(const Mapping& mapping, const std::string& command_name,
                         const std::string& empty_message) {
    if (mapping.empty()) {
        std::cout << empty_message << '\n';
        return;
    }

    for (const auto& [name, value] : mapping) {
        std::cout << command_name << " " << name << "='" << value << "'" << '\n';
    }
}

template <typename Mapping, typename NameValidator>
bool assign_or_print_named_entries(const std::vector<std::string>& args, size_t start_index,
                                   Mapping& mapping, const std::string& command_name,
                                   const std::vector<std::string>& not_found_hints,
                                   NameValidator&& validate_name) {
    bool all_successful = true;

    for (size_t i = start_index; i < args.size(); ++i) {
        std::string name;
        std::string value;
        if (parse_assignment(args[i], name, value, true)) {
            if (!validate_name(name)) {
                all_successful = false;
                continue;
            }
            mapping[name] = value;
            continue;
        }

        auto it = mapping.find(args[i]);
        if (it != mapping.end()) {
            std::cout << command_name << " " << it->first << "='" << it->second << "'" << '\n';
        } else {
            print_error({ErrorType::COMMAND_NOT_FOUND, command_name, args[i] + ": not found",
                         not_found_hints});
            all_successful = false;
        }
    }

    return all_successful;
}

template <typename Mapping>
bool remove_named_entries(const std::vector<std::string>& args, size_t start_index,
                          Mapping& mapping, const std::string& command_name) {
    bool success = true;
    for (size_t i = start_index; i < args.size(); ++i) {
        const std::string& name = args[i];
        auto it = mapping.find(name);
        if (it != mapping.end()) {
            mapping.erase(it);
        } else {
            print_error({ErrorType::COMMAND_NOT_FOUND, command_name, name + ": not found", {}});
            success = false;
        }
    }
    return success;
}

}  // namespace

int alias_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(args, {"Usage: alias [NAME[=VALUE] ...]", "List or define aliases.",
                                   "With no operands, display all aliases.",
                                   "NAME=VALUE defines an alias, NAME shows its definition."})) {
        return 0;
    }
    if (args.size() == 1) {
        auto& aliases = shell->get_aliases();
        print_named_mapping(aliases, "alias", "No aliases defined.");
        return 0;
    }

    auto& aliases = shell->get_aliases();
    bool all_successful = assign_or_print_named_entries(args, 1, aliases, "alias", {},
                                                        [](const std::string&) { return true; });

    if (shell != nullptr) {
        shell->set_aliases(aliases);
    }

    return all_successful ? 0 : 1;
}

int unalias_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(args, {"Usage: unalias NAME [NAME ...]", "Remove one or more aliases.",
                                   "Use 'alias --help' to learn how to create aliases."})) {
        return 0;
    }
    if (args.size() < 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "unalias", "not enough arguments", {}});
        return 1;
    }

    auto& aliases = shell->get_aliases();
    bool success = remove_named_entries(args, 1, aliases, "unalias");

    if (shell != nullptr) {
        shell->set_aliases(aliases);
    }

    return success ? 0 : 1;
}

int abbr_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(
            args, {"Usage: abbr [NAME=EXPANSION ...]", "List or define abbreviations.",
                   "With no operands, display all abbreviations.",
                   "NAME=EXPANSION defines an abbreviation, NAME shows its expansion."})) {
        return 0;
    }

    if (shell == nullptr) {
        print_error({ErrorType::FATAL_ERROR, "abbr", "shell not initialized properly", {}});
        return 1;
    }

    auto& abbreviations = shell->get_abbreviations();

    if (args.size() == 1) {
        print_named_mapping(abbreviations, "abbr", "No abbreviations defined.");
        return 0;
    }

    bool all_successful = assign_or_print_named_entries(
        args, 1, abbreviations, "abbr", {"Define it with 'abbr NAME=EXPANSION'."},
        [](const std::string& name) {
            if (name.empty()) {
                print_error({ErrorType::INVALID_ARGUMENT, "abbr", "name cannot be empty", {}});
                return false;
            }

            for (char ch : name) {
                if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 "abbr",
                                 "abbreviation name cannot contain whitespace",
                                 {}});
                    return false;
                }
            }

            return true;
        });

    shell->set_abbreviations(abbreviations);

    return all_successful ? 0 : 1;
}

int unabbr_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(args,
                            {"Usage: unabbr NAME [NAME ...]", "Remove one or more abbreviations.",
                             "Use 'abbr --help' to learn how to create abbreviations."})) {
        return 0;
    }

    if (shell == nullptr) {
        print_error({ErrorType::FATAL_ERROR, "unabbr", "shell not initialized properly", {}});
        return 1;
    }

    if (args.size() < 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "unabbr", "not enough arguments", {}});
        return 1;
    }

    auto& abbreviations = shell->get_abbreviations();
    bool success = remove_named_entries(args, 1, abbreviations, "unabbr");

    shell->set_abbreviations(abbreviations);

    return success ? 0 : 1;
}

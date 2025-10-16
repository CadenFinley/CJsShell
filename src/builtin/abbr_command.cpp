#include "abbr_command.h"

#include "alias_command.h"
#include "builtin_help.h"
#include "error_out.h"
#include "shell.h"

#include <cctype>
#include <iostream>

int abbr_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(
            args, {"Usage: abbr [NAME=EXPANSION ...]", "List or define abbreviations.",
                   "With no operands, display all abbreviations.",
                   "NAME=EXPANSION defines an abbreviation, NAME shows its expansion."})) {
        return 0;
    }

    if (shell == nullptr) {
        print_error({ErrorType::RUNTIME_ERROR, "abbr", "shell unavailable", {}});
        return 1;
    }

    auto& abbreviations = shell->get_abbreviations();

    if (args.size() == 1) {
        if (abbreviations.empty()) {
            std::cout << "No abbreviations defined." << '\n';
        } else {
            for (const auto& [name, value] : abbreviations) {
                std::cout << "abbr " << name << "='" << value << "'" << '\n';
            }
        }
        return 0;
    }

    bool all_successful = true;

    for (size_t i = 1; i < args.size(); ++i) {
        std::string name;
        std::string value;
        if (parse_assignment(args[i], name, value)) {
            if (name.empty()) {
                print_error({ErrorType::INVALID_ARGUMENT, "abbr", "name cannot be empty", {}});
                all_successful = false;
                continue;
            }
            bool has_whitespace = false;
            for (char ch : name) {
                if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                    has_whitespace = true;
                    break;
                }
            }
            if (has_whitespace) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             "abbr",
                             "abbreviation name cannot contain whitespace",
                             {}});
                all_successful = false;
                continue;
            }
            abbreviations[name] = value;
        } else {
            auto it = abbreviations.find(args[i]);
            if (it != abbreviations.end()) {
                std::cout << "abbr " << it->first << "='" << it->second << "'" << '\n';
            } else {
                print_error({ErrorType::COMMAND_NOT_FOUND,
                             "abbr",
                             args[i] + ": not found",
                             {"Define it with 'abbr NAME=EXPANSION'."}});
                all_successful = false;
            }
        }
    }

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
        print_error({ErrorType::RUNTIME_ERROR, "unabbr", "shell unavailable", {}});
        return 1;
    }

    if (args.size() < 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "unabbr", "not enough arguments", {}});
        return 1;
    }

    bool success = true;
    auto& abbreviations = shell->get_abbreviations();

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& name = args[i];
        auto it = abbreviations.find(name);
        if (it != abbreviations.end()) {
            abbreviations.erase(it);
        } else {
            print_error({ErrorType::COMMAND_NOT_FOUND, "unabbr", name + ": not found", {}});
            success = false;
        }
    }

    shell->set_abbreviations(abbreviations);

    return success ? 0 : 1;
}

#include "alias_command.h"

#include <fstream>
#include <iostream>
#include <vector>

#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "error_out.h"

int alias_command(const std::vector<std::string>& args, Shell* shell) {
    if (args.size() == 1) {
        auto& aliases = shell->get_aliases();
        if (aliases.empty()) {
            std::cout << "No aliases defined." << std::endl;
        } else {
            for (const auto& [name, value] : aliases) {
                std::cout << "alias " << name << "='" << value << "'"
                          << std::endl;
            }
        }
        return 0;
    }

    bool all_successful = true;
    auto& aliases = shell->get_aliases();

    for (size_t i = 1; i < args.size(); ++i) {
        std::string name, value;
        if (parse_assignment(args[i], name, value)) {
            aliases[name] = value;
            if (g_debug_mode) {
                std::cout << "Added alias: " << name << "='" << value << "'"
                          << std::endl;
            }
        } else {
            auto it = aliases.find(args[i]);
            if (it != aliases.end()) {
                std::cout << "alias " << it->first << "='" << it->second << "'"
                          << std::endl;
            } else {
                print_error({ErrorType::COMMAND_NOT_FOUND,
                             "alias",
                             args[i] + ": not found",
                             {}});
                all_successful = false;
            }
        }
    }

    if (shell) {
        shell->set_aliases(aliases);
    }

    return all_successful ? 0 : 1;
}

int unalias_command(const std::vector<std::string>& args, Shell* shell) {
    if (args.size() < 2) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "unalias",
                     "not enough arguments",
                     {}});
        return 1;
    }

    bool success = true;
    auto& aliases = shell->get_aliases();

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& name = args[i];
        auto it = aliases.find(name);

        if (it != aliases.end()) {
            aliases.erase(it);
            if (g_debug_mode) {
                std::cout << "Removed alias: " << name << std::endl;
            }
        } else {
            print_error({ErrorType::COMMAND_NOT_FOUND,
                         "unalias",
                         name + ": not found",
                         {}});
            success = false;
        }
    }

    if (shell) {
        shell->set_aliases(aliases);
    }

    return success ? 0 : 1;
}

bool parse_assignment(const std::string& arg, std::string& name,
                      std::string& value) {
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
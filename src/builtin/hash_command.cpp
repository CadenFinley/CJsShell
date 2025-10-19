#include "hash_command.h"

#include "builtin_help.h"

#include <iostream>
#include <unordered_map>
#include "cjsh_filesystem.h"
#include "error_out.h"

namespace {

std::unordered_map<std::string, std::string> command_hash;
std::unordered_map<std::string, int> command_hits;

}  // namespace

int hash_command(const std::vector<std::string>& args, Shell* shell) {
    (void)shell;

    if (builtin_handle_help(args, {"Usage: hash [-r|-d] [NAME ...]",
                                   "Display or control the command path hash table.",
                                   "With no operands, list cached commands.",
                                   "-r clears entries, -d removes lookup caching for NAME."})) {
        return 0;
    }

    if (args.size() == 1) {
        if (command_hash.empty()) {
            return 0;
        }

        std::cout << "hits\tcommand\n";
        for (const auto& pair : command_hash) {
            int hits = (command_hits.count(pair.first) != 0u) ? command_hits[pair.first] : 0;
            std::cout << hits << "\t" << pair.second << '\n';
        }
        return 0;
    }

    bool remove_mode = false;
    bool disable_lookup = false;
    size_t start_index = 1;

    for (size_t i = 1; i < args.size() && args[i][0] == '-'; ++i) {
        const std::string& option = args[i];
        if (option == "--") {
            start_index = i + 1;
            break;
        }

        if (option == "-r") {
            remove_mode = true;
        } else if (option == "-d") {
            disable_lookup = true;
        } else if (option == "-l") {
        } else if (option == "-p") {
        } else if (option == "-t") {
        } else {
            print_error({ErrorType::INVALID_ARGUMENT, "hash", "invalid option: " + option, {}});
            return 1;
        }
        start_index = i + 1;
    }

    if (remove_mode && start_index >= args.size()) {
        command_hash.clear();
        command_hits.clear();
        return 0;
    }

    for (size_t i = start_index; i < args.size(); ++i) {
        const std::string& name = args[i];

        if (remove_mode) {
            command_hash.erase(name);
            command_hits.erase(name);
        } else if (disable_lookup) {
            command_hash.erase(name);
            command_hits.erase(name);
        } else {
            std::string path = cjsh_filesystem::find_executable_in_path(name);
            if (!path.empty()) {
                command_hash[name] = path;
                if (command_hits.count(name) == 0) {
                    command_hits[name] = 0;
                }

                std::cout << path << '\n';
            } else {
                print_error({ErrorType::COMMAND_NOT_FOUND, "hash", name + ": not found", {}});
                return 1;
            }
        }
    }

    return 0;
}

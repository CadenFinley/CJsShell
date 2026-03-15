/*
  hash_command.cpp

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

#include "hash_command.h"

#include <iomanip>
#include <iostream>

#include "builtin_help.h"
#include "cjsh_filesystem.h"
#include "error_out.h"

namespace {

void print_cache_entries(const std::vector<cjsh_filesystem::PathHashEntry>& entries) {
    if (entries.empty()) {
        std::cout << "hash: no cached commands" << '\n';
        return;
    }

    std::cout << std::left << std::setw(6) << "hits" << std::setw(20) << "command"
              << "location" << '\n';
    for (const auto& entry : entries) {
        std::cout << std::left << std::setw(6) << entry.hits << std::setw(20) << entry.command
                  << entry.path;
        if (entry.manually_added) {
            std::cout << " *";
        }
        std::cout << '\n';
    }
}

void report_target_missing(const std::string& name) {
    print_error({ErrorType::COMMAND_NOT_FOUND,
                 "hash",
                 name + ": not found in PATH or not cacheable",
                 {"Only bare command names without '/' can be hashed."}});
}

}  // namespace

int hash_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(
            args,
            {"Usage: hash [-r] [NAME ...]", "Display or populate the command path hash cache.",
             "With NAME operands, add them to the cache.",
             "With no operands, list cached commands. -r resets the cache."})) {
        return 0;
    }

    bool reset_cache = false;
    std::vector<std::string> targets;

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--") {
            targets.insert(targets.end(), args.begin() + static_cast<std::ptrdiff_t>(i + 1),
                           args.end());
            break;
        }
        if (!arg.empty() && arg[0] == '-') {
            if (arg == "-r") {
                reset_cache = true;
                continue;
            }
            print_error({ErrorType::INVALID_ARGUMENT,
                         "hash",
                         "invalid option: " + arg,
                         {"Use 'hash -r' or specify command names."}});
            return 2;
        }
        targets.push_back(arg);
    }

    if (reset_cache) {
        cjsh_filesystem::reset_path_hash();
        if (targets.empty()) {
            return 0;
        }
    }

    if (targets.empty()) {
        print_cache_entries(cjsh_filesystem::get_path_hash_entries());
        return 0;
    }

    int status = 0;
    for (const auto& name : targets) {
        std::string resolved;
        if (!cjsh_filesystem::hash_executable(name, &resolved)) {
            report_target_missing(name);
            status = 1;
        }
    }

    return status;
}

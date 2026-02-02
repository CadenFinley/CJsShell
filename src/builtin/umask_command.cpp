/*
  umask_command.cpp

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

#include "umask_command.h"
#include "builtin_help.h"

#include <sys/stat.h>

#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "error_out.h"

namespace {

std::string format_octal_mode(mode_t mode) {
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << std::oct << mode;
    return oss.str();
}

std::string format_symbolic_mode(mode_t mask) {
    mode_t perms = (~mask) & 0777;
    std::ostringstream oss;

    oss << "u=";
    if (perms & S_IRUSR)
        oss << "r";
    if (perms & S_IWUSR)
        oss << "w";
    if (perms & S_IXUSR)
        oss << "x";

    oss << ",g=";
    if (perms & S_IRGRP)
        oss << "r";
    if (perms & S_IWGRP)
        oss << "w";
    if (perms & S_IXGRP)
        oss << "x";

    oss << ",o=";
    if (perms & S_IROTH)
        oss << "r";
    if (perms & S_IWOTH)
        oss << "w";
    if (perms & S_IXOTH)
        oss << "x";

    return oss.str();
}

bool parse_octal_mode(const std::string& str, mode_t& result) {
    if (str.empty()) {
        return false;
    }

    for (char c : str) {
        if (c < '0' || c > '7') {
            return false;
        }
    }

    try {
        unsigned long mode = std::stoul(str, nullptr, 8);
        if (mode > 0777) {
            return false;
        }
        result = static_cast<mode_t>(mode);
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_symbolic_mode(const std::string& str, mode_t current_mask, mode_t& result) {
    mode_t perms = (~current_mask) & 0777;
    mode_t new_perms = perms;

    size_t pos = 0;

    while (pos < str.length()) {
        mode_t who_mask = 0;
        bool has_who = false;

        while (pos < str.length() &&
               (str[pos] == 'u' || str[pos] == 'g' || str[pos] == 'o' || str[pos] == 'a')) {
            has_who = true;
            if (str[pos] == 'u') {
                who_mask |= S_IRWXU;
            } else if (str[pos] == 'g') {
                who_mask |= S_IRWXG;
            } else if (str[pos] == 'o') {
                who_mask |= S_IRWXO;
            } else if (str[pos] == 'a') {
                who_mask = S_IRWXU | S_IRWXG | S_IRWXO;
            }
            pos++;
        }

        if (!has_who) {
            who_mask = S_IRWXU | S_IRWXG | S_IRWXO;
        }

        if (pos >= str.length()) {
            return false;
        }

        char op = str[pos];
        if (op != '=' && op != '+' && op != '-') {
            return false;
        }
        pos++;

        mode_t perm_bits = 0;
        while (pos < str.length() && (str[pos] == 'r' || str[pos] == 'w' || str[pos] == 'x')) {
            if (str[pos] == 'r') {
                perm_bits |= (who_mask & S_IRWXU) ? S_IRUSR : 0;
                perm_bits |= (who_mask & S_IRWXG) ? S_IRGRP : 0;
                perm_bits |= (who_mask & S_IRWXO) ? S_IROTH : 0;
            } else if (str[pos] == 'w') {
                perm_bits |= (who_mask & S_IRWXU) ? S_IWUSR : 0;
                perm_bits |= (who_mask & S_IRWXG) ? S_IWGRP : 0;
                perm_bits |= (who_mask & S_IRWXO) ? S_IWOTH : 0;
            } else if (str[pos] == 'x') {
                perm_bits |= (who_mask & S_IRWXU) ? S_IXUSR : 0;
                perm_bits |= (who_mask & S_IRWXG) ? S_IXGRP : 0;
                perm_bits |= (who_mask & S_IRWXO) ? S_IXOTH : 0;
            }
            pos++;
        }

        if (op == '=') {
            new_perms = (new_perms & ~who_mask) | perm_bits;
        } else if (op == '+') {
            new_perms |= perm_bits;
        } else if (op == '-') {
            new_perms &= ~perm_bits;
        }

        if (pos < str.length() && str[pos] == ',') {
            pos++;
        }
    }

    result = (~new_perms) & 0777;
    return true;
}

}  // namespace

int umask_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(
            args,
            {"Usage: umask [-p] [-S] [MODE]", "Display or set the file mode creation mask.", "",
             "  -p        output in a form that can be reused as input",
             "  -S        display in symbolic form (default is octal)", "",
             "MODE can be:", "  Octal:    like 0022 (blocks write for group and others)",
             "  Symbolic: like u=rwx,g=rx,o=rx", "",
             "If MODE is omitted, prints the current mask value.",
             "The mask specifies which permission bits are NOT set on newly created files."})) {
        return 0;
    }

    mode_t current_mask = umask(0);
    umask(current_mask);

    bool symbolic_output = false;
    bool posix_output = false;
    size_t mode_index = 1;

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (arg == "-S") {
            symbolic_output = true;
            mode_index = i + 1;
        } else if (arg == "-p") {
            posix_output = true;
            mode_index = i + 1;
        } else if (arg == "--help" || arg == "--version") {
            return 0;
        } else if (arg[0] == '-' && arg != "-") {
            for (size_t j = 1; j < arg.length(); ++j) {
                if (arg[j] == 'S') {
                    symbolic_output = true;
                } else if (arg[j] == 'p') {
                    posix_output = true;
                } else {
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 "umask",
                                 "invalid option -- '" + std::string(1, arg[j]) + "'",
                                 {"Try 'umask --help' for more information."}});
                    return 1;
                }
            }
            mode_index = i + 1;
        } else {
            mode_index = i;
            break;
        }
    }

    if (mode_index >= args.size()) {
        if (posix_output) {
            std::cout << "umask ";
            if (symbolic_output) {
                std::cout << format_symbolic_mode(current_mask);
            } else {
                std::cout << format_octal_mode(current_mask);
            }
            std::cout << '\n';
        } else if (symbolic_output) {
            std::cout << format_symbolic_mode(current_mask) << '\n';
        } else {
            std::cout << format_octal_mode(current_mask) << '\n';
        }
        return 0;
    }

    const std::string& mode_str = args[mode_index];
    mode_t new_mask;

    if (mode_str.find_first_of("=+-") != std::string::npos) {
        if (!parse_symbolic_mode(mode_str, current_mask, new_mask)) {
            print_error(
                {ErrorType::INVALID_ARGUMENT, "umask", "invalid symbolic mode: " + mode_str, {}});
            return 1;
        }
    } else {
        if (!parse_octal_mode(mode_str, new_mask)) {
            print_error({ErrorType::INVALID_ARGUMENT, "umask", "invalid mode: " + mode_str, {}});
            return 1;
        }
    }

    umask(new_mask);

    return 0;
}

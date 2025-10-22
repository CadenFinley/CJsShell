// umask - set file mode creation mask
// Based on GNU Coreutils principles
// Copyright (C) 1990-2025 Free Software Foundation, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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

// Format mode as octal string
std::string format_octal_mode(mode_t mode) {
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << std::oct << mode;
    return oss.str();
}

// Format mode in symbolic notation (showing permissions NOT masked)
std::string format_symbolic_mode(mode_t mask) {
    // mask specifies what is BLOCKED, so we invert to show what is ALLOWED
    mode_t perms = (~mask) & 0777;
    std::ostringstream oss;

    // User permissions
    oss << "u=";
    if (perms & S_IRUSR)
        oss << "r";
    if (perms & S_IWUSR)
        oss << "w";
    if (perms & S_IXUSR)
        oss << "x";

    // Group permissions
    oss << ",g=";
    if (perms & S_IRGRP)
        oss << "r";
    if (perms & S_IWGRP)
        oss << "w";
    if (perms & S_IXGRP)
        oss << "x";

    // Other permissions
    oss << ",o=";
    if (perms & S_IROTH)
        oss << "r";
    if (perms & S_IWOTH)
        oss << "w";
    if (perms & S_IXOTH)
        oss << "x";

    return oss.str();
}

// Parse octal mode string
bool parse_octal_mode(const std::string& str, mode_t& result) {
    if (str.empty()) {
        return false;
    }

    // Check if all characters are octal digits
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

// Parse symbolic mode specification
// Supports: u=rwx, g=rx, o=rx, a=rwx, etc.
bool parse_symbolic_mode(const std::string& str, mode_t current_mask, mode_t& result) {
    // For symbolic mode, we work with permissions (inverted mask)
    mode_t perms = (~current_mask) & 0777;
    mode_t new_perms = perms;

    size_t pos = 0;

    while (pos < str.length()) {
        // Parse who: u, g, o, a
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

        // If no who specified, default to 'a' (all)
        if (!has_who) {
            who_mask = S_IRWXU | S_IRWXG | S_IRWXO;
        }

        // Parse operator: =, +, -
        if (pos >= str.length()) {
            return false;
        }

        char op = str[pos];
        if (op != '=' && op != '+' && op != '-') {
            return false;
        }
        pos++;

        // Parse permissions: r, w, x
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

        // Apply operation
        if (op == '=') {
            // Clear the who bits and set new permissions
            new_perms = (new_perms & ~who_mask) | perm_bits;
        } else if (op == '+') {
            new_perms |= perm_bits;
        } else if (op == '-') {
            new_perms &= ~perm_bits;
        }

        // Skip comma if present
        if (pos < str.length() && str[pos] == ',') {
            pos++;
        }
    }

    // Convert permissions back to mask (invert)
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

    // Get current mask
    mode_t current_mask = umask(0);
    umask(current_mask);  // Restore it

    bool symbolic_output = false;
    bool posix_output = false;
    size_t mode_index = 1;

    // Parse options
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (arg == "-S") {
            symbolic_output = true;
            mode_index = i + 1;
        } else if (arg == "-p") {
            posix_output = true;
            mode_index = i + 1;
        } else if (arg == "--help" || arg == "--version") {
            // Already handled by builtin_handle_help
            return 0;
        } else if (arg[0] == '-' && arg != "-") {
            // Handle combined options like -pS
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
            // This is the mode argument
            mode_index = i;
            break;
        }
    }

    // If no mode specified, display current mask
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

    // Parse and set new mode
    const std::string& mode_str = args[mode_index];
    mode_t new_mask;

    // Try symbolic mode first (contains = + or -)
    if (mode_str.find_first_of("=+-") != std::string::npos) {
        if (!parse_symbolic_mode(mode_str, current_mask, new_mask)) {
            print_error(
                {ErrorType::INVALID_ARGUMENT, "umask", "invalid symbolic mode: " + mode_str, {}});
            return 1;
        }
    } else {
        // Try octal mode
        if (!parse_octal_mode(mode_str, new_mask)) {
            print_error({ErrorType::INVALID_ARGUMENT, "umask", "invalid mode: " + mode_str, {}});
            return 1;
        }
    }

    // Set the new mask
    umask(new_mask);

    return 0;
}

// echo - display a line of text
// Derived from code in GNU Coreutils and Bash
// Copyright (C) 1987-2025 Free Software Foundation, Inc.
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
//
// Authors: Brian Fox, Chet Ramey

#include "echo_command.h"
#include "builtin_help.h"

#include <cstdlib>
#include <iostream>
#include <string>

#include "parser/parser_utils.h"

namespace {

// Convert hexadecimal character to integer
inline int hextobin(unsigned char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 0;
}

}  // namespace

int echo_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args, {"Usage: echo [-neE] [STRING ...]",
                                   "Display the STRING(s) to standard output.",
                                   "",
                                   "  -n     do not output the trailing newline",
                                   "  -e     enable interpretation of backslash escapes",
                                   "  -E     disable interpretation of backslash escapes (default)",
                                   "",
                                   "If -e is in effect, the following sequences are recognized:",
                                   "",
                                   "  \\\\      backslash",
                                   "  \\a      alert (BEL)",
                                   "  \\b      backspace",
                                   "  \\c      produce no further output",
                                   "  \\e      escape",
                                   "  \\f      form feed",
                                   "  \\n      new line",
                                   "  \\r      carriage return",
                                   "  \\t      horizontal tab",
                                   "  \\v      vertical tab",
                                   "  \\0NNN   byte with octal value NNN (1 to 3 digits)",
                                   "  \\xHH    byte with hexadecimal value HH (1 to 2 digits)"})) {
        return 0;
    }

    bool display_return = true;
    bool do_v9 = false;  // XSI mode (interpret backslash escapes)
    bool posixly_correct = (std::getenv("POSIXLY_CORRECT") != nullptr);
    bool allow_options = !posixly_correct || (args.size() > 1 && args[1] == "-n");

    std::vector<std::string> echo_args = args;

    // Check for redirection to stderr
    bool redirect_to_stderr = false;
    if (!echo_args.empty() && echo_args.back() == ">&2") {
        redirect_to_stderr = true;
        echo_args.pop_back();
    }

    size_t arg_idx = 1;

    // Parse options
    if (allow_options) {
        while (arg_idx < echo_args.size() && !echo_args[arg_idx].empty() &&
               echo_args[arg_idx][0] == '-') {
            const std::string& opt = echo_args[arg_idx];

            // Check if this is just "-" or if it contains invalid options
            if (opt.length() == 1) {
                break;  // Just a dash, treat as argument
            }

            bool valid_options = true;
            for (size_t i = 1; i < opt.length(); i++) {
                char c = opt[i];
                if (c != 'e' && c != 'E' && c != 'n') {
                    valid_options = false;
                    break;
                }
            }

            if (!valid_options) {
                break;  // Invalid option, treat as argument
            }

            // Process valid options
            for (size_t i = 1; i < opt.length(); i++) {
                switch (opt[i]) {
                    case 'e':
                        do_v9 = true;
                        break;
                    case 'E':
                        do_v9 = false;
                        break;
                    case 'n':
                        display_return = false;
                        break;
                }
            }

            arg_idx++;
        }
    }

    // Output target
    std::ostream& out = redirect_to_stderr ? std::cerr : std::cout;

    // Print arguments
    bool first = true;

    if (do_v9 || posixly_correct) {
        // Interpret backslash escapes
        while (arg_idx < echo_args.size()) {
            if (!first) {
                out << ' ';
            }
            first = false;

            const std::string& s = echo_args[arg_idx];
            size_t i = 0;

            while (i < s.length()) {
                unsigned char c = s[i];

                if (c == '\\' && i + 1 < s.length()) {
                    i++;
                    c = s[i];

                    switch (c) {
                        case 'a':
                            out << '\a';
                            break;
                        case 'b':
                            out << '\b';
                            break;
                        case 'c':
                            // Stop processing and return
                            out.flush();
                            return 0;
                        case 'e':
                            out << '\x1B';
                            break;
                        case 'f':
                            out << '\f';
                            break;
                        case 'n':
                            out << '\n';
                            break;
                        case 'r':
                            out << '\r';
                            break;
                        case 't':
                            out << '\t';
                            break;
                        case 'v':
                            out << '\v';
                            break;
                        case 'x': {
                            // Hexadecimal escape \xHH
                            if (i + 1 < s.length() && is_hex_digit(s[i + 1])) {
                                i++;
                                unsigned char ch = s[i];
                                c = hextobin(ch);

                                if (i + 1 < s.length() && is_hex_digit(s[i + 1])) {
                                    i++;
                                    ch = s[i];
                                    c = c * 16 + hextobin(ch);
                                }
                                out << static_cast<char>(c);
                            } else {
                                // No valid hex digits, output \x literally
                                out << '\\' << 'x';
                            }
                            break;
                        }
                        case '0':
                            // Octal escape \0NNN
                            c = 0;
                            if (i + 1 < s.length() && s[i + 1] >= '0' && s[i + 1] <= '7') {
                                i++;
                                c = s[i] - '0';
                                if (i + 1 < s.length() && s[i + 1] >= '0' && s[i + 1] <= '7') {
                                    i++;
                                    c = c * 8 + (s[i] - '0');
                                    if (i + 1 < s.length() && s[i + 1] >= '0' && s[i + 1] <= '7') {
                                        i++;
                                        c = c * 8 + (s[i] - '0');
                                    }
                                }
                            }
                            out << static_cast<char>(c);
                            break;
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7': {
                            // Octal escape \NNN (starting with non-zero)
                            c = c - '0';
                            if (i + 1 < s.length() && s[i + 1] >= '0' && s[i + 1] <= '7') {
                                i++;
                                c = c * 8 + (s[i] - '0');
                                if (i + 1 < s.length() && s[i + 1] >= '0' && s[i + 1] <= '7') {
                                    i++;
                                    c = c * 8 + (s[i] - '0');
                                }
                            }
                            out << static_cast<char>(c);
                            break;
                        }
                        case '\\':
                            out << '\\';
                            break;
                        default:
                            // Unknown escape, output backslash literally
                            out << '\\';
                            i--;  // Will be incremented at end of loop
                            break;
                    }
                } else {
                    out << static_cast<char>(c);
                }
                i++;
            }
            arg_idx++;
        }
    } else {
        // No escape interpretation
        while (arg_idx < echo_args.size()) {
            if (!first) {
                out << ' ';
            }
            first = false;
            out << echo_args[arg_idx];
            arg_idx++;
        }
    }

    if (display_return) {
        out << '\n';
    }

    out.flush();
    return 0;
}

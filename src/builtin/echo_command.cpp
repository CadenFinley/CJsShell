/*
  echo_command.cpp

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

#include "echo_command.h"
#include "builtin_help.h"

#include <cstdlib>
#include <iostream>
#include <string>

#include "parser_utils.h"

namespace {

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
    bool do_v9 = false;
    bool posixly_correct = (std::getenv("POSIXLY_CORRECT") != nullptr);
    bool allow_options = !posixly_correct || (args.size() > 1 && args[1] == "-n");

    std::vector<std::string> echo_args = args;

    bool redirect_to_stderr = false;
    if (!echo_args.empty() && echo_args.back() == ">&2") {
        redirect_to_stderr = true;
        echo_args.pop_back();
    }

    size_t arg_idx = 1;

    if (allow_options) {
        while (arg_idx < echo_args.size() && !echo_args[arg_idx].empty() &&
               echo_args[arg_idx][0] == '-') {
            const std::string& opt = echo_args[arg_idx];

            if (opt.length() == 1) {
                break;
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
                break;
            }

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

    std::ostream& out = redirect_to_stderr ? std::cerr : std::cout;

    bool first = true;

    if (do_v9 || posixly_correct) {
        while (arg_idx < echo_args.size()) {
            if (!first) {
                out << ' ';
            }
            first = false;

            const std::string& s = echo_args[arg_idx];
            size_t i = 0;

            while (i < s.length()) {
                unsigned char c = static_cast<unsigned char>(s[i]);

                if (c == '\\' && i + 1 < s.length()) {
                    i++;
                    c = static_cast<unsigned char>(s[i]);

                    switch (c) {
                        case 'a':
                            out << '\a';
                            break;
                        case 'b':
                            out << '\b';
                            break;
                        case 'c':

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
                            if (i + 1 < s.length() && is_hex_digit(s[i + 1])) {
                                i++;
                                unsigned char ch = static_cast<unsigned char>(s[i]);
                                unsigned int value = static_cast<unsigned int>(hextobin(ch));

                                if (i + 1 < s.length() && is_hex_digit(s[i + 1])) {
                                    i++;
                                    ch = static_cast<unsigned char>(s[i]);
                                    value = value * 16u + static_cast<unsigned int>(hextobin(ch));
                                }
                                out << static_cast<char>(static_cast<unsigned char>(value));
                            } else {
                                out << '\\' << 'x';
                            }
                            break;
                        }
                        case '0':

                        {
                            unsigned int value = 0;
                            if (i + 1 < s.length() && s[i + 1] >= '0' && s[i + 1] <= '7') {
                                i++;
                                value = static_cast<unsigned int>(s[i] - '0');
                                if (i + 1 < s.length() && s[i + 1] >= '0' && s[i + 1] <= '7') {
                                    i++;
                                    value = value * 8u + static_cast<unsigned int>(s[i] - '0');
                                    if (i + 1 < s.length() && s[i + 1] >= '0' && s[i + 1] <= '7') {
                                        i++;
                                        value = value * 8u + static_cast<unsigned int>(s[i] - '0');
                                    }
                                }
                            }
                            out << static_cast<char>(static_cast<unsigned char>(value));
                        } break;
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7': {
                            unsigned int value = static_cast<unsigned int>(c - '0');
                            if (i + 1 < s.length() && s[i + 1] >= '0' && s[i + 1] <= '7') {
                                i++;
                                value = value * 8u + static_cast<unsigned int>(s[i] - '0');
                                if (i + 1 < s.length() && s[i + 1] >= '0' && s[i + 1] <= '7') {
                                    i++;
                                    value = value * 8u + static_cast<unsigned int>(s[i] - '0');
                                }
                            }
                            out << static_cast<char>(static_cast<unsigned char>(value));
                            break;
                        }
                        case '\\':
                            out << '\\';
                            break;
                        default:

                            out << '\\';
                            i--;
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

    if (!redirect_to_stderr) {
        out.flush();
    }
    return 0;
}

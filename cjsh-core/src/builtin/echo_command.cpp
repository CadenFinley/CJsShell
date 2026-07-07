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
#include "shell_env.h"

#include <iostream>
#include <string>

#include "parser_utils.h"

namespace {

bool is_octal_digit(char c) {
    return c >= '0' && c <= '7';
}

bool is_valid_echo_option(const std::string& option) {
    if (option.length() <= 1 || option[0] != '-') {
        return false;
    }

    for (size_t i = 1; i < option.length(); ++i) {
        char c = option[i];
        if (c != 'e' && c != 'E' && c != 'n') {
            return false;
        }
    }

    return true;
}

void apply_echo_option(const std::string& option, bool& interpret_escapes, bool& append_newline) {
    for (size_t i = 1; i < option.length(); ++i) {
        switch (option[i]) {
            case 'e':
                interpret_escapes = true;
                break;
            case 'E':
                interpret_escapes = false;
                break;
            case 'n':
                append_newline = false;
                break;
        }
    }
}

unsigned int consume_octal_digits(const std::string& text, size_t& i, unsigned int value,
                                  size_t max_digits) {
    size_t digits = 0;
    while (digits < max_digits && i + 1 < text.length() && is_octal_digit(text[i + 1])) {
        ++i;
        value = value * 8U + static_cast<unsigned int>(text[i] - '0');
        ++digits;
    }
    return value;
}

bool parse_hex_byte(const std::string& text, size_t& i, unsigned int& value) {
    if (i + 1 >= text.length() || !is_hex_digit(text[i + 1])) {
        return false;
    }

    ++i;
    int high = from_hex_digit(text[i]);
    if (high < 0) {
        return false;
    }
    value = static_cast<unsigned int>(high);

    if (i + 1 < text.length() && is_hex_digit(text[i + 1])) {
        ++i;
        int low = from_hex_digit(text[i]);
        if (low < 0) {
            return false;
        }
        value = value * 16U + static_cast<unsigned int>(low);
    }

    return true;
}

bool print_with_escapes(std::ostream& out, const std::string& input) {
    size_t i = 0;
    while (i < input.length()) {
        unsigned char c = static_cast<unsigned char>(input[i]);

        if (c != '\\' || i + 1 >= input.length()) {
            out << static_cast<char>(c);
            ++i;
            continue;
        }

        c = static_cast<unsigned char>(input[++i]);

        switch (c) {
            case 'a':
                out << '\a';
                break;
            case 'b':
                out << '\b';
                break;
            case 'c':
                out.flush();
                return false;
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
                unsigned int value = 0;
                if (parse_hex_byte(input, i, value)) {
                    out << static_cast<char>(static_cast<unsigned char>(value));
                } else {
                    out << '\\' << 'x';
                }
                break;
            }
            case '0': {
                unsigned int value = consume_octal_digits(input, i, 0U, 3);
                out << static_cast<char>(static_cast<unsigned char>(value));
                break;
            }
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7': {
                unsigned int value =
                    consume_octal_digits(input, i, static_cast<unsigned int>(c - '0'), 2);
                out << static_cast<char>(static_cast<unsigned char>(value));
                break;
            }
            case '\\':
                out << '\\';
                break;
            default:
                out << '\\' << static_cast<char>(c);
                break;
        }

        ++i;
    }

    return true;
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

    bool append_newline = true;
    bool interpret_escapes = false;
    bool posixly_correct = cjsh_env::shell_variable_is_set("POSIXLY_CORRECT");
    bool allow_options = !posixly_correct || (args.size() > 1 && args[1] == "-n");

    bool redirect_to_stderr = !args.empty() && args.back() == ">&2";
    size_t args_end = redirect_to_stderr ? args.size() - 1 : args.size();

    size_t arg_idx = 1;

    if (allow_options) {
        while (arg_idx < args_end && is_valid_echo_option(args[arg_idx])) {
            apply_echo_option(args[arg_idx], interpret_escapes, append_newline);
            ++arg_idx;
        }
    }

    std::ostream& out = redirect_to_stderr ? std::cerr : std::cout;
    bool first = true;

    while (arg_idx < args_end) {
        if (!first) {
            out << ' ';
        }
        first = false;

        const std::string& value = args[arg_idx];
        if (interpret_escapes || posixly_correct) {
            if (!print_with_escapes(out, value)) {
                return 0;
            }
        } else {
            out << value;
        }

        ++arg_idx;
    }

    if (append_newline) {
        out << '\n';
    }

    if (!redirect_to_stderr) {
        out.flush();
    }
    return 0;
}

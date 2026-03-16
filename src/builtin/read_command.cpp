/*
  read_command.cpp

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

#include "read_command.h"

#include "builtin_help.h"
#include "builtin_option_parser.h"

#include <poll.h>
#include <unistd.h>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include "error_out.h"
#include "readonly_command.h"
#include "shell.h"
#include "shell_env.h"

int read_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(
            args,
            {"Usage: read [-r] [-p prompt] [-n nchars] [-d delim] [-t timeout] [name ...]",
             "Read a line from standard input and split it into fields.", "",
             "Options:", "  -r            do not allow backslashes to escape any characters",
             "  -p prompt     output PROMPT without a trailing newline before reading",
             "  -n nchars     return after reading NCHARS characters rather than waiting for a "
             "newline",
             "  -d delim      continue until the first character of DELIM is read, rather than "
             "newline",
             "  -t timeout    time out after TIMEOUT seconds (fractional allowed)"},
            BuiltinHelpScanMode::AnyArgument)) {
        return 0;
    }

    if (shell == nullptr) {
        print_error({ErrorType::FATAL_ERROR, "read", "shell not initialized properly", {}});
        return 1;
    }

    std::cin.clear();
    clearerr(stdin);

    bool raw_mode = false;
    int nchars = -1;
    std::string prompt;
    std::string delim = "\n";
    bool has_timeout = false;
    double timeout_seconds = 0.0;

    size_t start_index = 1;
    std::vector<BuiltinParsedShortOption> parsed_options;
    const bool options_ok = builtin_parse_short_options_ex(
        args, start_index, "read",
        [](char option) {
            return option == 'r' || option == 'n' || option == 'p' || option == 'd' ||
                   option == 't';
        },
        [](char option) {
            return option == 'n' || option == 'p' || option == 'd' || option == 't';
        },
        parsed_options);
    if (!options_ok) {
        return 1;
    }

    for (const auto& option : parsed_options) {
        switch (option.option) {
            case 'r':
                raw_mode = true;
                break;
            case 'n':
                try {
                    nchars = std::stoi(option.value.value_or(""));
                } catch (const std::exception&) {
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 "read",
                                 "invalid number of characters: " + option.value.value_or(""),
                                 {}});
                    return 1;
                }
                break;
            case 'p':
                prompt = option.value.value_or("");
                break;
            case 'd':
                delim = option.value.value_or("\n");
                break;
            case 't':
                try {
                    timeout_seconds = std::stod(option.value.value_or(""));
                } catch (const std::exception&) {
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 "read",
                                 "invalid timeout: " + option.value.value_or(""),
                                 {}});
                    return 1;
                }
                if (timeout_seconds < 0.0) {
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 "read",
                                 "invalid timeout: " + option.value.value_or(""),
                                 {}});
                    return 1;
                }
                has_timeout = true;
                break;
            default:
                break;
        }
    }

    std::vector<std::string> var_names(args.begin() + static_cast<long>(start_index), args.end());

    if (var_names.empty()) {
        var_names.push_back("REPLY");
    }

    if (!prompt.empty()) {
        std::cout << prompt << std::flush;
    }

    auto deadline = std::chrono::steady_clock::now();
    if (has_timeout) {
        deadline += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(timeout_seconds));
    }

    auto wait_for_input = [&]() -> bool {
        if (!has_timeout) {
            return true;
        }

        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            return false;
        }

        auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
        if (remaining < 0) {
            return false;
        }

        int timeout_ms =
            static_cast<int>(std::min<long long>(remaining, std::numeric_limits<int>::max()));
        struct pollfd pfd{STDIN_FILENO, POLLIN, 0};
        int result = 0;
        do {
            result = poll(&pfd, 1, timeout_ms);
        } while (result < 0 && errno == EINTR);
        return result > 0;
    };

    std::string input;
    char c = 0;
    int chars_read = 0;
    bool timed_out = false;

    auto read_char = [&](char& out) -> bool {
        if (!wait_for_input()) {
            timed_out = true;
            return false;
        }
        return static_cast<bool>(std::cin.get(out));
    };

    if (nchars > 0) {
        while (chars_read < nchars && read_char(c)) {
            input += c;
            chars_read++;
        }
    } else {
        while (read_char(c)) {
            if (delim.find(c) != std::string::npos) {
                break;
            }
            input += c;
        }
    }

    if (timed_out && input.empty()) {
        return 1;
    }

    if (std::cin.eof() && input.empty()) {
        return 1;
    }

    if (!raw_mode) {
        std::string processed;
        for (size_t i = 0; i < input.length(); ++i) {
            if (input[i] == '\\' && i + 1 < input.length()) {
                char next = input[i + 1];
                switch (next) {
                    case 'n':
                        processed += '\n';
                        i++;
                        break;
                    case 't':
                        processed += '\t';
                        i++;
                        break;
                    case 'r':
                        processed += '\r';
                        i++;
                        break;
                    case 'b':
                        processed += '\b';
                        i++;
                        break;
                    case 'a':
                        processed += '\a';
                        i++;
                        break;
                    case 'v':
                        processed += '\v';
                        i++;
                        break;
                    case 'f':
                        processed += '\f';
                        i++;
                        break;
                    case '\\':
                        processed += '\\';
                        i++;
                        break;
                    default:
                        processed += input[i];
                        break;
                }
            } else {
                processed += input[i];
            }
        }
        input = processed;
    }

    std::string ifs = cjsh_env::get_ifs_delimiters();

    std::vector<std::string> fields;

    if (ifs.empty()) {
        fields.push_back(input);
    } else {
        bool ifs_all_whitespace = true;
        for (char ifs_char : ifs) {
            if (ifs_char != ' ' && ifs_char != '\t' && ifs_char != '\n') {
                ifs_all_whitespace = false;
                break;
            }
        }

        if (ifs_all_whitespace) {
            size_t start = 0;

            while (start < input.length() && ifs.find(input[start]) != std::string::npos) {
                start++;
            }

            std::string current_field;
            for (size_t i = start; i < input.length(); ++i) {
                if (ifs.find(input[i]) != std::string::npos) {
                    if (!current_field.empty()) {
                        fields.push_back(current_field);
                        current_field.clear();
                    }
                } else {
                    current_field += input[i];
                }
            }
            if (!current_field.empty()) {
                fields.push_back(current_field);
            }
        } else {
            std::string current_field;
            for (size_t i = 0; i < input.length(); ++i) {
                if (ifs.find(input[i]) != std::string::npos) {
                    fields.push_back(current_field);
                    current_field.clear();
                } else {
                    current_field += input[i];
                }
            }
            fields.push_back(current_field);

            while (!fields.empty() && fields.front().empty()) {
                fields.erase(fields.begin());
            }
            while (!fields.empty() && fields.back().empty()) {
                fields.pop_back();
            }
        }
    }

    for (size_t i = 0; i < var_names.size(); ++i) {
        const std::string& var_name = var_names[i];

        if (!readonly_manager_can_assign(var_name, "read")) {
            return 1;
        }

        std::string value;
        if (i < fields.size()) {
            if (i == var_names.size() - 1 && fields.size() > var_names.size()) {
                for (size_t j = i; j < fields.size(); ++j) {
                    if (j > i)
                        value += " ";
                    value += fields[j];
                }
            } else {
                value = fields[i];
            }
        }

        if (!cjsh_env::set_shell_variable_value(var_name, value)) {
            print_error({ErrorType::FATAL_ERROR, "read", "shell not initialized properly", {}});
            return 1;
        }
    }

    cjsh_env::sync_parser_env_vars(shell);

    return 0;
}

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
    auto print_usage = []() {
        std::cout
            << "Usage: read [-r] [-p prompt] [-n nchars] [-d delim] [-t timeout] [name ...]\n";
        std::cout << "Read a line from standard input and split it into fields.\n\n";
        std::cout << "Options:\n";
        std::cout << "  -r            do not allow backslashes to escape any characters\n";
        std::cout << "  -p prompt     output PROMPT without a trailing newline before reading\n";
        std::cout << "  -n nchars     return after reading NCHARS characters rather than waiting "
                     "for a newline\n";
        std::cout << "  -d delim      continue until the first character of DELIM is read, rather "
                     "than newline\n";
        std::cout << "  -t timeout    time out after TIMEOUT seconds (fractional allowed)\n";
    };

    bool help_requested = false;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--help") {
            help_requested = true;
            break;
        }
    }

    if (help_requested) {
        print_usage();
        return 0;
    }

    if (shell == nullptr) {
        print_error({ErrorType::RUNTIME_ERROR, "read", "internal error - no shell context", {}});
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

    std::vector<std::string> var_names;

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (arg == "-r") {
            raw_mode = true;
        } else if (arg == "-n" && i + 1 < args.size()) {
            try {
                nchars = std::stoi(args[i + 1]);
                i++;
            } catch (const std::exception&) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             "read",
                             "invalid number of characters: " + args[i + 1],
                             {}});
                return 1;
            }
        } else if (arg.substr(0, 2) == "-n" && arg.length() > 2) {
            try {
                nchars = std::stoi(arg.substr(2));
            } catch (const std::exception&) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             "read",
                             "invalid number of characters: " + arg.substr(2),
                             {}});
                return 1;
            }
        } else if (arg == "-p" && i + 1 < args.size()) {
            prompt = args[i + 1];
            i++;
        } else if (arg.substr(0, 2) == "-p" && arg.length() > 2) {
            prompt = arg.substr(2);
        } else if (arg == "-d" && i + 1 < args.size()) {
            delim = args[i + 1];
            i++;
        } else if (arg.substr(0, 2) == "-d" && arg.length() > 2) {
            delim = arg.substr(2);
        } else if (arg == "-t" && i + 1 < args.size()) {
            try {
                timeout_seconds = std::stod(args[i + 1]);
                if (timeout_seconds < 0.0) {
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 "read",
                                 "invalid timeout: " + args[i + 1],
                                 {}});
                    return 1;
                }
                has_timeout = true;
                i++;
            } catch (const std::exception&) {
                print_error(
                    {ErrorType::INVALID_ARGUMENT, "read", "invalid timeout: " + args[i + 1], {}});
                return 1;
            }
        } else if (arg.substr(0, 2) == "-t" && arg.length() > 2) {
            try {
                timeout_seconds = std::stod(arg.substr(2));
                if (timeout_seconds < 0.0) {
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 "read",
                                 "invalid timeout: " + arg.substr(2),
                                 {}});
                    return 1;
                }
                has_timeout = true;
            } catch (const std::exception&) {
                print_error(
                    {ErrorType::INVALID_ARGUMENT, "read", "invalid timeout: " + arg.substr(2), {}});
                return 1;
            }
        } else if (arg == "-t") {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "read",
                         "timeout requires a value",
                         {"Try 'read --help' for more information."}});
            return 1;
        } else if (arg[0] == '-') {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "read",
                         "invalid option -- '" + arg + "'",
                         {"Try 'read --help' for more information."}});
            return 1;
        } else {
            var_names.push_back(arg);
        }
    }

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

    std::string ifs = " \t\n";
    const auto& env_vars = cjsh_env::env_vars();
    auto ifs_it = env_vars.find("IFS");
    if (ifs_it != env_vars.end()) {
        ifs = ifs_it->second;
    } else {
        const char* ifs_env = getenv("IFS");
        if (ifs_env != nullptr) {
            ifs = ifs_env;
        }
    }

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

        if (readonly_manager_is(var_name)) {
            print_error(
                {ErrorType::INVALID_ARGUMENT, "read", var_name + ": readonly variable", {}});
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

        if (setenv(var_name.c_str(), value.c_str(), 1) != 0) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "read",
                         std::string("failed to set ") + var_name + ": " + std::strerror(errno),
                         {}});
            return 1;
        }

        cjsh_env::env_vars()[var_name] = value;
    }

    cjsh_env::sync_parser_env_vars(shell);

    return 0;
}

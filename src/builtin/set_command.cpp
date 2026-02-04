/*
  set_command.cpp

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

#include "set_command.h"

#include "builtin_help.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

#include "cjsh.h"
#include "error_out.h"
#include "flags.h"
#include "shell.h"

namespace {

struct OptionDescriptor {
    char short_flag;
    const char* name;
};

constexpr size_t kOptionNamePadding = 15;

const OptionDescriptor kOptionDescriptors[] = {
    {'e', "errexit"},   {'C', "noclobber"}, {'u', "nounset"}, {'x', "xtrace"},
    {'v', "verbose"},   {'n', "noexec"},    {'f', "noglob"},  {0, "globstar"},
    {'a', "allexport"}, {0, "huponexit"},   {0, "pipefail"}};

std::string pad_option_name(const std::string& name) {
    if (name.size() >= kOptionNamePadding) {
        return name;
    }
    std::string padded = name;
    padded.append(kOptionNamePadding - name.size(), ' ');
    return padded;
}

void print_option_status(Shell* shell) {
    for (const auto& opt : kOptionDescriptors) {
        std::cout << pad_option_name(opt.name) << '\t'
                  << (shell->get_shell_option(opt.name) ? "on" : "off") << '\n';
    }
    std::cout << pad_option_name("errexit_severity") << '\t' << shell->get_errexit_severity()
              << '\n';
}

bool apply_short_flag(char flag, bool enable, Shell* shell) {
    switch (flag) {
        case 'e':
            shell->set_shell_option("errexit", enable);
            return true;
        case 'C':
            shell->set_shell_option("noclobber", enable);
            return true;
        case 'u':
            shell->set_shell_option("nounset", enable);
            return true;
        case 'x':
            shell->set_shell_option("xtrace", enable);
            return true;
        case 'v':
            shell->set_shell_option("verbose", enable);
            return true;
        case 'n':
            shell->set_shell_option("noexec", enable);
            return true;
        case 'f':
            shell->set_shell_option("noglob", enable);
            return true;
        case 'a':
            shell->set_shell_option("allexport", enable);
            return true;
        default:
            return false;
    }
}

std::string normalize_option_key(std::string key) {
    if (key.empty()) {
        return key;
    }

    size_t first_non_dash = key.find_first_not_of('-');
    if (first_non_dash == std::string::npos) {
        key.clear();
    } else if (first_non_dash > 0) {
        key.erase(0, first_non_dash);
    }

    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::replace(key.begin(), key.end(), '-', '_');
    return key;
}

bool handle_named_option(const std::string& raw_option, bool enable, Shell* shell) {
    size_t eq_pos = raw_option.find('=');
    std::string option_key =
        (eq_pos == std::string::npos) ? raw_option : raw_option.substr(0, eq_pos);
    std::string option_value = (eq_pos == std::string::npos || eq_pos + 1 >= raw_option.size())
                                   ? ""
                                   : raw_option.substr(eq_pos + 1);

    std::string normalized_key = normalize_option_key(option_key);
    if (normalized_key.empty()) {
        return false;
    }

    if (normalized_key == "errexit_severity") {
        shell->set_errexit_severity(option_value);
        return true;
    }

    if (eq_pos != std::string::npos) {
        return false;
    }

    for (const auto& opt : kOptionDescriptors) {
        if (normalized_key == opt.name) {
            shell->set_shell_option(opt.name, enable);
            return true;
        }
    }

    return false;
}

bool handle_long_errexit_severity(const std::vector<std::string>& args, size_t& index,
                                  Shell* shell) {
    const std::string& arg = args[index];
    const std::string hyphenated_prefix = "--errexit-severity";
    const std::string underscored_prefix = "--errexit_severity";

    auto matches_prefix = [&](const std::string& prefix) {
        if (arg.rfind(prefix, 0) != 0) {
            return false;
        }

        std::string value;
        if (arg.size() > prefix.size() && arg[prefix.size()] == '=') {
            value = arg.substr(prefix.size() + 1);
        } else if (arg.size() == prefix.size()) {
            if (index + 1 < args.size()) {
                value = args[++index];
            } else {
                value.clear();
            }
        } else {
            return false;
        }

        shell->set_errexit_severity(value);
        return true;
    };

    return matches_prefix(hyphenated_prefix) || matches_prefix(underscored_prefix);
}

void report_invalid_option(const std::string& context) {
    print_error({ErrorType::INVALID_ARGUMENT, "set", "option '" + context + "' not supported", {}});
}

}  // namespace

int set_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(
            args, {"Usage: set [-+eCunxvfna] [-o option] [--] [ARG ...]",
                   "Set or unset shell options and positional parameters.",
                   "",
                   "Options:",
                   "  -e              Exit on error (errexit)",
                   "  -C              Prevent file overwriting (noclobber)",
                   "  -u              Treat unset variables as error (nounset)",
                   "  -x              Print commands before execution (xtrace)",
                   "  -v              Print input lines as they are read (verbose)",
                   "  -n              Read but don't execute commands (noexec)",
                   "  -f              Disable pathname expansion (noglob)",
                   "  -a              Auto-export modified variables (allexport)",
                   "  -o option       Set option by name (globstar, huponexit, pipefail, etc.)",
                   "                  globstar enables recursive '**' globs",
                   "                  pipefail makes pipelines return the last non-zero status",
                   "  +<option>       Unset the specified option",
                   "  --              End options; remaining args set $1, $2, etc.",
                   "",
                   "With no arguments, print all environment variables.",
                   "Use 'set -o' to list current option settings.",
                   "",
                   "Special options:",
                   "  --errexit-severity=LEVEL  Set errexit sensitivity level"})) {
        return 0;
    }
    if (shell == nullptr) {
        print_error({ErrorType::RUNTIME_ERROR, "set", "shell not available", {}});
        return 1;
    }

    if (args.size() == 1) {
        extern char** environ;
        for (char** env = environ; *env != nullptr; ++env) {
            std::cout << *env << '\n';
        }
        return 0;
    }

    bool parsing_options = true;
    bool positional_specified = false;
    std::vector<std::string> positional_params;
    positional_params.reserve(args.size());

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (parsing_options) {
            if (arg == "--") {
                parsing_options = false;
                positional_specified = true;
                continue;
            }

            if (!arg.empty() && arg[0] == '-' && handle_long_errexit_severity(args, i, shell)) {
                continue;
            }

            if (!arg.empty() && (arg[0] == '-' || arg[0] == '+')) {
                if (arg.size() == 1) {
                    parsing_options = false;
                } else if (arg[1] == 'o') {
                    bool enable_option = arg[0] == '-';
                    std::string option_name;

                    if (arg.size() == 2) {
                        if (i + 1 >= args.size()) {
                            print_option_status(shell);
                            return 0;
                        }
                        option_name = args[++i];
                    } else {
                        option_name = arg.substr(2);
                    }

                    if (option_name.empty()) {
                        report_invalid_option(arg);
                        return 1;
                    }

                    std::string normalized_key = normalize_option_key(option_name);
                    bool inline_value = option_name.find('=') != std::string::npos;

                    if (normalized_key == "errexit_severity" && !inline_value) {
                        std::string severity_value;
                        if (i + 1 < args.size()) {
                            severity_value = args[++i];
                        }
                        shell->set_errexit_severity(severity_value);
                        continue;
                    }

                    if (!handle_named_option(option_name, enable_option, shell)) {
                        report_invalid_option(option_name);
                        return 1;
                    }
                    continue;
                } else {
                    std::string flags = arg.substr(1);
                    bool enable_flag = arg[0] == '-';
                    bool ok = true;
                    for (char flag : flags) {
                        if (!apply_short_flag(flag, enable_flag, shell)) {
                            report_invalid_option(std::string(1, arg[0]) + flag);
                            ok = false;
                            break;
                        }
                    }
                    if (!ok) {
                        return 1;
                    }
                    continue;
                }
            }
        }

        parsing_options = false;
        positional_specified = true;
        positional_params.push_back(arg);
    }

    if (positional_specified) {
        flags::set_positional_parameters(positional_params);
    }

    return 0;
}

int shift_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(
            args, {"Usage: shift [N]", "Discard the first N positional parameters (default 1)."})) {
        return 0;
    }
    if (shell == nullptr) {
        print_error({ErrorType::RUNTIME_ERROR, "shift", "shell not available", {}});
        return 1;
    }

    int shift_count = 1;

    if (args.size() > 1) {
        try {
            shift_count = std::stoi(args[1]);
            if (shift_count < 0) {
                print_error({ErrorType::INVALID_ARGUMENT, "shift", "negative shift count", {}});
                return 1;
            }
        } catch (const std::exception&) {
            print_error(
                {ErrorType::INVALID_ARGUMENT, "shift", "invalid shift count: " + args[1], {}});
            return 1;
        }
    }

    return flags::shift_positional_parameters(shift_count);
}

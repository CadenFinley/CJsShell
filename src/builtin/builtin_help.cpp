/*
  builtin_help.cpp

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

#include "builtin_help.h"

#include <iostream>

#include "shell_env.h"

bool builtin_handle_help(const std::vector<std::string>& args,
                         const std::vector<std::string>& help_lines,
                         BuiltinHelpScanMode scan_mode) {
    if (args.size() <= 1) {
        return false;
    }

    auto should_print_help = [&](const std::string& flag) {
        return flag == "--help" || flag == "-h";
    };

    bool help_requested = false;
    if (scan_mode == BuiltinHelpScanMode::FirstArgument) {
        help_requested = should_print_help(args[1]);
    } else {
        for (size_t i = 1; i < args.size(); ++i) {
            if (should_print_help(args[i])) {
                help_requested = true;
                break;
            }
        }
    }

    if (!help_requested) {
        return false;
    }

    for (const auto& line : help_lines) {
        std::cout << line << '\n';
    }
    return true;
}

bool builtin_handle_help_with_startup_guard(const std::vector<std::string>& args,
                                            const std::vector<std::string>& help_lines,
                                            BuiltinHelpScanMode scan_mode) {
    if (args.size() <= 1) {
        return false;
    }

    auto is_help_flag = [](const std::string& flag) { return flag == "--help" || flag == "-h"; };

    bool help_requested = false;
    if (scan_mode == BuiltinHelpScanMode::FirstArgument) {
        help_requested = is_help_flag(args[1]);
    } else {
        for (size_t i = 1; i < args.size(); ++i) {
            if (is_help_flag(args[i])) {
                help_requested = true;
                break;
            }
        }
    }

    if (!help_requested) {
        return false;
    }

    if (!cjsh_env::startup_active()) {
        for (const auto& line : help_lines) {
            std::cout << line << '\n';
        }
    }

    return true;
}

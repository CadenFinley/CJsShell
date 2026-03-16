/*
  command_line_utils.h

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

#pragma once

#include <string>
#include <vector>

namespace command_line_utils {

inline std::vector<std::string> tokenize_shell_words(const std::string& command,
                                                     bool preserve_quotes_and_escapes = false) {
    std::vector<std::string> args;
    std::string current;
    bool in_single_quote = false;
    bool in_double_quote = false;
    bool escaped = false;

    for (char c : command) {
        if (escaped) {
            current += c;
            escaped = false;
            continue;
        }

        if (c == '\\') {
            if (in_single_quote) {
                current += c;
            } else {
                if (preserve_quotes_and_escapes) {
                    current += c;
                }
                escaped = true;
            }
            continue;
        }

        if (c == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            if (preserve_quotes_and_escapes) {
                current += c;
            }
            continue;
        }

        if (c == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            if (preserve_quotes_and_escapes) {
                current += c;
            }
            continue;
        }

        if ((c == ' ' || c == '\t') && !in_single_quote && !in_double_quote) {
            if (!current.empty()) {
                args.push_back(current);
                current.clear();
            }
            continue;
        }

        current += c;
    }

    if (!current.empty()) {
        args.push_back(current);
    }

    return args;
}

}  // namespace command_line_utils

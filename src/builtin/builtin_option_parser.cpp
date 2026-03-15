/*
  builtin_option_parser.cpp

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

#include "builtin_option_parser.h"

#include "error_out.h"

bool builtin_parse_short_options(const std::vector<std::string>& args, size_t& start_index,
                                 const std::string& command_name,
                                 const std::function<bool(char)>& handle_option,
                                 bool require_option_character) {
    start_index = 1;
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& option = args[i];
        if (option == "--") {
            start_index = i + 1;
            break;
        }

        if (option.empty() || option[0] != '-') {
            break;
        }

        if (require_option_character && option.size() <= 1) {
            break;
        }

        if (!require_option_character && option.size() == 1) {
            break;
        }

        for (size_t j = 1; j < option.size(); ++j) {
            if (!handle_option(option[j])) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             command_name,
                             "invalid option: -" + std::string(1, option[j]),
                             {}});
                return false;
            }
        }
        start_index = i + 1;
    }

    return true;
}

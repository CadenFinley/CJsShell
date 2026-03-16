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

#include <utility>

#include "error_out.h"

bool builtin_parse_short_options_ex(const std::vector<std::string>& args, size_t& start_index,
                                    const std::string& command_name,
                                    const std::function<bool(char)>& is_valid_option,
                                    const std::function<bool(char)>& option_requires_value,
                                    std::vector<BuiltinParsedShortOption>& parsed_options,
                                    bool require_option_character, bool passthrough_long_options) {
    start_index = 1;
    parsed_options.clear();

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& option = args[i];
        if (option == "--") {
            start_index = i + 1;
            break;
        }

        if (passthrough_long_options && option.rfind("--", 0) == 0 && option.size() > 2) {
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
            const char opt = option[j];
            if (!is_valid_option(opt)) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             command_name,
                             "invalid option: -" + std::string(1, opt),
                             {}});
                return false;
            }

            if (option_requires_value(opt)) {
                BuiltinParsedShortOption parsed;
                parsed.option = opt;

                if (j + 1 < option.size()) {
                    parsed.value = option.substr(j + 1);
                } else if (i + 1 < args.size()) {
                    parsed.value = args[i + 1];
                    ++i;
                } else {
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 command_name,
                                 "option requires an argument: -" + std::string(1, opt),
                                 {}});
                    return false;
                }

                parsed_options.push_back(std::move(parsed));
                break;
            }

            parsed_options.push_back(BuiltinParsedShortOption{opt, std::nullopt});
        }

        start_index = i + 1;
    }

    return true;
}

bool builtin_parse_short_options(const std::vector<std::string>& args, size_t& start_index,
                                 const std::string& command_name,
                                 const std::function<bool(char)>& handle_option,
                                 bool require_option_character) {
    std::vector<BuiltinParsedShortOption> parsed_options;
    const bool ok = builtin_parse_short_options_ex(
        args, start_index, command_name, handle_option, [](char) { return false; }, parsed_options,
        require_option_character);
    if (!ok) {
        return false;
    }

    for (const auto& option : parsed_options) {
        if (!handle_option(option.option)) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         command_name,
                         "invalid option: -" + std::string(1, option.option),
                         {}});
            return false;
        }
    }

    return true;
}

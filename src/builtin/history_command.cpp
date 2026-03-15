/*
  history_command.cpp

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

#include "history_command.h"

#include "builtin_help.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "cjsh_filesystem.h"
#include "error_out.h"

int history_command(const std::vector<std::string>& args) {
    auto run = [&]() -> int {
        if (builtin_handle_help(
                args, {"Usage: history [COUNT]",
                       "Display command history, optionally limiting to COUNT entries."})) {
            return 0;
        }
        cjsh_filesystem::initialize_cjsh_directories();

        auto read_result =
            cjsh_filesystem::read_file_content(cjsh_filesystem::g_cjsh_history_path().string());

        std::string content;
        if (read_result.is_error()) {
            auto write_result = cjsh_filesystem::write_file_content(
                cjsh_filesystem::g_cjsh_history_path().string(), "");
            if (write_result.is_error()) {
                print_error({ErrorType::RUNTIME_ERROR,
                             "history",
                             "could not create history file at " +
                                 cjsh_filesystem::g_cjsh_history_path().string() + ": " +
                                 write_result.error(),
                             {}});
                return 1;
            }
            content = "";
        } else {
            content = read_result.value();
        }

        std::stringstream content_stream(content);
        std::string line;
        std::vector<std::string> entries;
        entries.reserve(256);

        while (std::getline(content_stream, line)) {
            if (line.empty()) {
                continue;
            }
            if (!line.empty() && line[0] == '#') {
                continue;
            }
            entries.push_back(line);
        }

        int limit = static_cast<int>(entries.size());
        if (args.size() > 1) {
            if (!args[1].empty() && args[1][0] == '-' &&
                (args[1].size() == 1 || !std::isdigit(static_cast<unsigned char>(args[1][1])))) {
                print_error(
                    {ErrorType::INVALID_ARGUMENT, "history", "invalid option: " + args[1], {}});
                return 2;
            }
            try {
                limit = std::stoi(args[1]);
            } catch (const std::invalid_argument&) {
                print_error(
                    {ErrorType::INVALID_ARGUMENT, "history", "invalid argument: " + args[1], {}});
                return 1;
            } catch (const std::out_of_range&) {
                print_error(
                    {ErrorType::INVALID_ARGUMENT, "history", "invalid argument: " + args[1], {}});
                return 1;
            }

            if (limit < 0) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             "history",
                             "COUNT must be a non-negative integer",
                             {}});
                return 1;
            }

            limit = std::min(limit, static_cast<int>(entries.size()));
        }

        for (int i = 0; i < limit; ++i) {
            std::cout << std::setw(5) << i << "  " << entries[static_cast<size_t>(i)] << '\n';
        }

        return 0;
    };

    try {
        return run();
    } catch (...) {
        print_error({ErrorType::INVALID_ARGUMENT, "history", "invalid argument", {}});
        return 1;
    }
}

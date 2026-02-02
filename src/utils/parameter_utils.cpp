/*
  parameter_utils.cpp

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

#include "utils/parameter_utils.h"

#include <unistd.h>
#include <cstdlib>
#include <string>

#include "job_control.h"
#include "shell.h"

namespace parameter_utils {

std::string join_positional_parameters(const Shell* shell) {
    if (shell == nullptr) {
        return "";
    }

    const auto params = shell->get_positional_parameters();
    if (params.empty()) {
        return "";
    }

    size_t total_length = 0;
    for (const auto& param : params) {
        total_length += param.size();
    }
    if (params.size() > 1) {
        total_length += params.size() - 1;
    }

    std::string joined;
    joined.reserve(total_length);

    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) {
            joined.push_back(' ');
        }
        joined += params[i];
    }
    return joined;
}

std::string get_last_background_pid_string() {
    const char* last_bg_pid = getenv("!");
    if (last_bg_pid != nullptr) {
        return last_bg_pid;
    }

    pid_t last_pid = JobManager::instance().get_last_background_pid();
    if (last_pid > 0) {
        return std::to_string(last_pid);
    }
    return "";
}

}  // namespace parameter_utils

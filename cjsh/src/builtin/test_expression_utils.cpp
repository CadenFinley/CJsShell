/*
  test_expression_utils.cpp

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

#include "test_expression_utils.h"

#include <sys/stat.h>
#include <unistd.h>

namespace test_expression_utils {

bool is_basic_unary_operator(const std::string& op) {
    return op == "-z" || op == "-n" || op == "-e" || op == "-f" || op == "-d" || op == "-r" ||
           op == "-w" || op == "-x" || op == "-s";
}

bool evaluate_basic_unary_operator(const std::string& op, const std::string& arg) {
    struct stat st;
    if (op == "-z") {
        return arg.empty();
    }
    if (op == "-n") {
        return !arg.empty();
    }
    if (op == "-e") {
        return access(arg.c_str(), F_OK) == 0;
    }
    if (op == "-f") {
        return stat(arg.c_str(), &st) == 0 && S_ISREG(st.st_mode);
    }
    if (op == "-d") {
        return stat(arg.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }
    if (op == "-r") {
        return access(arg.c_str(), R_OK) == 0;
    }
    if (op == "-w") {
        return access(arg.c_str(), W_OK) == 0;
    }
    if (op == "-x") {
        return access(arg.c_str(), X_OK) == 0;
    }
    if (op == "-s") {
        return stat(arg.c_str(), &st) == 0 && st.st_size > 0;
    }
    return false;
}

bool is_numeric_comparison_operator(const std::string& op) {
    return op == "-eq" || op == "-ne" || op == "-lt" || op == "-le" || op == "-gt" || op == "-ge";
}

std::optional<bool> evaluate_numeric_comparison(const std::string& left, const std::string& op,
                                                const std::string& right) {
    if (!is_numeric_comparison_operator(op)) {
        return std::nullopt;
    }

    try {
        long long left_val = std::stoll(left);
        long long right_val = std::stoll(right);

        if (op == "-eq") {
            return left_val == right_val;
        }
        if (op == "-ne") {
            return left_val != right_val;
        }
        if (op == "-lt") {
            return left_val < right_val;
        }
        if (op == "-le") {
            return left_val <= right_val;
        }
        if (op == "-gt") {
            return left_val > right_val;
        }
        return left_val >= right_val;
    } catch (...) {
        return false;
    }
}

}  // namespace test_expression_utils

/*
  numeric_utils.cpp

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

#include "numeric_utils.h"

#include <cerrno>
#include <climits>
#include <cstdlib>

namespace numeric_utils {

bool parse_long_strict(std::string_view text, long& value_out) {
    if (text.empty()) {
        return false;
    }

    errno = 0;
    char* endptr = nullptr;
    std::string owned(text);
    long value = std::strtol(owned.c_str(), &endptr, 10);
    if (errno == ERANGE || endptr == owned.c_str() || *endptr != '\0') {
        return false;
    }

    value_out = value;
    return true;
}

bool parse_int_strict(std::string_view text, int& value_out) {
    long parsed = 0;
    if (!parse_long_strict(text, parsed)) {
        return false;
    }

    if (parsed < static_cast<long>(INT_MIN) || parsed > static_cast<long>(INT_MAX)) {
        return false;
    }

    value_out = static_cast<int>(parsed);
    return true;
}

bool parse_int_in_range(std::string_view text, int min_value, int max_value, int& value_out) {
    int parsed = 0;
    if (!parse_int_strict(text, parsed)) {
        return false;
    }

    if (parsed < min_value || parsed > max_value) {
        return false;
    }

    value_out = parsed;
    return true;
}

int parse_exit_status_or(std::string_view text, int fallback, bool mask_to_byte) {
    long parsed = 0;
    if (!parse_long_strict(text, parsed)) {
        return fallback;
    }

    int value = static_cast<int>(parsed);
    if (mask_to_byte) {
        value &= 0xFF;
    }
    return value;
}

}  // namespace numeric_utils

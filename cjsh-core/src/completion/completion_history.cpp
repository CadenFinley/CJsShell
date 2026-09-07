/*
  completion_history.cpp

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

#include "completion_history.h"

#include <string>

#include "cjsh_filesystem.h"
#include "isocline.h"
#include "shell_env.h"

namespace completion_history {

namespace {
constexpr long kHistoryMinEntries = 0;
constexpr long kHistoryDefaultEntries = 1000;

long g_history_max_entries_value = kHistoryDefaultEntries;

}  // namespace

bool enforce_history_limit(std::string* error_message) {
    (void)error_message;
    if (!config::history_enabled || !config::history_persistence_enabled) {
        ic_set_history(nullptr, 0);
        return true;
    }

    if (g_history_max_entries_value <= 0) {
        // Clear through the same transaction lock used by all history writers.
        ic_set_history(cjsh_filesystem::g_cjsh_history_path().c_str(), 1);
        ic_history_clear();
        ic_set_history(nullptr, 0);
        return true;
    }

    ic_set_history(cjsh_filesystem::g_cjsh_history_path().c_str(), g_history_max_entries_value);
    return true;
}

bool set_history_max_entries(long max_entries, std::string* error_message) {
    long resolved = max_entries;
    if (max_entries < 0) {
        if (max_entries == -1) {
            resolved = kHistoryDefaultEntries;
        } else {
            if (error_message != nullptr) {
                *error_message = "History limit must be zero or greater.";
            }
            return false;
        }
    }

    long previous_limit = g_history_max_entries_value;
    g_history_max_entries_value = resolved;

    if (!enforce_history_limit(error_message)) {
        g_history_max_entries_value = previous_limit;
        (void)enforce_history_limit(nullptr);
        return false;
    }

    return true;
}

long get_history_max_entries() {
    return g_history_max_entries_value;
}

long get_history_default_history_limit() {
    return kHistoryDefaultEntries;
}

long get_history_min_history_limit() {
    return kHistoryMinEntries;
}

}  // namespace completion_history

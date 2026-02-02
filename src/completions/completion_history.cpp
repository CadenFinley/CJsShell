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

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "cjsh_filesystem.h"
#include "isocline.h"

namespace completion_history {

namespace {
constexpr long kHistoryMinEntries = 0;
constexpr long kHistoryDefaultEntries = 1000;

long g_history_max_entries_value = kHistoryDefaultEntries;

struct SerializedHistoryEntry {
    std::string timestamp;
    std::string payload;
};

bool trim_history_file(long max_entries, std::string* error_message) {
    if (max_entries < 0) {
        return true;
    }

    const auto& history_path = cjsh_filesystem::g_cjsh_history_path();

    if (max_entries == 0) {
        std::error_code remove_ec;
        std::filesystem::remove(history_path, remove_ec);
        if (remove_ec && remove_ec != std::errc::no_such_file_or_directory) {
            if (error_message != nullptr) {
                *error_message = "Failed to remove history file '" + history_path.string() +
                                 "': " + remove_ec.message();
            }
            return false;
        }
        return true;
    }

    std::error_code exists_ec;
    if (!std::filesystem::exists(history_path, exists_ec)) {
        if (exists_ec) {
            if (error_message != nullptr) {
                *error_message = "Failed to inspect history file '" + history_path.string() +
                                 "': " + exists_ec.message();
            }
            return false;
        }
        return true;
    }

    std::ifstream history_stream(history_path);
    if (!history_stream.is_open()) {
        if (error_message != nullptr) {
            *error_message =
                "Failed to open history file '" + history_path.string() + "' for reading.";
        }
        return false;
    }

    std::vector<SerializedHistoryEntry> entries;

    std::string line;
    SerializedHistoryEntry current;
    bool seen_timestamp = false;

    while (std::getline(history_stream, line)) {
        if (!line.empty() && line[0] == '#') {
            if (seen_timestamp && !current.timestamp.empty()) {
                entries.push_back(current);
                current = SerializedHistoryEntry{};
            }
            current.timestamp = line;
            current.payload.clear();
            seen_timestamp = true;
        } else {
            if (!seen_timestamp) {
                continue;
            }
            if (!current.payload.empty()) {
                current.payload += '\n';
            }
            current.payload += line;
        }
    }

    if (seen_timestamp && !current.timestamp.empty()) {
        entries.push_back(current);
    }

    history_stream.close();

    if (entries.size() <= static_cast<size_t>(max_entries)) {
        return true;
    }

    size_t start_index = entries.size() - static_cast<size_t>(max_entries);
    std::ostringstream buffer;

    for (size_t i = start_index; i < entries.size(); ++i) {
        buffer << entries[i].timestamp << '\n';
        if (!entries[i].payload.empty()) {
            buffer << entries[i].payload;
        }
        buffer << '\n';
    }

    auto write_result = cjsh_filesystem::write_file_content(history_path.string(), buffer.str());
    if (write_result.is_error()) {
        if (error_message != nullptr) {
            *error_message = write_result.error();
        }
        return false;
    }

    return true;
}
}  // namespace

bool enforce_history_limit(std::string* error_message) {
    if (g_history_max_entries_value <= 0) {
        ic_set_history(nullptr, 0);
        return trim_history_file(0, error_message);
    }

    ic_set_history(cjsh_filesystem::g_cjsh_history_path().c_str(), g_history_max_entries_value);
    return trim_history_file(g_history_max_entries_value, error_message);
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
        enforce_history_limit(nullptr);
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

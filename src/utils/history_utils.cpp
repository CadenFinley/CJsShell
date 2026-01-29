#include "history_utils.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "isocline/isocline.h"

namespace history_utils {

namespace {

bool decode_hex_pair(char high, char low, char* result) {
    auto hex_value = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F')
            return 10 + (c - 'A');
        return -1;
    };

    int hi = hex_value(high);
    int lo = hex_value(low);
    if (hi < 0 || lo < 0)
        return false;
    *result = static_cast<char>((hi << 4) | lo);
    return true;
}

bool parse_metadata_line(const std::string& line, std::time_t* timestamp, int* exit_code) {
    if (line.empty() || line[0] != '#')
        return false;

    const char* cursor = line.c_str() + 1;
    while (*cursor == ' ' || *cursor == '\t')
        ++cursor;

    if (*cursor == '\0')
        return false;

    char* endptr = nullptr;
    long long ts_value = std::strtoll(cursor, &endptr, 10);
    if (endptr == cursor)
        return false;

    *timestamp = static_cast<std::time_t>(ts_value);
    cursor = endptr;
    while (*cursor == ' ' || *cursor == '\t')
        ++cursor;

    *exit_code = IC_HISTORY_EXIT_CODE_UNKNOWN;
    if (*cursor != '\0' && *cursor != '\n' && *cursor != '\r') {
        long exit_value = std::strtol(cursor, &endptr, 10);
        if (endptr != cursor) {
            *exit_code = static_cast<int>(exit_value);
        }
    }

    return true;
}

struct RuntimeCollector {
    std::vector<HistoryRecord>* records;
};

bool runtime_callback(const char* command, int exit_code, std::time_t timestamp, void* ctx) {
    auto* collector = static_cast<RuntimeCollector*>(ctx);
    HistoryRecord record;
    record.command = command ? command : "";
    record.exit_code = exit_code;
    record.timestamp = timestamp;
    collector->records->push_back(std::move(record));
    return true;
}

bool load_from_runtime(std::vector<HistoryRecord>* out) {
    RuntimeCollector collector{out};
    if (!ic_history_visit_entries(runtime_callback, &collector))
        return false;
    return !out->empty();
}

bool load_from_file(std::vector<HistoryRecord>* out) {
    cjsh_filesystem::initialize_cjsh_directories();
    std::ifstream history_file(cjsh_filesystem::g_cjsh_history_path());
    if (!history_file.is_open())
        return false;

    std::string line;
    std::string decoded;
    decoded.reserve(256);

    std::time_t pending_timestamp = 0;
    int pending_exit_code = IC_HISTORY_EXIT_CODE_UNKNOWN;
    bool has_pending_metadata = false;

    while (std::getline(history_file, line)) {
        if (line.empty())
            continue;

        if (line[0] == '#') {
            if (parse_metadata_line(line, &pending_timestamp, &pending_exit_code)) {
                has_pending_metadata = true;
                continue;
            }
            has_pending_metadata = false;
            pending_timestamp = 0;
            pending_exit_code = IC_HISTORY_EXIT_CODE_UNKNOWN;
            continue;
        }

        decoded.clear();
        if (!decode_history_line(line, decoded)) {
            decoded = line;
        }

        HistoryRecord record;
        record.command = decoded;
        record.timestamp = has_pending_metadata ? pending_timestamp : 0;
        record.exit_code = has_pending_metadata ? pending_exit_code : IC_HISTORY_EXIT_CODE_UNKNOWN;
        out->push_back(std::move(record));

        has_pending_metadata = false;
        pending_timestamp = 0;
        pending_exit_code = IC_HISTORY_EXIT_CODE_UNKNOWN;
    }

    return !out->empty();
}

}  // namespace

bool decode_history_line(const std::string& raw, std::string& decoded) {
    decoded.clear();
    decoded.reserve(raw.size());

    for (size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (c == '\\') {
            if (i + 1 >= raw.size())
                return false;
            char next = raw[++i];
            switch (next) {
                case 'n':
                    decoded.push_back('\n');
                    break;
                case 'r':
                    decoded.push_back('\r');
                    break;
                case 't':
                    decoded.push_back('\t');
                    break;
                case '\\':
                    decoded.push_back('\\');
                    break;
                case 'x': {
                    if (i + 2 >= raw.size())
                        return false;
                    char value = 0;
                    if (!decode_hex_pair(raw[i + 1], raw[i + 2], &value))
                        return false;
                    decoded.push_back(value);
                    i += 2;
                    break;
                }
                default:
                    return false;
            }
        } else if (c == '\r') {
            continue;
        } else {
            decoded.push_back(c);
        }
    }

    return true;
}

std::vector<HistoryRecord> load_history_records() {
    const bool prefer_runtime_cache = ic_history_single_io_enabled();
    std::vector<HistoryRecord> records;
    if (prefer_runtime_cache) {
        if (load_from_runtime(&records)) {
            return records;
        }
        records.clear();
    }

    load_from_file(&records);
    return records;
}

std::vector<std::string> commands_from_records(const std::vector<HistoryRecord>& records) {
    std::vector<std::string> commands;
    commands.reserve(records.size());
    for (const auto& record : records) {
        commands.push_back(record.command);
    }
    return commands;
}

}  // namespace history_utils

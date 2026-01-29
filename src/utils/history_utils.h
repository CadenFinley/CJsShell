#pragma once

#include <ctime>
#include <string>
#include <vector>

namespace history_utils {

struct HistoryRecord {
    std::string command;
    int exit_code;
    std::time_t timestamp;
};

bool decode_history_line(const std::string& raw, std::string& decoded);
std::vector<HistoryRecord> load_history_records();
std::vector<std::string> commands_from_records(const std::vector<HistoryRecord>& records);

}  // namespace history_utils

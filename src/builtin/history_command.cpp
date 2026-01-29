#include "history_command.h"

#include "builtin_help.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "history_utils.h"

int history_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args,
                            {"Usage: history [COUNT]",
                             "Display command history, optionally limiting to COUNT entries."})) {
        return 0;
    }
    cjsh_filesystem::initialize_cjsh_directories();

    auto records = history_utils::load_history_records();
    if (records.empty()) {
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
    }

    std::vector<std::string> entries;
    entries.reserve(records.size());
    for (const auto& record : records) {
        entries.push_back(record.command);
    }

    int limit = static_cast<int>(entries.size());
    if (args.size() > 1) {
        try {
            limit = std::stoi(args[1]);
        } catch (const std::invalid_argument&) {
            print_error({ErrorType::INVALID_ARGUMENT, "history", "Invalid index: " + args[1], {}});
            return 1;
        } catch (const std::out_of_range&) {
            print_error(
                {ErrorType::INVALID_ARGUMENT, "history", "Index out of range: " + args[1], {}});
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
        std::cout << std::setw(5) << i << "  " << entries[i] << '\n';
    }

    return 0;
}

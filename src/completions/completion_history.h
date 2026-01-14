#pragma once

#include <string>

namespace completion_history {

bool set_history_max_entries(long max_entries, std::string* error_message = nullptr);
long get_history_max_entries();
long get_history_default_history_limit();
long get_history_min_history_limit();
bool enforce_history_limit(std::string* error_message = nullptr);

}  // namespace completion_history

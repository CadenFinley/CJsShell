#include "time_info.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace {
std::tm current_local_tm() {
    auto now = std::chrono::system_clock::now();
    std::time_t time_t = std::chrono::system_clock::to_time_t(now);
    if (std::tm* tm_ptr = std::localtime(&time_t)) {
        return *tm_ptr;
    }
    return {};
}

std::string format_current_tm(const char* format) {
    const auto tm = current_local_tm();
    std::ostringstream oss;
    oss << std::put_time(&tm, format);
    return oss.str();
}
}  // namespace

std::string get_current_time(bool twelve_hour_format) {
    return format_current_tm(twelve_hour_format ? "%I:%M:%S %p" : "%H:%M:%S");
}

std::string get_current_date() {
    return format_current_tm("%Y-%m-%d");
}

int get_current_day() {
    const auto tm = current_local_tm();
    return tm.tm_mday;
}

int get_current_month() {
    const auto tm = current_local_tm();
    return tm.tm_mon + 1;
}

int get_current_year() {
    const auto tm = current_local_tm();
    return tm.tm_year + 1900;
}

std::string get_current_day_name() {
    return format_current_tm("%A");
}

std::string get_current_month_name() {
    return format_current_tm("%B");
}
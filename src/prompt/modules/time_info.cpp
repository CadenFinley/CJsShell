#include "time_info.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

std::string TimeInfo::get_current_time(bool twelve_hour_format) {
    auto now = std::chrono::system_clock::now();
    std::time_t time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time_t);

    std::ostringstream oss;
    if (twelve_hour_format) {
        oss << std::put_time(tm, "%I:%M:%S %p");
    } else {
        oss << std::put_time(tm, "%H:%M:%S");
    }
    return oss.str();
}

std::string TimeInfo::get_current_date() {
    auto now = std::chrono::system_clock::now();
    std::time_t time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time_t);

    std::ostringstream oss;
    oss << std::put_time(tm, "%Y-%m-%d");
    return oss.str();
}

int TimeInfo::get_current_day() {
    auto now = std::chrono::system_clock::now();
    std::time_t time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time_t);
    return tm->tm_mday;
}

int TimeInfo::get_current_month() {
    auto now = std::chrono::system_clock::now();
    std::time_t time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time_t);
    return tm->tm_mon + 1;
}

int TimeInfo::get_current_year() {
    auto now = std::chrono::system_clock::now();
    std::time_t time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time_t);
    return tm->tm_year + 1900;
}

std::string TimeInfo::get_current_day_name() {
    auto now = std::chrono::system_clock::now();
    std::time_t time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time_t);

    std::ostringstream oss;
    oss << std::put_time(tm, "%A");
    return oss.str();
}

std::string TimeInfo::get_current_month_name() {
    auto now = std::chrono::system_clock::now();
    std::time_t time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time_t);

    std::ostringstream oss;
    oss << std::put_time(tm, "%B");
    return oss.str();
}
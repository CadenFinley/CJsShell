#pragma once

#include <string>

class TimeInfo {
 public:
  TimeInfo() = default;
  ~TimeInfo() = default;
  
  std::string get_current_time(bool twelve_hour_format = false);
  std::string get_current_date();
  int get_current_day();
  int get_current_month();
  int get_current_year();
  std::string get_current_day_name();
  std::string get_current_month_name();
};
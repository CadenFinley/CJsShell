#include "times_command.h"
#include <sys/times.h>
#include <unistd.h>
#include <iomanip>
#include <iostream>
#include <sstream>

int times_command(const std::vector<std::string>& args, Shell* shell) {
  (void)args;
  (void)shell;

  struct tms time_buf;
  clock_t wall_time = times(&time_buf);

  if (wall_time == (clock_t)-1) {
    perror("times");
    return 1;
  }

  long clock_ticks = sysconf(_SC_CLK_TCK);
  if (clock_ticks <= 0) {
    std::cerr << "times: unable to get clock ticks per second" << std::endl;
    return 1;
  }

  double user_time = static_cast<double>(time_buf.tms_utime) / clock_ticks;
  double system_time = static_cast<double>(time_buf.tms_stime) / clock_ticks;
  double child_user_time =
      static_cast<double>(time_buf.tms_cutime) / clock_ticks;
  double child_system_time =
      static_cast<double>(time_buf.tms_cstime) / clock_ticks;

  auto format_time = [](double seconds) -> std::string {
    int minutes = static_cast<int>(seconds) / 60;
    double remaining_seconds = seconds - (minutes * 60);

    std::ostringstream oss;
    oss << minutes << "m" << std::fixed << std::setprecision(3)
        << remaining_seconds << "s";
    return oss.str();
  };

  std::cout << format_time(user_time) << " " << format_time(system_time) << " "
            << format_time(child_user_time) << " "
            << format_time(child_system_time) << std::endl;

  return 0;
}

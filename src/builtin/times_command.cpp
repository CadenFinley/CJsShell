#include "times_command.h"
#include <sys/times.h>
#include <unistd.h>
#include <iomanip>
#include <iostream>
#include <sstream>

int times_command(const std::vector<std::string>& args, Shell* shell) {
  (void)args;   // Suppress unused parameter warning
  (void)shell;  // Suppress unused parameter warning

  struct tms time_buf;
  clock_t wall_time = times(&time_buf);

  if (wall_time == (clock_t)-1) {
    perror("times");
    return 1;
  }

  // Get clock ticks per second
  long clock_ticks = sysconf(_SC_CLK_TCK);
  if (clock_ticks <= 0) {
    std::cerr << "times: unable to get clock ticks per second" << std::endl;
    return 1;
  }

  // Convert clock ticks to seconds and format output
  // POSIX times command format: "user_time system_time child_user_time
  // child_system_time"

  double user_time = static_cast<double>(time_buf.tms_utime) / clock_ticks;
  double system_time = static_cast<double>(time_buf.tms_stime) / clock_ticks;
  double child_user_time =
      static_cast<double>(time_buf.tms_cutime) / clock_ticks;
  double child_system_time =
      static_cast<double>(time_buf.tms_cstime) / clock_ticks;

  // Format: "0m0.123s 0m0.456s 0m0.789s 0m0.012s"
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

/*
  times_command.cpp

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

#include "times_command.h"

#include "builtin_help.h"

#include <sys/times.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "error_out.h"

int times_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(
            args,
            {"Usage: times", "Print accumulated process times for the shell and its children."})) {
        return 0;
    }
    (void)shell;

    struct tms time_buf{};
    clock_t wall_time = times(&time_buf);

    if (wall_time == (clock_t)-1) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "times",
                     std::string("system call failed: ") + std::strerror(errno),
                     {}});
        return 1;
    }

    long clock_ticks = sysconf(_SC_CLK_TCK);
    if (clock_ticks <= 0) {
        print_error(
            {ErrorType::RUNTIME_ERROR, "times", "unable to get clock ticks per second", {}});
        return 1;
    }

    double user_time = static_cast<double>(time_buf.tms_utime) / clock_ticks;
    double system_time = static_cast<double>(time_buf.tms_stime) / clock_ticks;
    double child_user_time = static_cast<double>(time_buf.tms_cutime) / clock_ticks;
    double child_system_time = static_cast<double>(time_buf.tms_cstime) / clock_ticks;

    auto format_time = [](double seconds) -> std::string {
        int minutes = static_cast<int>(seconds) / 60;
        double remaining_seconds = seconds - (minutes * 60);

        std::ostringstream oss;
        oss << minutes << "m" << std::fixed << std::setprecision(3) << remaining_seconds << "s";
        return oss.str();
    };

    std::cout << format_time(user_time) << " " << format_time(system_time) << " "
              << format_time(child_user_time) << " " << format_time(child_system_time) << '\n';

    return 0;
}

#include "system_info.h"

#include <cstdio>
#include "utils/cjsh_filesystem.h"

std::string get_disk_usage(const std::filesystem::path& path) {
    std::string cmd = "df -h '" + path.string() + "' | awk 'NR==2{print $5}'";
    auto result = cjsh_filesystem::FileOperations::read_command_output(cmd);
    if (result.is_error()) {
        return "";
    }
    std::string output = result.value();
    if (!output.empty() && output.back() == '\n') {
        output.pop_back();
    }
    return output;
}

std::string get_swap_usage() {
#ifdef __APPLE__
    std::string cmd = "sysctl vm.swapusage | awk '{print $7}' | sed 's/[(),]//g'";
#elif defined(__linux__)
    std::string cmd = "free | awk 'NR==3{printf \"%.2f%%\", $3/$2*100}'";
#else
    std::string cmd = "echo \"N/A\"";
#endif
    auto result = cjsh_filesystem::FileOperations::read_command_output(cmd);
    if (result.is_error()) {
        return "";
    }
    std::string output = result.value();
    if (!output.empty() && output.back() == '\n') {
        output.pop_back();
    }
    return output;
}

std::string get_load_avg() {
    std::string cmd = "uptime | awk -F'load averages?: ' '{print $2}'";
    auto result = cjsh_filesystem::FileOperations::read_command_output(cmd);
    if (result.is_error()) {
        return "";
    }
    std::string output = result.value();
    if (!output.empty() && output.back() == '\n') {
        output.pop_back();
    }
    return output;
}

std::string get_os_info() {
#ifdef __APPLE__
    std::string cmd = "sw_vers -productName && sw_vers -productVersion";
#elif defined(__linux__)
    // Try multiple sources for Linux distribution info
    std::string cmd =
        "if [ -f /etc/os-release ]; then "
        ". /etc/os-release && echo \"$NAME $VERSION\"; "
        "elif [ -f /etc/lsb-release ]; then "
        ". /etc/lsb-release && echo \"$DISTRIB_ID $DISTRIB_RELEASE\"; "
        "elif [ -f /etc/debian_version ]; then "
        "echo \"Debian $(cat /etc/debian_version)\"; "
        "elif [ -f /etc/redhat-release ]; then "
        "cat /etc/redhat-release; "
        "elif [ -f /etc/arch-release ]; then "
        "echo \"Arch Linux\"; "
        "else "
        "uname -s; "
        "fi";
#else
    std::string cmd = "uname -s";
#endif

    auto result_data = cjsh_filesystem::FileOperations::read_command_output(cmd);
    if (result_data.is_error()) {
        return "Unknown";
    }

    std::string result = result_data.value();
    // Clean up the result
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    // For macOS, combine product name and version on one line
#ifdef __APPLE__
    size_t newline_pos = result.find('\n');
    if (newline_pos != std::string::npos) {
        result[newline_pos] = ' ';
    }
#endif

    return result.empty() ? "Unknown" : result;
}

std::string get_kernel_version() {
#ifdef __APPLE__
    std::string cmd = "uname -r";
#elif defined(__linux__)
    std::string cmd = "uname -r";
#else
    std::string cmd = "uname -r";
#endif

    auto result_data = cjsh_filesystem::FileOperations::read_command_output(cmd);
    if (result_data.is_error()) {
        return "Unknown";
    }

    std::string result = result_data.value();
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    return result.empty() ? "Unknown" : result;
}

float get_cpu_usage() {
#ifdef __APPLE__
    std::string cmd = "top -l 1 -n 0 | awk '/CPU usage/ {print $3}' | sed 's/%//'";
#elif defined(__linux__)
    std::string cmd = R"(
    grep 'cpu ' /proc/stat | awk '{usage=($2+$4)*100/($2+$3+$4)} END {printf "%.1f", usage}'
  )";
#else
    return 0.0f;
#endif

    auto result_data = cjsh_filesystem::FileOperations::read_command_output(cmd);
    if (result_data.is_error()) {
        return 0.0f;
    }

    float usage = 0.0f;
    try {
        usage = std::stof(result_data.value());
    } catch (const std::exception& e) {
        usage = 0.0f;
    }

    return usage;
}

float get_memory_usage() {
#ifdef __APPLE__
    std::string cmd = R"(
    vm_stat | awk '
    /Pages free/ {free=$3}
    /Pages active/ {active=$3}
    /Pages inactive/ {inactive=$3}
    /Pages speculative/ {speculative=$3}
    /Pages wired/ {wired=$3}
    END {
      total = free + active + inactive + speculative + wired
      used = active + inactive + wired
      printf "%.1f", (used/total)*100
    }'
  )";
#elif defined(__linux__)
    std::string cmd = "free | awk 'NR==2{printf \"%.1f\", $3/$2*100}'";
#else
    return 0.0f;
#endif

    auto result_data = cjsh_filesystem::FileOperations::read_command_output(cmd);
    if (result_data.is_error()) {
        return 0.0f;
    }

    float usage = 0.0f;
    try {
        usage = std::stof(result_data.value());
    } catch (const std::exception& e) {
        usage = 0.0f;
    }

    return usage;
}

std::string get_battery_status() {
#ifdef __APPLE__
    std::string cmd = R"(
    pmset -g batt | awk '/InternalBattery/ {
      gsub(/[;%]/, "", $3)
      charging = ($4 == "charging" || $4 == "AC")
      printf "%s%s", $3, (charging ? "⚡" : "🔋")
    }'
  )";
#elif defined(__linux__)
    std::string cmd = R"(
    if [ -d /sys/class/power_supply/BAT0 ]; then
      capacity=$(cat /sys/class/power_supply/BAT0/capacity 2>/dev/null || echo "0")
      status=$(cat /sys/class/power_supply/BAT0/status 2>/dev/null || echo "Unknown")
      if [ "$status" = "Charging" ] || [ "$status" = "Full" ]; then
        echo "${capacity}%⚡"
      else
        echo "${capacity}%🔋"
      fi
    else
      echo "N/A"
    fi
  )";
#else
    return "N/A";
#endif

    auto result_data = cjsh_filesystem::FileOperations::read_command_output(cmd);
    if (result_data.is_error()) {
        return "N/A";
    }

    std::string result = result_data.value();
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    return result.empty() ? "N/A" : result;
}

std::string get_uptime() {
#ifdef __APPLE__
    std::string cmd = "uptime | awk '{print $3, $4}' | sed 's/,$//g'";
#elif defined(__linux__)
    std::string cmd = "uptime | awk '{print $3, $4}' | sed 's/,$//g'";
#else
    std::string cmd = "uptime";
#endif

    auto result_data = cjsh_filesystem::FileOperations::read_command_output(cmd);
    if (result_data.is_error()) {
        return "Unknown";
    }

    std::string result = result_data.value();
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    return result.empty() ? "Unknown" : result;
}

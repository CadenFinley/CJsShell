#include "system_info.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

#include "cjsh.h"
#include "utils/cjsh_filesystem.h"

std::string SystemInfo::get_disk_usage(const std::filesystem::path& path) {
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

std::string SystemInfo::get_swap_usage() {
#ifdef __APPLE__
  std::string cmd = "sysctl vm.swapusage | awk '{print $7}' | sed 's/[(),]//g'";
#elif defined(__linux__)
  std::string cmd = "free | awk 'NR==3{printf \"%.2f%%\", $3/$2*100}'";
#else
  std::string cmd = "echo \"N/A\"";
#endif
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp)
    return "";
  char buffer[32];
  std::string result = "";
  while (fgets(buffer, sizeof(buffer), fp) != NULL)
    result += buffer;
  pclose(fp);
  if (!result.empty() && result.back() == '\n')
    result.pop_back();
  return result;
}

std::string SystemInfo::get_load_avg() {
  std::string cmd = "uptime | awk -F'load averages?: ' '{print $2}'";
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp)
    return "";
  char buffer[64];
  std::string result = "";
  while (fgets(buffer, sizeof(buffer), fp) != NULL)
    result += buffer;
  pclose(fp);
  if (!result.empty() && result.back() == '\n')
    result.pop_back();
  return result;
}

std::string SystemInfo::get_os_info() {
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

  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp) {
    return "Unknown";
  }

  std::string result = "";
  char buffer[256];
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    result += buffer;
  }
  pclose(fp);

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

std::string SystemInfo::get_kernel_version() {
#ifdef __APPLE__
  std::string cmd = "uname -r";
#elif defined(__linux__)
  std::string cmd = "uname -r";
#else
  std::string cmd = "uname -r";
#endif

  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp) {
    return "Unknown";
  }

  char buffer[128];
  std::string result = "";
  if (fgets(buffer, sizeof(buffer), fp) != NULL) {
    result = buffer;
    if (!result.empty() && result.back() == '\n') {
      result.pop_back();
    }
  }
  pclose(fp);

  return result.empty() ? "Unknown" : result;
}

float SystemInfo::get_cpu_usage() {
#ifdef __APPLE__
  std::string cmd =
      "top -l 1 -n 0 | awk '/CPU usage/ {print $3}' | sed 's/%//'";
#elif defined(__linux__)
  std::string cmd = R"(
    grep 'cpu ' /proc/stat | awk '{usage=($2+$4)*100/($2+$3+$4)} END {printf "%.1f", usage}'
  )";
#else
  return 0.0f;
#endif

  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp) {
    return 0.0f;
  }

  char buffer[32];
  float usage = 0.0f;
  if (fgets(buffer, sizeof(buffer), fp) != NULL) {
    try {
      usage = std::stof(buffer);
    } catch (const std::exception& e) {
      usage = 0.0f;
    }
  }
  pclose(fp);

  return usage;
}

float SystemInfo::get_memory_usage() {
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

  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp) {
    return 0.0f;
  }

  char buffer[32];
  float usage = 0.0f;
  if (fgets(buffer, sizeof(buffer), fp) != NULL) {
    try {
      usage = std::stof(buffer);
    } catch (const std::exception& e) {
      usage = 0.0f;
    }
  }
  pclose(fp);

  return usage;
}

std::string SystemInfo::get_battery_status() {
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

  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp) {
    return "N/A";
  }

  char buffer[64];
  std::string result = "";
  if (fgets(buffer, sizeof(buffer), fp) != NULL) {
    result = buffer;
    if (!result.empty() && result.back() == '\n') {
      result.pop_back();
    }
  }
  pclose(fp);

  return result.empty() ? "N/A" : result;
}

std::string SystemInfo::get_uptime() {
#ifdef __APPLE__
  std::string cmd = "uptime | awk '{print $3, $4}' | sed 's/,$//g'";
#elif defined(__linux__)
  std::string cmd = "uptime | awk '{print $3, $4}' | sed 's/,$//g'";
#else
  std::string cmd = "uptime";
#endif

  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp) {
    return "Unknown";
  }

  char buffer[128];
  std::string result = "";
  if (fgets(buffer, sizeof(buffer), fp) != NULL) {
    result = buffer;
    if (!result.empty() && result.back() == '\n') {
      result.pop_back();
    }
  }
  pclose(fp);

  return result.empty() ? "Unknown" : result;
}
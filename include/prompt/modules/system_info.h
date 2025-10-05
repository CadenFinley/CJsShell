#pragma once

#include <filesystem>
#include <string>

namespace system_info {

std::string get_os_info();
std::string get_kernel_version();
float get_cpu_usage();
float get_memory_usage();
std::string get_battery_status();
std::string get_uptime();
std::string get_disk_usage(const std::filesystem::path& path = "/");
std::string get_swap_usage();
std::string get_load_avg();

}  // namespace system_info
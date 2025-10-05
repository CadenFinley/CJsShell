#pragma once

#include <filesystem>
#include <string>

namespace basic_info {

bool is_root_path(const std::filesystem::path& path);
std::string get_current_file_name();
std::string get_current_file_path();
std::string get_username();
std::string get_hostname();

}  // namespace basic_info
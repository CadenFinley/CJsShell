#pragma once

#include <string>
#include <vector>

namespace shell_script_interpreter::detail {

std::string trim(const std::string& s);
std::string strip_inline_comment(const std::string& s);
std::string process_line_for_validation(const std::string& line);
std::vector<std::string> split_ampersand(const std::string& s);
std::string to_lower_copy(std::string value);
bool is_readable_file(const std::string& path);

}  // namespace shell_script_interpreter::detail

#pragma once

#include <string>

namespace shell_script_interpreter::detail {

std::string trim(const std::string& s);
std::string strip_inline_comment(const std::string& s);
std::string process_line_for_validation(const std::string& line);

}  

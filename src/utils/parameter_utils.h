#pragma once

#include <string>

class Shell;

namespace parameter_utils {

std::string join_positional_parameters(const Shell* shell);
std::string get_last_background_pid_string();

}  // namespace parameter_utils

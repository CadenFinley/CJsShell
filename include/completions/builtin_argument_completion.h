#pragma once

#include <string>
#include <vector>

#include "isocline/isocline.h"

namespace builtin_argument_completion {

bool add_completions(ic_completion_env_t* cenv, const std::string& command,
                     const std::vector<std::string>& args, bool at_new_token);

}  // namespace builtin_argument_completion

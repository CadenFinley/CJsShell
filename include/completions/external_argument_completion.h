#pragma once

#include <string>
#include <vector>

#include "isocline/isocline.h"

namespace external_argument_completion {

void initialize();
bool add_completions(ic_completion_env_t* cenv, const std::string& command,
                     const std::vector<std::string>& args, bool at_new_token);

}  // namespace external_argument_completion

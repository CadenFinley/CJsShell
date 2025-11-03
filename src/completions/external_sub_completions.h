#pragma once

#include <string>

#include "isocline.h"

void handle_external_sub_completions(ic_completion_env_t* cenv, const char* raw_path_input);
std::string get_command_summary(const std::string& command, bool allow_fetch = true);
bool regenerate_external_completion_cache(const std::string& command, bool force_refresh = true);
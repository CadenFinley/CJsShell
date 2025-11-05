#pragma once

#include <string>

namespace prompt {

std::string render_primary_prompt();

std::string render_right_prompt();

std::string default_primary_prompt_template();

std::string default_right_prompt_template();

void execute_prompt_command();

}  // namespace prompt

#pragma once

#include <string>

#include "isocline/isocline.h"

namespace highlight_helpers {

void highlight_quotes_and_variables(ic_highlight_env_t* henv, const char* input, size_t start,
                                    size_t length);
void highlight_variable_assignment(ic_highlight_env_t* henv, const char* input,
                                   size_t absolute_start, const std::string& token);
void highlight_assignment_value(ic_highlight_env_t* henv, const char* input, size_t absolute_start,
                                const std::string& value);
void highlight_history_expansions(ic_highlight_env_t* henv, const char* input, size_t len);

}  

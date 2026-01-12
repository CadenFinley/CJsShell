#pragma once
#ifndef IC_PROMPT_LINE_REPLACEMENT_H
#define IC_PROMPT_LINE_REPLACEMENT_H

#include "common.h"

typedef struct ic_prompt_line_replacement_state_s {
    bool replace_prompt_line_with_line_number;
    bool prompt_has_prefix_lines;
    bool prompt_begins_with_newline;
    bool line_numbers_enabled;
    bool input_has_content;
} ic_prompt_line_replacement_state_t;

ic_private bool ic_prompt_line_replacement_should_activate(
    const ic_prompt_line_replacement_state_t* state);

#endif  // IC_PROMPT_LINE_REPLACEMENT_H

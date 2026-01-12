#include "prompt_line_replacement.h"

ic_private bool ic_prompt_line_replacement_should_activate(
    const ic_prompt_line_replacement_state_t* state) {
    if (state == NULL) {
        return false;
    }
    if (!state->replace_prompt_line_with_line_number) {
        return false;
    }
    if (!state->prompt_has_prefix_lines && !state->prompt_begins_with_newline) {
        return false;
    }
    if (!state->line_numbers_enabled) {
        return false;
    }
    if (!state->input_has_content) {
        return false;
    }
    return true;
}

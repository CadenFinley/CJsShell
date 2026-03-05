#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "isocline.h"

static int run_case(const char* scenario) {
    if (scenario == NULL) {
        return 2;
    }

    ic_enable_multiline(false);
    ic_enable_hint(false);
    ic_enable_inline_help(false);
    ic_enable_completion_preview(false);
    ic_enable_auto_tab(false);
    ic_enable_prompt_cleanup(false, 0);
    ic_reset_key_bindings();
    (void)ic_bind_key(IC_KEY_CTRL_P, IC_KEY_ACTION_HISTORY_PREV);
    (void)ic_bind_key(IC_KEY_CTRL_N, IC_KEY_ACTION_HISTORY_NEXT);

    const char* initial_input = NULL;
    if (strcmp(scenario, "cursor_move_insert") == 0) {
        initial_input = "ab";
    } else if (strcmp(scenario, "home_insert") == 0) {
        initial_input = "bc";
    } else if (strcmp(scenario, "end_insert") == 0) {
        initial_input = "ab";
    } else if (strcmp(scenario, "backspace_at_start_noop") == 0) {
        initial_input = "ab";
    } else if (strcmp(scenario, "delete_at_end_noop") == 0) {
        initial_input = "ab";
    } else if (strcmp(scenario, "kill_to_end_at_end_noop") == 0) {
        initial_input = "ab";
    } else if (strcmp(scenario, "kill_to_start_at_start_noop") == 0) {
        initial_input = "ab";
    } else if (strcmp(scenario, "left_boundary_insert") == 0) {
        initial_input = "ab";
    } else if (strcmp(scenario, "right_boundary_insert") == 0) {
        initial_input = "ab";
    } else if (strcmp(scenario, "append_to_initial_input") == 0) {
        initial_input = "ab";
    } else if (strcmp(scenario, "ctrl_l_redraw_keeps_buffer") == 0) {
        initial_input = "ab";
    } else if (strcmp(scenario, "ctrl_k_delete_to_end") == 0) {
        initial_input = "abcdef";
    } else if (strcmp(scenario, "ctrl_u_delete_to_start") == 0) {
        initial_input = "abcdef";
    } else if (strcmp(scenario, "ctrl_d_delete_mid") == 0) {
        initial_input = "abc";
    } else if (strcmp(scenario, "ctrl_w_single_word") == 0) {
        initial_input = "alpha";
    } else if (strcmp(scenario, "history_prev") == 0 ||
               strcmp(scenario, "history_prev_prev") == 0 ||
               strcmp(scenario, "history_next_empty") == 0 ||
               strcmp(scenario, "history_prev_edit") == 0) {
        ic_history_clear();
        ic_history_add("echo one");
        ic_history_add("echo two");
    } else if (strcmp(scenario, "insert_backspace") == 0 || strcmp(scenario, "ctrl_c") == 0 ||
               strcmp(scenario, "ctrl_d_empty") == 0 ||
               strcmp(scenario, "ctrl_w_delete_word") == 0) {
        // No additional setup needed.
    } else {
        return 2;
    }

    char* line = ic_readline("pty", NULL, initial_input);
    if (line == NULL) {
        return 3;
    }

    (void)printf("\n[IC_RESULT]%s\n", line);
    (void)fflush(stdout);
    ic_free(line);
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        (void)fprintf(stderr,
                      "usage: %s <insert_backspace|cursor_move_insert|home_insert|end_insert|"
                      "backspace_at_start_noop|delete_at_end_noop|kill_to_end_at_end_noop|"
                      "kill_to_start_at_start_noop|left_boundary_insert|right_boundary_insert|"
                      "append_to_initial_input|ctrl_l_redraw_keeps_buffer|ctrl_k_delete_to_end|"
                      "ctrl_u_delete_to_start|ctrl_w_delete_word|ctrl_w_single_word|"
                      "ctrl_d_delete_mid|ctrl_c|ctrl_d_empty|history_prev|history_prev_prev|"
                      "history_next_empty|history_prev_edit>\n",
                      argv[0]);
        return 2;
    }
    return run_case(argv[1]);
}

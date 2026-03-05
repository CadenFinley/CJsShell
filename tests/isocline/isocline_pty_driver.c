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

    if (strcmp(scenario, "insert_backspace") == 0) {
        // No additional setup needed.
    } else if (strcmp(scenario, "cursor_move_insert") == 0) {
        // Uses initial input to validate cursor navigation edits.
    } else if (strcmp(scenario, "ctrl_c") == 0) {
        // No additional setup needed.
    } else {
        return 2;
    }

    const char* initial_input = NULL;
    if (strcmp(scenario, "cursor_move_insert") == 0) {
        initial_input = "ab";
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
        (void)fprintf(stderr, "usage: %s <insert_backspace|cursor_move_insert|ctrl_c>\n", argv[0]);
        return 2;
    }
    return run_case(argv[1]);
}

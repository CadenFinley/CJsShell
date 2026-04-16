/*
  isocline_pty_driver.c

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if !defined(_WIN32)
#include <pthread.h>
#include <unistd.h>
#endif

#include "env.h"
#include "history.h"
#include "isocline.h"

typedef enum completion_mode_e {
    COMPLETION_MODE_NONE = 0,
    COMPLETION_MODE_SINGLE,
    COMPLETION_MODE_DUAL,
    COMPLETION_MODE_MANY,
} completion_mode_t;

static completion_mode_t g_completion_mode = COMPLETION_MODE_NONE;

static void pty_completion_word_provider(ic_completion_env_t* cenv, const char* prefix) {
    if (g_completion_mode == COMPLETION_MODE_SINGLE) {
        static const char* single_words[] = {"hello", NULL};
        (void)ic_add_completions(cenv, prefix, single_words);
        return;
    }
    if (g_completion_mode == COMPLETION_MODE_DUAL) {
        static const char* dual_words[] = {"planet", "planar", NULL};
        (void)ic_add_completions(cenv, prefix, dual_words);
        return;
    }
    if (g_completion_mode == COMPLETION_MODE_MANY) {
        static const char* many_words[] = {
            "s01", "s02", "s03", "s04", "s05", "s06", "s07",
            "s08", "s09", "s10", "s11", "s12", NULL,
        };
        (void)ic_add_completions(cenv, prefix, many_words);
        return;
    }
}

static void pty_completion_dispatcher(ic_completion_env_t* cenv, const char* prefix) {
    ic_complete_word(cenv, prefix, pty_completion_word_provider, NULL);
}

static void emit_result(const char* line) {
    const char* text = (line == NULL ? "" : line);
    (void)printf("\n[IC_RESULT_BEGIN]");
    if (text[0] != '\0') {
        (void)fwrite(text, sizeof(char), strlen(text), stdout);
    }
    (void)printf("[IC_RESULT_END]\n");
    (void)fflush(stdout);
}

static int run_history_probe_case(const char* scenario) {
#if defined(_WIN32)
    const char* history_path = "cjsh_isocline_pty_history.tmp";
#else
    const char* history_path = "/tmp/cjsh_isocline_pty_history.tmp";
#endif
    (void)remove(history_path);
    ic_set_history(history_path, 200);
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->history == NULL) {
        emit_result("ERR:no-env");
        return 0;
    }

    ic_history_clear();
    ic_history_add("echo one");
    ic_history_add("echo two");

    if (strcmp(scenario, "history_probe_latest") == 0) {
        const char* latest = history_get(env->history, 0);
        emit_result(latest);
        return 0;
    }
    if (strcmp(scenario, "history_probe_previous") == 0) {
        const char* prev = history_get(env->history, 1);
        emit_result(prev);
        return 0;
    }
    if (strcmp(scenario, "history_probe_remove_last") == 0) {
        ic_history_remove_last();
        const char* latest = history_get(env->history, 0);
        emit_result(latest);
        return 0;
    }
    if (strcmp(scenario, "history_probe_count") == 0) {
        char buf[32];
        (void)snprintf(buf, sizeof(buf), "%zd", history_count(env->history));
        emit_result(buf);
        return 0;
    }

    emit_result("ERR:unknown-probe");
    return 0;
}

static bool queue_raw_bytes(const uint8_t* bytes, size_t count) {
    if (bytes == NULL && count > 0) {
        return false;
    }
    return ic_push_raw_input(bytes, count);
}

#if !defined(_WIN32)
typedef struct delayed_raw_feed_s {
    const uint8_t* bytes;
    size_t count;
    unsigned int delay_us;
} delayed_raw_feed_t;

static void* delayed_raw_feed_thread(void* arg) {
    delayed_raw_feed_t* feed = (delayed_raw_feed_t*)arg;
    if (feed == NULL) {
        return NULL;
    }

    struct timespec req = {
        .tv_sec = feed->delay_us / 1000000u,
        .tv_nsec = (long)((feed->delay_us % 1000000u) * 1000u),
    };
    (void)nanosleep(&req, NULL);

    (void)ic_push_raw_input(feed->bytes, feed->count);
    return NULL;
}
#endif

static int run_case(const char* scenario) {
    if (scenario == NULL) {
        return 2;
    }

    if (strncmp(scenario, "history_probe_", 14) == 0) {
        return run_history_probe_case(scenario);
    }

    bool multiline_mode = (strcmp(scenario, "multiline_ctrl_j_insert_newline") == 0 ||
                           strcmp(scenario, "multiline_backslash_continuation") == 0 ||
                           strcmp(scenario, "multiline_initial_ctrl_j") == 0 ||
                           strcmp(scenario, "multiline_ctrl_a_chain") == 0 ||
                           strcmp(scenario, "multiline_ctrl_e_chain") == 0);
    ic_enable_multiline(multiline_mode);
    ic_enable_hint(false);
    ic_enable_inline_help(false);
    ic_enable_completion_preview(false);
    ic_enable_auto_tab(false);
    ic_enable_prompt_cleanup(false, 0);
#if defined(_WIN32)
    const char* history_path = "cjsh_isocline_pty_history.tmp";
#else
    const char* history_path = "/tmp/cjsh_isocline_pty_history.tmp";
#endif
    (void)remove(history_path);
    ic_set_history(history_path, 200);
    g_completion_mode = COMPLETION_MODE_NONE;
    ic_set_default_completer(NULL, NULL);

    const char* initial_input = NULL;
    const char* pre_prompt_output = NULL;
    const char* prompt_text = "pty";
    const char* prompt_marker = NULL;
    const char* continuation_prompt_marker = NULL;
    bool history_interactive_triplet = false;
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
    } else if (strcmp(scenario, "ctrl_a_ctrl_e_append") == 0) {
        initial_input = "ab";
    } else if (strcmp(scenario, "ctrl_d_at_end_noop") == 0) {
        initial_input = "ab";
    } else if (strcmp(scenario, "undo_single_change") == 0) {
        initial_input = "ab";
    } else if (strcmp(scenario, "undo_redo_roundtrip") == 0) {
        initial_input = "ab";
    } else if (strcmp(scenario, "undo_after_kill_to_end") == 0) {
        initial_input = "abcdef";
    } else if (strcmp(scenario, "redo_cleared_by_new_edit") == 0) {
        initial_input = "ab";
    } else if (strcmp(scenario, "resize_reflow_initial_input") == 0) {
        initial_input = "abcdefghij";
    } else if (strcmp(scenario, "shell_prompt_wrap_boundary") == 0) {
        prompt_text = "pty> CJsShell git:(master) x ";
        prompt_marker = "";
        continuation_prompt_marker = "> ";
    } else if (strcmp(scenario, "multiline_initial_ctrl_j") == 0) {
        initial_input = "ab";
    } else if (strcmp(scenario, "multiline_ctrl_a_chain") == 0) {
        initial_input = "ab\ncd\nef";
    } else if (strcmp(scenario, "multiline_ctrl_e_chain") == 0) {
        initial_input = "ab\ncd\nef";
    } else if (strcmp(scenario, "completion_midline_single") == 0) {
        initial_input = "say he";
        g_completion_mode = COMPLETION_MODE_SINGLE;
        ic_set_default_completer(pty_completion_dispatcher, NULL);
    } else if (strcmp(scenario, "hint_clears_on_empty_line") == 0) {
        g_completion_mode = COMPLETION_MODE_SINGLE;
        ic_enable_hint(true);
        (void)ic_set_hint_delay(0);
        ic_set_default_completer(pty_completion_dispatcher, NULL);
    } else if (strcmp(scenario, "completion_many_menu") == 0) {
        g_completion_mode = COMPLETION_MODE_MANY;
        ic_set_default_completer(pty_completion_dispatcher, NULL);
    } else if (strcmp(scenario, "history_search_scroll") == 0) {
        initial_input = "history";
        ic_history_clear();
        ic_history_add("history alpha");
        ic_history_add("history beta");
    } else if (strcmp(scenario, "ctrl_k_delete_to_end") == 0) {
        initial_input = "abcdef";
    } else if (strcmp(scenario, "ctrl_k_then_type") == 0) {
        initial_input = "abcdef";
    } else if (strcmp(scenario, "ctrl_u_delete_to_start") == 0) {
        initial_input = "abcdef";
    } else if (strcmp(scenario, "ctrl_u_then_type") == 0) {
        initial_input = "abcdef";
    } else if (strcmp(scenario, "delete_mid_twice") == 0) {
        initial_input = "abcd";
    } else if (strcmp(scenario, "ctrl_d_delete_mid") == 0) {
        initial_input = "abc";
    } else if (strcmp(scenario, "ctrl_w_single_word") == 0) {
        initial_input = "alpha";
    } else if (strcmp(scenario, "history_prev") == 0 ||
               strcmp(scenario, "history_prev_prev") == 0 ||
               strcmp(scenario, "history_next_empty") == 0 ||
               strcmp(scenario, "history_prev_edit") == 0) {
        ic_history_clear();
        history_interactive_triplet = true;
    } else if (strcmp(scenario, "insert_backspace") == 0 || strcmp(scenario, "ctrl_c") == 0 ||
               strcmp(scenario, "ctrl_d_empty") == 0 ||
               strcmp(scenario, "ctrl_w_delete_word") == 0 ||
               strcmp(scenario, "backspace_twice_typed") == 0 ||
               strcmp(scenario, "ctrl_w_then_type") == 0 ||
               strcmp(scenario, "resize_reflow_typed_input") == 0 ||
               strcmp(scenario, "multiline_ctrl_j_insert_newline") == 0 ||
               strcmp(scenario, "multiline_backslash_continuation") == 0 ||
               strcmp(scenario, "completion_single_tab") == 0 ||
               strcmp(scenario, "completion_single_then_type") == 0 ||
               strcmp(scenario, "completion_no_match") == 0 ||
               strcmp(scenario, "completion_dual_common_prefix") == 0) {
        if (strcmp(scenario, "completion_single_tab") == 0 ||
            strcmp(scenario, "completion_single_then_type") == 0 ||
            strcmp(scenario, "completion_no_match") == 0) {
            g_completion_mode = COMPLETION_MODE_SINGLE;
            ic_set_default_completer(pty_completion_dispatcher, NULL);
        } else if (strcmp(scenario, "completion_dual_common_prefix") == 0) {
            g_completion_mode = COMPLETION_MODE_DUAL;
            ic_set_default_completer(pty_completion_dispatcher, NULL);
        }
        // No additional setup needed.
    } else if (strcmp(scenario, "prompt_guard_visible_text") == 0) {
        pre_prompt_output = "visible-before-prompt";
    } else if (strcmp(scenario, "prompt_guard_tab_only") == 0) {
        pre_prompt_output = "\t";
    } else if (strcmp(scenario, "prompt_guard_escape_only") == 0) {
        pre_prompt_output = "\x1B[31m\x1B[0m";
    } else if (strcmp(scenario, "prompt_guard_osc_only") == 0) {
        pre_prompt_output = "\x1B]0;isocline-title\x07";
    } else if (strcmp(scenario, "prompt_guard_newline_reset") == 0) {
        pre_prompt_output = "prefix line\n";
    } else if (strcmp(scenario, "prompt_guard_escape_then_visible") == 0) {
        pre_prompt_output = "\x1B[31mX\x1B[0m";
    } else if (strcmp(scenario, "prompt_guard_spaces_only") == 0) {
        pre_prompt_output = "   ";
    } else if (strcmp(scenario, "prompt_guard_controls_only") == 0) {
        pre_prompt_output = "\a\b\r\v\f";
    } else if (strcmp(scenario, "prompt_guard_carriage_return_only") == 0) {
        pre_prompt_output = "\r";
    } else if (strcmp(scenario, "prompt_guard_visible_then_carriage_return") == 0) {
        pre_prompt_output = "abc\r";
    } else if (strcmp(scenario, "prompt_guard_visible_then_carriage_return_clear") == 0) {
        pre_prompt_output = "abc\r\x1B[2K";
    } else if (strcmp(scenario, "prompt_guard_forced_visible_line_start") == 0) {
        ic_term_mark_line_visible(true);
        pre_prompt_output = "\r";
    } else if (strcmp(scenario, "prompt_guard_visible_then_newline") == 0) {
        pre_prompt_output = "abc\n";
    } else if (strcmp(scenario, "prompt_guard_newline_then_visible") == 0) {
        pre_prompt_output = "\nabc";
    } else if (strcmp(scenario, "prompt_guard_double_newline_reset") == 0) {
        pre_prompt_output = "abc\n\n";
    } else if (strcmp(scenario, "prompt_guard_escape_then_space") == 0) {
        pre_prompt_output = "\x1B[31m \x1B[0m";
    } else if (strcmp(scenario, "prompt_guard_escape_then_newline_then_visible") == 0) {
        pre_prompt_output = "\x1B[31m\nX\x1B[0m";
    } else if (strcmp(scenario, "prompt_guard_visible_then_newline_then_escape") == 0) {
        pre_prompt_output = "abc\n\x1B[31m\x1B[0m";
    } else if (strcmp(scenario, "prompt_guard_bracketed_toggle_only") == 0) {
        pre_prompt_output = "\x1B[?2004h\x1B[?2004l";
    } else if (strcmp(scenario, "prompt_guard_bracketed_toggle_then_tab") == 0) {
        pre_prompt_output = "\x1B[?2004h\t\x1B[?2004l";
    } else if (strcmp(scenario, "prompt_guard_utf8_visible") == 0) {
        pre_prompt_output = "\xE2\x82\xAC";
    } else if (strcmp(scenario, "prompt_guard_osc_then_space") == 0) {
        pre_prompt_output = "\x1B]0;x\x07 ";
    } else {
        return 2;
    }

    if (pre_prompt_output != NULL) {
        ic_term_write(pre_prompt_output);
    }

    if (prompt_marker != NULL || continuation_prompt_marker != NULL) {
        ic_set_prompt_marker((prompt_marker != NULL ? prompt_marker : "> "),
                             continuation_prompt_marker);
    }

    char* line = NULL;
    if (history_interactive_triplet) {
        char* first = ic_readline(prompt_text, NULL, NULL);
        if (first == NULL)
            return 4;
        ic_free(first);

        char* second = ic_readline(prompt_text, NULL, NULL);
        if (second == NULL)
            return 4;
        ic_free(second);

        line = ic_readline(prompt_text, NULL, NULL);
    } else {
        line = ic_readline(prompt_text, NULL, initial_input);
    }
    if (line == NULL) {
        return 3;
    }

    emit_result(line);
    ic_free(line);
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        (void)fprintf(stderr, "usage: %s <scenario>\n", argv[0]);
        return 2;
    }
    return run_case(argv[1]);
}

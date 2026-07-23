/*
  isocline_terminal.c

  This file is part of isocline

  MIT License

  Copyright (c) 2026 Caden Finley
  Copyright (c) 2021 Daan Leijen
  Largely modified for CJ's Shell

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

/* ----------------------------------------------------------------------------
    Terminal helper APIs split from the original isocline.c file.
-----------------------------------------------------------------------------*/

#include <stdarg.h>
#include <stdio.h>

#include "common.h"
#include "env.h"
#include "env_internal.h"

typedef enum ic_terminal_region_state_e {
    IC_TERMINAL_REGION_NONE = 0,
    IC_TERMINAL_REGION_INPUT = 1,
    IC_TERMINAL_REGION_OUTPUT = 2,
} ic_terminal_region_state_t;

static bool terminal_region_marking_available(const ic_env_t* env) {
    return (env != NULL && env->terminal_region_marking_enabled && env->term != NULL &&
            term_is_interactive(env->term));
}

static void terminal_region_write(ic_env_t* env, const char* sequence, bool flush) {
    if (!terminal_region_marking_available(env) || sequence == NULL) {
        return;
    }
    term_write_untracked(env->term, sequence);
    if (flush) {
        term_flush(env->term);
    }
}

ic_private void ic_term_mark_prompt_start(ic_env_t* env, bool continuation_line) {
    terminal_region_write(env, (continuation_line ? "\x1b]133;A;k=s\x1b\\" : "\x1b]133;A\x1b\\"),
                          false);
}

ic_private void ic_term_mark_input_start(ic_env_t* env) {
    if (!terminal_region_marking_available(env)) {
        return;
    }
    terminal_region_write(env, "\x1b]133;B\x1b\\", false);
    env->terminal_region_state = IC_TERMINAL_REGION_INPUT;
}

ic_private void ic_term_abort_input_region(ic_env_t* env) {
    if (!terminal_region_marking_available(env) ||
        env->terminal_region_state != IC_TERMINAL_REGION_INPUT) {
        return;
    }
    terminal_region_write(env, "\x1b]133;D\x1b\\", true);
    env->terminal_region_state = IC_TERMINAL_REGION_NONE;
}

ic_public bool ic_enable_terminal_region_marking(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL) {
        return false;
    }
    bool previous = env->terminal_region_marking_enabled;
    if (!enable && previous && env->terminal_region_state != IC_TERMINAL_REGION_NONE &&
        env->term != NULL && term_is_interactive(env->term)) {
        terminal_region_write(env, "\x1b]133;D\x1b\\", true);
    }
    env->terminal_region_marking_enabled = enable;
    if (!enable) {
        env->terminal_region_state = IC_TERMINAL_REGION_NONE;
    }
    return previous;
}

ic_public bool ic_terminal_region_marking_is_enabled(void) {
    ic_env_t* env = ic_get_env();
    return (env != NULL && env->terminal_region_marking_enabled);
}

ic_public void ic_mark_command_start(void) {
    ic_env_t* env = ic_get_env();
    if (!terminal_region_marking_available(env) ||
        env->terminal_region_state == IC_TERMINAL_REGION_OUTPUT) {
        return;
    }
    terminal_region_write(env, "\x1b]133;C\x1b\\", true);
    env->terminal_region_state = IC_TERMINAL_REGION_OUTPUT;
}

ic_public void ic_mark_command_finished(int exit_status) {
    ic_env_t* env = ic_get_env();
    if (!terminal_region_marking_available(env) ||
        env->terminal_region_state == IC_TERMINAL_REGION_NONE) {
        return;
    }
    if (exit_status < 0) {
        exit_status = 0;
    } else if (exit_status > 255) {
        exit_status = 255;
    }
    char sequence[32];
    (void)snprintf(sequence, sizeof(sequence), "\x1b]133;D;%d\x1b\\", exit_status);
    terminal_region_write(env, sequence, true);
    env->terminal_region_state = IC_TERMINAL_REGION_NONE;
}

ic_public void ic_term_init(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    if (env->term == NULL)
        return;
    term_start_raw(env->term);
}

ic_public bool ic_push_key_event(ic_keycode_t key) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->tty == NULL)
        return false;
    tty_code_pushback(env->tty, key);
    return true;
}

ic_public bool ic_push_key_sequence(const ic_keycode_t* keys, size_t count) {
    if (keys == NULL || count == 0)
        return true;
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->tty == NULL)
        return false;
    for (size_t i = count; i > 0; --i) {
        tty_code_pushback(env->tty, keys[i - 1]);
    }
    return true;
}

ic_public bool ic_push_raw_input(const uint8_t* data, size_t length) {
    if (length == 0)
        return true;
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->tty == NULL)
        return false;
    if (data == NULL)
        return false;
    for (size_t i = length; i > 0; --i) {
        tty_cpush_char(env->tty, data[i - 1]);
    }
    return true;
}

ic_public void ic_term_done(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    if (env->term == NULL)
        return;
    term_end_raw(env->term, false);
}

ic_public void ic_term_flush(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    if (env->term == NULL)
        return;
    term_flush(env->term);
}

ic_public void ic_term_write(const char* s) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    if (env->term == NULL)
        return;
    term_write(env->term, s);
}

ic_public void ic_term_writeln(const char* s) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    if (env->term == NULL)
        return;
    term_writeln(env->term, s);
}

ic_public void ic_term_writef(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    ic_term_vwritef(fmt, ap);
    va_end(ap);
}

ic_public void ic_term_vwritef(const char* fmt, va_list args) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    if (env->term == NULL)
        return;
    term_vwritef(env->term, fmt, args);
}

ic_public void ic_term_reset(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    if (env->term == NULL)
        return;
    term_attr_reset(env->term);
}

ic_public void ic_term_style(const char* style) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    if (env->term == NULL || env->bbcode == NULL)
        return;
    term_set_attr(env->term, bbcode_style(env->bbcode, style));
}

ic_public int ic_term_get_color_bits(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->term == NULL)
        return 4;
    return term_get_color_bits(env->term);
}

ic_public void ic_term_mark_line_visible(bool visible) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->term == NULL)
        return;
    term_mark_line_visible(env->term, visible);
}

ic_public void ic_term_bold(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->term == NULL)
        return;
    term_bold(env->term, enable);
}

ic_public void ic_term_underline(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->term == NULL)
        return;
    term_underline(env->term, enable);
}

ic_public void ic_term_italic(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->term == NULL)
        return;
    term_italic(env->term, enable);
}

ic_public void ic_term_reverse(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->term == NULL)
        return;
    term_reverse(env->term, enable);
}

ic_public void ic_term_color_ansi(bool foreground, int ansi_color) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->term == NULL)
        return;
    ic_color_t color = color_from_ansi256(ansi_color);
    if (foreground) {
        term_color(env->term, color);
    } else {
        term_bgcolor(env->term, color);
    }
}

ic_public void ic_term_color_rgb(bool foreground, uint32_t hcolor) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->term == NULL)
        return;
    ic_color_t color = ic_rgb(hcolor);
    if (foreground) {
        term_color(env->term, color);
    } else {
        term_bgcolor(env->term, color);
    }
}

ic_public void ic_term_underline_color_ansi(int ansi_color) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->term == NULL)
        return;
    ic_color_t color = color_from_ansi256(ansi_color);
    term_underline_color(env->term, color);
}

ic_public void ic_term_underline_color_rgb(uint32_t hcolor) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->term == NULL)
        return;
    ic_color_t color = ic_rgb(hcolor);
    term_underline_color(env->term, color);
}

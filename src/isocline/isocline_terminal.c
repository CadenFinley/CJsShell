/* ----------------------------------------------------------------------------
    Copyright (c) 2021, Daan Leijen
    Largely Modified by Caden Finley 2025 for CJ's Shell
    This is free software; you can redistribute it and/or modify it
    under the terms of the MIT License. A copy of the license can be
    found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/

/* ----------------------------------------------------------------------------
    Terminal helper APIs split from the original isocline.c file.
-----------------------------------------------------------------------------*/

#include <stdarg.h>

#include "common.h"
#include "env.h"

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
        return true;
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

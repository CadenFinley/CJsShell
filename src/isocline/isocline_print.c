/* ----------------------------------------------------------------------------
    Copyright (c) 2021, Daan Leijen
    Largely Modified by Caden Finley 2025 for CJ's Shell
    This is free software; you can redistribute it and/or modify it
    under the terms of the MIT License. A copy of the license can be
    found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/

/* ----------------------------------------------------------------------------
    Printing helpers extracted from the original isocline.c.
-----------------------------------------------------------------------------*/

#include <stdarg.h>

#include "common.h"
#include "env.h"

ic_public void ic_printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    ic_vprintf(fmt, ap);
    va_end(ap);
}

ic_public void ic_vprintf(const char* fmt, va_list args) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->bbcode == NULL)
        return;
    bbcode_vprintf(env->bbcode, fmt, args);
}

ic_public void ic_print(const char* s) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->bbcode == NULL)
        return;
    bbcode_print(env->bbcode, s);
}

ic_public void ic_println(const char* s) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->bbcode == NULL)
        return;
    bbcode_println(env->bbcode, s);
}

void ic_style_def(const char* name, const char* fmt) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->bbcode == NULL)
        return;
    bbcode_style_def(env->bbcode, name, fmt);
}

void ic_style_open(const char* fmt) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->bbcode == NULL)
        return;
    bbcode_style_open(env->bbcode, fmt);
}

void ic_style_close(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->bbcode == NULL)
        return;
    bbcode_style_close(env->bbcode, NULL);
}

/* ----------------------------------------------------------------------------
  Copyright (c) 2021, Daan Leijen
  Largely Modified by Caden Finley 2025 for CJ's Shell
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License.

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
-----------------------------------------------------------------------------*/

/* ----------------------------------------------------------------------------
    Printing helpers extracted from the original isocline.c.
-----------------------------------------------------------------------------*/

#include <stdarg.h>
#include <stdio.h>

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
    // printf("Defining style: %s -> %s\n", name, fmt);  // DEBUG
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

/* ----------------------------------------------------------------------------
    Copyright (c) 2021, Daan Leijen
    Largely Modified by Caden Finley 2025 for CJ's Shell
    This is free software; you can redistribute it and/or modify it
    under the terms of the MIT License. A copy of the license can be
    found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/

/* ----------------------------------------------------------------------------
    Readline front-end extracted from the original isocline.c.
-----------------------------------------------------------------------------*/

#include <stdio.h>

#include "common.h"
#include "env.h"
#include "env_internal.h"
#include "stringbuf.h"

//-------------------------------------------------------------
// Global variables
//-------------------------------------------------------------

static bool getline_interrupt = false;

//-------------------------------------------------------------
// Fallback getline implementation
//-------------------------------------------------------------

static char* ic_getline(alloc_t* mem) {
    getline_interrupt = false;
    stringbuf_t* sb = sbuf_new(mem);
    int c;
    while (true) {
        c = fgetc(stdin);
        if (c == EOF || c == '\n') {
            break;
        } else {
            sbuf_append_char(sb, (char)c);
        }
        if (getline_interrupt) {
            break;
        }
    }
    return sbuf_free_dup(sb);
}

//-------------------------------------------------------------
// Public API
//-------------------------------------------------------------

ic_public char* ic_readline(const char* prompt_text, const char* initial_input) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return NULL;
    if (!env->noedit) {
        if (initial_input != NULL) {
            ic_env_set_initial_input(env, initial_input);
        }
        char* result = ic_editline(env, prompt_text);
        ic_env_clear_initial_input(env);
        return result;
    } else {
        if (env->tty != NULL && env->term != NULL) {
            term_start_raw(env->term);
            if (prompt_text != NULL) {
                term_write(env->term, prompt_text);
            }
            term_write(env->term, env->prompt_marker);
            term_end_raw(env->term, false);
        }
        return ic_getline(env->mem);
    }
}

ic_public char* ic_readline_inline(const char* prompt_text, const char* inline_right_text,
                                   const char* initial_input) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return NULL;
    if (!env->noedit) {
        if (initial_input != NULL) {
            ic_env_set_initial_input(env, initial_input);
        }
        char* result = ic_editline_inline(env, prompt_text, inline_right_text);
        ic_env_clear_initial_input(env);
        return result;
    } else {
        return ic_readline(prompt_text, initial_input);
    }
}

ic_public bool ic_async_stop(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    if (env->tty == NULL)
        return false;
    return tty_async_stop(env->tty);
}

ic_public bool ic_async_interrupt_getline(void) {
    getline_interrupt = true;
    return true;
}

ic_public void ic_print_prompt(const char* prompt_text, bool continuation_line) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->term == NULL || env->bbcode == NULL) {
        return;
    }

    term_start_raw(env->term);

    const char* text = (prompt_text != NULL ? prompt_text : "");

    bbcode_style_open(env->bbcode, "ic-prompt");

    if (!continuation_line) {
        bbcode_print(env->bbcode, text);
    } else if (!env->no_multiline_indent) {
        ssize_t textw = bbcode_column_width(env->bbcode, text);
        ssize_t markerw = bbcode_column_width(env->bbcode, env->prompt_marker);
        ssize_t cmarkerw = bbcode_column_width(env->bbcode, env->cprompt_marker);
        if (cmarkerw < markerw + textw) {
            term_write_repeat(env->term, " ", markerw + textw - cmarkerw);
        }
    }

    bbcode_print(env->bbcode, (continuation_line ? env->cprompt_marker : env->prompt_marker));

    bbcode_style_close(env->bbcode, NULL);
    term_flush(env->term);
}

ic_public char* ic_readline_ex(const char* prompt_text, ic_completer_fun_t* completer,
                               void* completer_arg, ic_highlight_fun_t* highlighter,
                               void* highlighter_arg) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return NULL;
    ic_completer_fun_t* prev_completer;
    void* prev_completer_arg;
    completions_get_completer(env->completions, &prev_completer, &prev_completer_arg);
    ic_highlight_fun_t* prev_highlighter = env->highlighter;
    void* prev_highlighter_arg = env->highlighter_arg;
    if (completer != NULL) {
        ic_set_default_completer(completer, completer_arg);
    }
    if (highlighter != NULL) {
        ic_set_default_highlighter(highlighter, highlighter_arg);
    }
    char* res = ic_readline(prompt_text, "");
    ic_set_default_completer(prev_completer, prev_completer_arg);
    ic_set_default_highlighter(prev_highlighter, prev_highlighter_arg);
    return res;
}

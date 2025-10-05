/* ----------------------------------------------------------------------------
  Copyright (c) 2021, Daan Leijen
  Largely Modified by Caden Finley 2025 for CJ's Shell
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License. A copy of the license can be
  found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/

//-------------------------------------------------------------
// Usually we include all sources one file so no internal
// symbols are public in the libray.
//
// You can compile the entire library just as:
// $ gcc -c src/isocline.c
//-------------------------------------------------------------
#if !defined(IC_SEPARATE_OBJS)
#ifndef _CRT_NONSTDC_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS  // for msvc
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS  // for msvc
#endif
#define _XOPEN_SOURCE 700  // for wcwidth
#define _DEFAULT_SOURCE    // ensure usleep stays visible with _XOPEN_SOURCE >=
                           // 700
#include "isocline/attr.c"
#include "isocline/bbcode.c"
#include "isocline/common.c"
#include "isocline/completers.c"
#include "isocline/completions.c"
#include "isocline/editline.c"
#include "isocline/highlight.c"
#include "isocline/history.c"
#include "isocline/stringbuf.c"
#include "isocline/term.c"
#include "isocline/tty.c"
#include "isocline/tty_esc.c"
#include "isocline/undo.c"
#endif

//-------------------------------------------------------------
// includes
//-------------------------------------------------------------
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "isocline/common.h"
#include "isocline/env.h"
#include "isocline/isocline.h"
#include "isocline/keybinding_specs.h"

//-------------------------------------------------------------
// Global variables
//-------------------------------------------------------------
static bool getline_interrupt = false;

//-------------------------------------------------------------
// Key binding helpers
//-------------------------------------------------------------

typedef struct key_action_name_entry_s {
    const char* name;
    ic_key_action_t action;
} key_action_name_entry_t;

static const key_action_name_entry_t key_action_names[] = {
    {"none", IC_KEY_ACTION_NONE},
    {"suppress", IC_KEY_ACTION_NONE},

    {"complete", IC_KEY_ACTION_COMPLETE},
    {"completion", IC_KEY_ACTION_COMPLETE},

    {"history-search", IC_KEY_ACTION_HISTORY_SEARCH},
    {"search-history", IC_KEY_ACTION_HISTORY_SEARCH},

    {"history-prev", IC_KEY_ACTION_HISTORY_PREV},
    {"history-up", IC_KEY_ACTION_HISTORY_PREV},

    {"history-next", IC_KEY_ACTION_HISTORY_NEXT},
    {"history-down", IC_KEY_ACTION_HISTORY_NEXT},

    {"clear-screen", IC_KEY_ACTION_CLEAR_SCREEN},
    {"cls", IC_KEY_ACTION_CLEAR_SCREEN},

    {"undo", IC_KEY_ACTION_UNDO},
    {"redo", IC_KEY_ACTION_REDO},

    {"show-help", IC_KEY_ACTION_SHOW_HELP},
    {"help", IC_KEY_ACTION_SHOW_HELP},

    {"cursor-left", IC_KEY_ACTION_CURSOR_LEFT},

    {"cursor-right-smart", IC_KEY_ACTION_CURSOR_RIGHT_OR_COMPLETE},
    {"cursor-right", IC_KEY_ACTION_CURSOR_RIGHT_OR_COMPLETE},

    {"cursor-up", IC_KEY_ACTION_CURSOR_UP},
    {"cursor-down", IC_KEY_ACTION_CURSOR_DOWN},

    {"cursor-line-start", IC_KEY_ACTION_CURSOR_LINE_START},
    {"cursor-line-end", IC_KEY_ACTION_CURSOR_LINE_END},

    {"cursor-word-prev", IC_KEY_ACTION_CURSOR_WORD_PREV},

    {"cursor-word-next-smart", IC_KEY_ACTION_CURSOR_WORD_NEXT_OR_COMPLETE},
    {"cursor-word-next", IC_KEY_ACTION_CURSOR_WORD_NEXT_OR_COMPLETE},

    {"cursor-input-start", IC_KEY_ACTION_CURSOR_INPUT_START},
    {"cursor-input-end", IC_KEY_ACTION_CURSOR_INPUT_END},
    {"cursor-match-brace", IC_KEY_ACTION_CURSOR_MATCH_BRACE},

    {"delete-backward", IC_KEY_ACTION_DELETE_BACKWARD},
    {"backspace", IC_KEY_ACTION_DELETE_BACKWARD},

    {"delete-forward", IC_KEY_ACTION_DELETE_FORWARD},
    {"delete", IC_KEY_ACTION_DELETE_FORWARD},

    {"delete-word-end", IC_KEY_ACTION_DELETE_WORD_END},
    {"kill-word", IC_KEY_ACTION_DELETE_WORD_END},

    {"delete-word-start-ws", IC_KEY_ACTION_DELETE_WORD_START_WS},
    {"backward-kill-word-ws", IC_KEY_ACTION_DELETE_WORD_START_WS},

    {"delete-word-start", IC_KEY_ACTION_DELETE_WORD_START},
    {"backward-kill-word", IC_KEY_ACTION_DELETE_WORD_START},

    {"delete-line-start", IC_KEY_ACTION_DELETE_LINE_START},
    {"delete-line-end", IC_KEY_ACTION_DELETE_LINE_END},

    {"transpose-chars", IC_KEY_ACTION_TRANSPOSE_CHARS},
    {"swap-chars", IC_KEY_ACTION_TRANSPOSE_CHARS},

    {"insert-newline", IC_KEY_ACTION_INSERT_NEWLINE},
    {"newline", IC_KEY_ACTION_INSERT_NEWLINE},
};

static size_t key_action_name_count(void) {
    return sizeof(key_action_names) / sizeof(key_action_names[0]);
}

typedef struct keybinding_profile_action_spec_s {
    ic_key_action_t action;
    const char* specs;
} keybinding_profile_action_spec_t;

typedef struct keybinding_profile_binding_s {
    ic_key_action_t action;
    const char* specs;
} keybinding_profile_binding_t;

typedef struct ic_keybinding_profile_s {
    const char* name;
    const char* description;
    const struct ic_keybinding_profile_s* parent;
    const keybinding_profile_action_spec_t* specs;
    size_t spec_count;
    const keybinding_profile_binding_t* bindings;
    size_t binding_count;
} keybinding_profile_t;

static const keybinding_profile_action_spec_t keybinding_profile_default_spec_entries[] = {
    {IC_KEY_ACTION_CURSOR_LEFT, SPEC_CURSOR_LEFT},
    {IC_KEY_ACTION_CURSOR_RIGHT_OR_COMPLETE, SPEC_CURSOR_RIGHT},
    {IC_KEY_ACTION_CURSOR_UP, SPEC_CURSOR_UP},
    {IC_KEY_ACTION_CURSOR_DOWN, SPEC_CURSOR_DOWN},
    {IC_KEY_ACTION_CURSOR_WORD_PREV, SPEC_CURSOR_WORD_PREV},
    {IC_KEY_ACTION_CURSOR_WORD_NEXT_OR_COMPLETE, SPEC_CURSOR_WORD_NEXT},
    {IC_KEY_ACTION_CURSOR_LINE_START, SPEC_CURSOR_LINE_START},
    {IC_KEY_ACTION_CURSOR_LINE_END, SPEC_CURSOR_LINE_END},
    {IC_KEY_ACTION_CURSOR_INPUT_START, SPEC_CURSOR_INPUT_START},
    {IC_KEY_ACTION_CURSOR_INPUT_END, SPEC_CURSOR_INPUT_END},
    {IC_KEY_ACTION_CURSOR_MATCH_BRACE, SPEC_CURSOR_MATCH_BRACE},
    {IC_KEY_ACTION_HISTORY_PREV, SPEC_HISTORY_PREV},
    {IC_KEY_ACTION_HISTORY_NEXT, SPEC_HISTORY_NEXT},
    {IC_KEY_ACTION_HISTORY_SEARCH, SPEC_HISTORY_SEARCH},
    {IC_KEY_ACTION_DELETE_FORWARD, SPEC_DELETE_FORWARD},
    {IC_KEY_ACTION_DELETE_BACKWARD, SPEC_DELETE_BACKWARD},
    {IC_KEY_ACTION_DELETE_WORD_START_WS, SPEC_DELETE_WORD_START_WS},
    {IC_KEY_ACTION_DELETE_WORD_START, SPEC_DELETE_WORD_START},
    {IC_KEY_ACTION_DELETE_WORD_END, SPEC_DELETE_WORD_END},
    {IC_KEY_ACTION_DELETE_LINE_START, SPEC_DELETE_LINE_START},
    {IC_KEY_ACTION_DELETE_LINE_END, SPEC_DELETE_LINE_END},
    {IC_KEY_ACTION_TRANSPOSE_CHARS, SPEC_TRANSPOSE},
    {IC_KEY_ACTION_CLEAR_SCREEN, SPEC_CLEAR_SCREEN},
    {IC_KEY_ACTION_UNDO, SPEC_UNDO},
    {IC_KEY_ACTION_REDO, SPEC_REDO},
    {IC_KEY_ACTION_COMPLETE, SPEC_COMPLETE},
    {IC_KEY_ACTION_INSERT_NEWLINE, SPEC_INSERT_NEWLINE},
};

static const keybinding_profile_t keybinding_profile_default = {
    "emacs",
    "Emacs-style bindings (default)",
    NULL,
    keybinding_profile_default_spec_entries,
    sizeof(keybinding_profile_default_spec_entries) /
        sizeof(keybinding_profile_default_spec_entries[0]),
    NULL,
    0,
};

static const keybinding_profile_action_spec_t keybinding_profile_vim_spec_entries[] = {
    {IC_KEY_ACTION_CURSOR_LEFT, SPEC_CURSOR_LEFT "|alt+h"},
    {IC_KEY_ACTION_CURSOR_RIGHT_OR_COMPLETE, SPEC_CURSOR_RIGHT "|alt+l"},
    {IC_KEY_ACTION_CURSOR_UP, SPEC_CURSOR_UP "|alt+k"},
    {IC_KEY_ACTION_CURSOR_DOWN, SPEC_CURSOR_DOWN "|alt+j"},
    {IC_KEY_ACTION_CURSOR_WORD_NEXT_OR_COMPLETE, SPEC_CURSOR_WORD_NEXT "|alt+w"},
};

static const keybinding_profile_binding_t keybinding_profile_vim_bindings[] = {
    {IC_KEY_ACTION_CURSOR_LEFT, "alt+h"},
    {IC_KEY_ACTION_CURSOR_RIGHT_OR_COMPLETE, "alt+l"},
    {IC_KEY_ACTION_CURSOR_UP, "alt+k"},
    {IC_KEY_ACTION_CURSOR_DOWN, "alt+j"},
    {IC_KEY_ACTION_CURSOR_WORD_NEXT_OR_COMPLETE, "alt+w"},
};

static const keybinding_profile_t keybinding_profile_vim = {
    "vim",
    "Vim-inspired navigation bindings (Alt+H/J/K/L, Alt+W)",
    &keybinding_profile_default,
    keybinding_profile_vim_spec_entries,
    sizeof(keybinding_profile_vim_spec_entries) / sizeof(keybinding_profile_vim_spec_entries[0]),
    keybinding_profile_vim_bindings,
    sizeof(keybinding_profile_vim_bindings) / sizeof(keybinding_profile_vim_bindings[0]),
};

static const keybinding_profile_t* const keybinding_profiles[] = {
    &keybinding_profile_default,
    &keybinding_profile_vim,
};

static size_t keybinding_profile_count(void) {
    return sizeof(keybinding_profiles) / sizeof(keybinding_profiles[0]);
}

static ic_key_binding_entry_t* key_binding_find_entry(ic_env_t* env, ic_keycode_t key,
                                                      ssize_t* index_out) {
    if (env == NULL || env->key_bindings == NULL)
        return NULL;
    for (ssize_t i = 0; i < env->key_binding_count; ++i) {
        if (env->key_bindings[i].key == key) {
            if (index_out != NULL)
                *index_out = i;
            return &env->key_bindings[i];
        }
    }
    return NULL;
}

static bool key_bindings_ensure_capacity(ic_env_t* env, ssize_t needed) {
    if (env->key_binding_capacity >= needed)
        return true;
    ssize_t new_capacity = (env->key_binding_capacity == 0 ? 8 : env->key_binding_capacity * 2);
    while (new_capacity < needed) {
        new_capacity *= 2;
    }
    ic_key_binding_entry_t* resized =
        mem_realloc_tp(env->mem, ic_key_binding_entry_t, env->key_bindings, new_capacity);
    if (resized == NULL)
        return false;
    env->key_bindings = resized;
    env->key_binding_capacity = new_capacity;
    return true;
}

typedef struct key_name_entry_s {
    const char* name;
    ic_keycode_t key;
} key_name_entry_t;

static const key_name_entry_t key_name_map[] = {
    {"tab", IC_KEY_TAB},
    {"enter", IC_KEY_ENTER},
    {"return", IC_KEY_ENTER},
    {"linefeed", IC_KEY_LINEFEED},
    {"lf", IC_KEY_LINEFEED},
    {"backspace", IC_KEY_BACKSP},
    {"bs", IC_KEY_BACKSP},
    {"delete", IC_KEY_DEL},
    {"del", IC_KEY_DEL},
    {"insert", IC_KEY_INS},
    {"ins", IC_KEY_INS},
    {"escape", IC_KEY_ESC},
    {"esc", IC_KEY_ESC},
    {"space", IC_KEY_SPACE},
    {"left", IC_KEY_LEFT},
    {"right", IC_KEY_RIGHT},
    {"up", IC_KEY_UP},
    {"down", IC_KEY_DOWN},
    {"home", IC_KEY_HOME},
    {"end", IC_KEY_END},
    {"pageup", IC_KEY_PAGEUP},
    {"pgup", IC_KEY_PAGEUP},
    {"pagedown", IC_KEY_PAGEDOWN},
    {"pgdn", IC_KEY_PAGEDOWN},
    {"f1", IC_KEY_F1},
    {"f2", IC_KEY_F2},
    {"f3", IC_KEY_F3},
    {"f4", IC_KEY_F4},
    {"f5", IC_KEY_F5},
    {"f6", IC_KEY_F6},
    {"f7", IC_KEY_F7},
    {"f8", IC_KEY_F8},
    {"f9", IC_KEY_F9},
    {"f10", IC_KEY_F10},
    {"f11", IC_KEY_F11},
    {"f12", IC_KEY_F12},
};

static const keybinding_profile_t* keybinding_profile_lookup(const char* name) {
    if (name == NULL)
        return NULL;
    for (size_t i = 0; i < keybinding_profile_count(); ++i) {
        const keybinding_profile_t* profile = keybinding_profiles[i];
        if (profile != NULL && ic_stricmp(name, profile->name) == 0)
            return profile;
    }
    return NULL;
}

static const char* keybinding_profile_find_spec(const keybinding_profile_t* profile,
                                                ic_key_action_t action) {
    if (profile == NULL)
        return NULL;
    for (size_t i = 0; i < profile->spec_count; ++i) {
        if (profile->specs[i].action == action)
            return profile->specs[i].specs;
    }
    return keybinding_profile_find_spec(profile->parent, action);
}

static bool keybinding_profile_bind_string(ic_env_t* env, ic_key_action_t action,
                                           const char* specs) {
    if (env == NULL || specs == NULL)
        return true;
    const char* p = specs;
    while (*p != '\0') {
        while (*p == ' ' || *p == '\t' || *p == '|')
            p++;
        if (*p == '\0')
            break;
        const char* start = p;
        while (*p != '\0' && *p != '|')
            p++;
        const char* end = p;
        while (start < end && (end[-1] == ' ' || end[-1] == '\t'))
            end--;
        while (start < end && (*start == ' ' || *start == '\t'))
            start++;
        if (start >= end)
            continue;
        size_t len = (size_t)(end - start);
        if (len >= 64)
            return false;
        char token[64];
        memcpy(token, start, len);
        token[len] = '\0';
        ic_keycode_t key;
        if (!ic_parse_key_spec(token, &key))
            return false;
        if (!ic_bind_key(key, action))
            return false;
    }
    return true;
}

static bool keybinding_profile_apply_bindings(ic_env_t* env, const keybinding_profile_t* profile) {
    if (env == NULL || profile == NULL)
        return true;
    if (!keybinding_profile_apply_bindings(env, profile->parent))
        return false;
    for (size_t i = 0; i < profile->binding_count; ++i) {
        const keybinding_profile_binding_t* binding = &profile->bindings[i];
        if (!keybinding_profile_bind_string(env, binding->action, binding->specs))
            return false;
    }
    return true;
}

static void key_binding_clear_all(ic_env_t* env) {
    if (env == NULL)
        return;
    env->key_binding_count = 0;
}

static bool key_lookup_named(const char* token, ic_keycode_t* out_key) {
    for (size_t i = 0; i < sizeof(key_name_map) / sizeof(key_name_map[0]); ++i) {
        if (ic_stricmp(token, key_name_map[i].name) == 0) {
            *out_key = key_name_map[i].key;
            return true;
        }
    }
    // F-keys beyond explicit table
    if (token[0] == 'f' || token[0] == 'F') {
        char* endptr = NULL;
        long number = strtol(token + 1, &endptr, 10);
        if (endptr != token + 1 && *endptr == '\0' && number >= 1 && number <= 24) {
            *out_key = (ic_keycode_t)(IC_KEY_F1 + (number - 1));
            return true;
        }
    }
    return false;
}

static bool append_token(bool* first, char* buffer, size_t buflen, size_t* len, const char* token) {
    size_t token_len = strlen(token);
    size_t extra = (*first ? 0 : 1);
    if (*len + extra + token_len + 1 > buflen)
        return false;
    if (!*first) {
        buffer[*len] = '+';
        (*len)++;
    }
    memcpy(buffer + *len, token, token_len);
    *len += token_len;
    buffer[*len] = '\0';
    *first = false;
    return true;
}

//-------------------------------------------------------------
// Readline
//-------------------------------------------------------------

static char* ic_getline(alloc_t* mem);

ic_public char* ic_readline(const char* prompt_text, const char* initial_input) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return NULL;
    if (!env->noedit) {
        // terminal editing enabled
        if (initial_input != NULL) {
            ic_env_set_initial_input(env, initial_input);
        }
        char* result = ic_editline(env, prompt_text);  // in editline.c
        ic_env_clear_initial_input(env);
        return result;
    } else {
        // no editing capability (pipe, dumb terminal, etc)
        if (env->tty != NULL && env->term != NULL) {
            // if the terminal is not interactive, but we are reading from the
            // tty (keyboard), we display a prompt
            term_start_raw(env->term);  // set utf8 mode on windows
            if (prompt_text != NULL) {
                term_write(env->term, prompt_text);
            }
            term_write(env->term, env->prompt_marker);
            term_end_raw(env->term, false);
        }
        // read directly from stdin
        return ic_getline(env->mem);
    }
}

ic_public char* ic_readline_inline(const char* prompt_text, const char* inline_right_text,
                                   const char* initial_input) {
    // fprintf(stderr, "DEBUG: ic_readline_inline called with prompt='%s',
    // inline_right='%s'\n",
    //         prompt_text ? prompt_text : "NULL",
    //         inline_right_text ? inline_right_text : "NULL");
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return NULL;
    if (!env->noedit) {
        // terminal editing enabled
        if (initial_input != NULL) {
            ic_env_set_initial_input(env, initial_input);
        }
        char* result = ic_editline_inline(env, prompt_text,
                                          inline_right_text);  // in editline.c
        ic_env_clear_initial_input(env);
        return result;
    } else {
        // no editing capability (pipe, dumb terminal, etc)
        // For fallback mode, just use regular readline behavior
        return ic_readline(prompt_text, initial_input);
    }
}

//-------------------------------------------------------------
// Read a line from the stdin stream if there is no editing
// support (like from a pipe, file, or dumb terminal).
//-------------------------------------------------------------

static char* ic_getline(alloc_t* mem) {
    // reset interrupt flag
    getline_interrupt = false;

    // read until eof or newline
    stringbuf_t* sb = sbuf_new(mem);
    int c;
    while (true) {
        c = fgetc(stdin);
        if (c == EOF || c == '\n') {
            break;
        } else {
            sbuf_append_char(sb, (char)c);
        }
        // check if we've been interrupted
        if (getline_interrupt) {
            break;
        }
    }
    return sbuf_free_dup(sb);
}

//-------------------------------------------------------------
// Formatted output
//-------------------------------------------------------------

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

//-------------------------------------------------------------
// Interface
//-------------------------------------------------------------

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

static void set_prompt_marker(ic_env_t* env, const char* prompt_marker,
                              const char* cprompt_marker) {
    if (prompt_marker == NULL)
        prompt_marker = "> ";
    if (cprompt_marker == NULL)
        cprompt_marker = prompt_marker;
    mem_free(env->mem, env->prompt_marker);
    mem_free(env->mem, env->cprompt_marker);
    env->prompt_marker = mem_strdup(env->mem, prompt_marker);
    env->cprompt_marker = mem_strdup(env->mem, cprompt_marker);
}

ic_public const char* ic_get_prompt_marker(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return NULL;
    return env->prompt_marker;
}

ic_public const char* ic_get_continuation_prompt_marker(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return NULL;
    return env->cprompt_marker;
}

ic_public void ic_set_prompt_marker(const char* prompt_marker, const char* cprompt_marker) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    set_prompt_marker(env, prompt_marker, cprompt_marker);
}

ic_public bool ic_enable_multiline(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->singleline_only;
    env->singleline_only = !enable;
    return !prev;
}

ic_public bool ic_enable_beep(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    return term_enable_beep(env->term, enable);
}

ic_public bool ic_enable_color(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    return term_enable_color(env->term, enable);
}

ic_public bool ic_enable_history_duplicates(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    return history_enable_duplicates(env->history, enable);
}

ic_public void ic_set_history(const char* fname, long max_entries) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    history_load_from(env->history, fname, max_entries);
}

ic_public void ic_history_remove_last(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    history_remove_last(env->history);
}

ic_public void ic_history_add(const char* entry) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    history_push(env->history, entry);
    // append to history file immediately (with timestamp)
    history_save(env->history);
}

ic_public void ic_history_clear(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    history_clear(env->history);
}

ic_public bool ic_enable_auto_tab(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->complete_autotab;
    env->complete_autotab = enable;
    return prev;
}

ic_public bool ic_enable_completion_preview(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->complete_nopreview;
    env->complete_nopreview = !enable;
    return !prev;
}

ic_public bool ic_enable_multiline_indent(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->no_multiline_indent;
    env->no_multiline_indent = !enable;
    return !prev;
}

ic_public bool ic_enable_hint(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->no_hint;
    env->no_hint = !enable;
    return !prev;
}

ic_public long ic_set_hint_delay(long delay_ms) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    long prev = env->hint_delay;
    env->hint_delay = (delay_ms < 0 ? 0 : (delay_ms > 5000 ? 5000 : delay_ms));
    return prev;
}

ic_public void ic_set_tty_esc_delay(long initial_delay_ms, long followup_delay_ms) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    if (env->tty == NULL)
        return;
    tty_set_esc_delay(env->tty, initial_delay_ms, followup_delay_ms);
}

ic_public bool ic_enable_highlight(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->no_highlight;
    env->no_highlight = !enable;
    return !prev;
}

ic_public bool ic_enable_inline_help(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->no_help;
    env->no_help = !enable;
    return !prev;
}

ic_public bool ic_enable_prompt_cleanup(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->prompt_cleanup;
    env->prompt_cleanup = enable;
    return prev;
}

ic_public bool ic_enable_prompt_cleanup_empty_line(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->prompt_cleanup_add_empty_line;
    env->prompt_cleanup_add_empty_line = enable;
    return prev;
}

ic_public bool ic_enable_brace_matching(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->no_bracematch;
    env->no_bracematch = !enable;
    return !prev;
}

ic_public void ic_set_matching_braces(const char* brace_pairs) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    mem_free(env->mem, env->match_braces);
    env->match_braces = NULL;
    if (brace_pairs != NULL) {
        ssize_t len = ic_strlen(brace_pairs);
        if (len > 0 && (len % 2) == 0) {
            env->match_braces = mem_strdup(env->mem, brace_pairs);
        }
    }
}

ic_public bool ic_enable_brace_insertion(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->no_autobrace;
    env->no_autobrace = !enable;
    return !prev;
}

ic_public void ic_set_insertion_braces(const char* brace_pairs) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    mem_free(env->mem, env->auto_braces);
    env->auto_braces = NULL;
    if (brace_pairs != NULL) {
        ssize_t len = ic_strlen(brace_pairs);
        if (len > 0 && (len % 2) == 0) {
            env->auto_braces = mem_strdup(env->mem, brace_pairs);
        }
    }
}

ic_public ic_key_action_t ic_key_action_from_name(const char* name) {
    if (name == NULL)
        return IC_KEY_ACTION__MAX;
    for (size_t i = 0; i < key_action_name_count(); ++i) {
        if (ic_stricmp(name, key_action_names[i].name) == 0)
            return key_action_names[i].action;
    }
    return IC_KEY_ACTION__MAX;
}

ic_public const char* ic_key_action_name(ic_key_action_t action) {
    if (action < IC_KEY_ACTION_NONE || action >= IC_KEY_ACTION__MAX)
        return NULL;
    for (size_t i = 0; i < key_action_name_count(); ++i) {
        if (key_action_names[i].action == action)
            return key_action_names[i].name;
    }
    return NULL;
}

ic_public bool ic_bind_key(ic_keycode_t key, ic_key_action_t action) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    if (action < IC_KEY_ACTION_NONE || action >= IC_KEY_ACTION__MAX)
        return false;
    ssize_t index = -1;
    ic_key_binding_entry_t* entry = key_binding_find_entry(env, key, &index);
    if (entry != NULL) {
        entry->action = action;
        return true;
    }
    if (!key_bindings_ensure_capacity(env, env->key_binding_count + 1))
        return false;
    env->key_bindings[env->key_binding_count].key = key;
    env->key_bindings[env->key_binding_count].action = action;
    env->key_binding_count++;
    return true;
}

ic_public bool ic_clear_key_binding(ic_keycode_t key) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    ssize_t index = -1;
    if (key_binding_find_entry(env, key, &index) == NULL)
        return false;
    for (ssize_t i = index; i < env->key_binding_count - 1; ++i) {
        env->key_bindings[i] = env->key_bindings[i + 1];
    }
    if (env->key_binding_count > 0)
        env->key_binding_count--;
    return true;
}

ic_public void ic_reset_key_bindings(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    key_binding_clear_all(env);
    if (env->key_binding_profile != NULL) {
        keybinding_profile_apply_bindings(env, env->key_binding_profile);
    }
}

ic_public bool ic_get_key_binding(ic_keycode_t key, ic_key_action_t* out_action) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    ic_key_binding_entry_t* entry = key_binding_find_entry(env, key, NULL);
    if (entry == NULL)
        return false;
    if (out_action != NULL)
        *out_action = entry->action;
    return true;
}

ic_public size_t ic_list_key_bindings(ic_key_binding_entry_t* buffer, size_t capacity) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return 0;
    size_t count = to_size_t(env->key_binding_count);
    if (buffer == NULL || capacity == 0)
        return count;
    size_t limit = (count < capacity ? count : capacity);
    for (size_t i = 0; i < limit; ++i) {
        buffer[i] = env->key_bindings[i];
    }
    return limit;
}

ic_public bool ic_parse_key_spec(const char* spec, ic_keycode_t* out_key) {
    if (spec == NULL || out_key == NULL)
        return false;
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
    bool base_set = false;
    bool base_is_char = false;
    char base_char = '\0';
    ic_keycode_t base_key = 0;

    char token[32];
    size_t tok_len = 0;
    for (const char* p = spec;; ++p) {
        char ch = *p;
        bool at_end = (ch == '\0');
        if (at_end || ch == '+' || ch == '-' || ch == ' ' || ch == '\t') {
            if (tok_len > 0) {
                token[tok_len] = '\0';
                for (size_t i = 0; i < tok_len; ++i) {
                    token[i] = (char)tolower((unsigned char)token[i]);
                }
                if (strcmp(token, "ctrl") == 0 || strcmp(token, "control") == 0 ||
                    strcmp(token, "c") == 0) {
                    ctrl = true;
                } else if (strcmp(token, "alt") == 0 || strcmp(token, "meta") == 0 ||
                           strcmp(token, "option") == 0) {
                    alt = true;
                } else if (strcmp(token, "shift") == 0 || strcmp(token, "s") == 0) {
                    shift = true;
                } else {
                    if (base_set)
                        return false;
                    if (tok_len == 1) {
                        base_is_char = true;
                        base_char = token[0];
                        base_set = true;
                    } else {
                        ic_keycode_t named;
                        if (key_lookup_named(token, &named)) {
                            base_key = named;
                            base_is_char = false;
                            base_set = true;
                        } else if (strcmp(token, "newline") == 0) {
                            base_key = IC_KEY_LINEFEED;
                            base_is_char = false;
                            base_set = true;
                        } else {
                            return false;
                        }
                    }
                }
                tok_len = 0;
            }
            if (at_end)
                break;
        } else {
            if (tok_len + 1 >= sizeof(token))
                return false;
            token[tok_len++] = ch;
        }
    }

    if (!base_set)
        return false;

    ic_keycode_t code = 0;
    if (base_is_char) {
        unsigned char ch = (unsigned char)base_char;
        if (ctrl) {
            if (ch >= 'a' && ch <= 'z') {
                code = (ic_keycode_t)(IC_KEY_CTRL_A + (ch - 'a'));
                ctrl = false;
            } else if (ch >= 'A' && ch <= 'Z') {
                code = (ic_keycode_t)(IC_KEY_CTRL_A + (ch - 'A'));
                ctrl = false;
            } else {
                code = IC_KEY_WITH_CTRL(ic_key_char((char)ch));
                ctrl = false;
            }
        } else {
            code = ic_key_char((char)ch);
        }
    } else {
        code = base_key;
    }

    if (ctrl)
        code = IC_KEY_WITH_CTRL(code);
    if (alt)
        code = IC_KEY_WITH_ALT(code);
    if (shift)
        code = IC_KEY_WITH_SHIFT(code);

    *out_key = code;
    return true;
}

ic_public bool ic_bind_key_named(const char* key_spec, const char* action_name) {
    if (key_spec == NULL || action_name == NULL)
        return false;
    ic_keycode_t key;
    if (!ic_parse_key_spec(key_spec, &key))
        return false;
    ic_key_action_t action = ic_key_action_from_name(action_name);
    if (action == IC_KEY_ACTION__MAX)
        return false;
    return ic_bind_key(key, action);
}

ic_public bool ic_format_key_spec(ic_keycode_t key, char* buffer, size_t buflen) {
    if (buffer == NULL || buflen == 0)
        return false;
    buffer[0] = '\0';
    size_t len = 0;
    bool first = true;

    ic_keycode_t mods = IC_KEY_MODS(key);
    bool implicit_ctrl = false;
    ic_keycode_t base = key;
    if ((mods & IC_KEY_MOD_CTRL) == 0 && key >= IC_KEY_CTRL_A && key <= IC_KEY_CTRL_Z) {
        implicit_ctrl = true;
        base = key;
    } else {
        base = IC_KEY_NO_MODS(key);
    }

    if ((mods & IC_KEY_MOD_CTRL) != 0 || implicit_ctrl) {
        if (!append_token(&first, buffer, buflen, &len, "ctrl"))
            return false;
    }
    if (mods & IC_KEY_MOD_ALT) {
        if (!append_token(&first, buffer, buflen, &len, "alt"))
            return false;
    }
    if (mods & IC_KEY_MOD_SHIFT) {
        if (!append_token(&first, buffer, buflen, &len, "shift"))
            return false;
    }

    char base_buf[16];
    const char* base_name = NULL;
    if (implicit_ctrl) {
        base_buf[0] = (char)('a' + (int)(base - IC_KEY_CTRL_A));
        base_buf[1] = '\0';
        base_name = base_buf;
    } else if (base >= IC_KEY_F1 && base <= IC_KEY_F1 + 23) {
        unsigned number = 1u + (unsigned)(base - IC_KEY_F1);
        if (number > 24)
            return false;
        snprintf(base_buf, sizeof(base_buf), "f%u", number);
        base_name = base_buf;
    } else {
        switch (base) {
            case IC_KEY_TAB:
                base_name = "tab";
                break;
            case IC_KEY_ENTER:
                base_name = "enter";
                break;
            case IC_KEY_LINEFEED:
                base_name = "linefeed";
                break;
            case IC_KEY_BACKSP:
                base_name = "backspace";
                break;
            case IC_KEY_DEL:
                base_name = "delete";
                break;
            case IC_KEY_INS:
                base_name = "insert";
                break;
            case IC_KEY_ESC:
                base_name = "esc";
                break;
            case IC_KEY_SPACE:
                base_name = "space";
                break;
            case IC_KEY_LEFT:
                base_name = "left";
                break;
            case IC_KEY_RIGHT:
                base_name = "right";
                break;
            case IC_KEY_UP:
                base_name = "up";
                break;
            case IC_KEY_DOWN:
                base_name = "down";
                break;
            case IC_KEY_HOME:
                base_name = "home";
                break;
            case IC_KEY_END:
                base_name = "end";
                break;
            case IC_KEY_PAGEUP:
                base_name = "pageup";
                break;
            case IC_KEY_PAGEDOWN:
                base_name = "pagedown";
                break;
            default:
                break;
        }
    }

    if (base_name == NULL) {
        if (base <= 0x7F && base >= 32) {
            base_buf[0] = (char)base;
            base_buf[1] = '\0';
            base_name = base_buf;
        } else if (base == IC_KEY_NONE) {
            base_name = "";
        } else {
            return false;
        }
    }

    if (base_name[0] != '\0') {
        if (!append_token(&first, buffer, buflen, &len, base_name))
            return false;
    }

    if (first) {
        return append_token(&first, buffer, buflen, &len, "none");
    }

    return true;
}

ic_public bool ic_set_key_binding_profile(const char* name) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    const keybinding_profile_t* profile =
        (name == NULL ? &keybinding_profile_default : keybinding_profile_lookup(name));
    if (profile == NULL)
        return false;
    if (env->key_binding_profile == profile) {
        key_binding_clear_all(env);
        return keybinding_profile_apply_bindings(env, profile);
    }
    const keybinding_profile_t* previous =
        (env->key_binding_profile != NULL ? env->key_binding_profile : &keybinding_profile_default);
    env->key_binding_profile = profile;
    key_binding_clear_all(env);
    if (!keybinding_profile_apply_bindings(env, profile)) {
        env->key_binding_profile = previous;
        key_binding_clear_all(env);
        keybinding_profile_apply_bindings(env, env->key_binding_profile);
        return false;
    }
    return true;
}

ic_public const char* ic_get_key_binding_profile(void) {
    ic_env_t* env = ic_get_env();
    const keybinding_profile_t* profile =
        (env != NULL && env->key_binding_profile != NULL ? env->key_binding_profile
                                                         : &keybinding_profile_default);
    return profile->name;
}

ic_public size_t ic_list_key_binding_profiles(ic_key_binding_profile_info_t* buffer,
                                              size_t capacity) {
    size_t count = keybinding_profile_count();
    if (buffer == NULL || capacity == 0)
        return count;
    size_t limit = (count < capacity ? count : capacity);
    for (size_t i = 0; i < limit; ++i) {
        buffer[i].name = keybinding_profiles[i]->name;
        buffer[i].description = keybinding_profiles[i]->description;
    }
    return limit;
}

ic_public const char* ic_key_binding_profile_default_specs(ic_key_action_t action) {
    if (action <= IC_KEY_ACTION_NONE || action >= IC_KEY_ACTION__MAX)
        return NULL;
    ic_env_t* env = ic_get_env();
    const keybinding_profile_t* profile =
        (env != NULL && env->key_binding_profile != NULL ? env->key_binding_profile
                                                         : &keybinding_profile_default);
    return keybinding_profile_find_spec(profile, action);
}

ic_private const char* ic_env_get_match_braces(ic_env_t* env) {
    return (env->match_braces == NULL ? "()[]{}" : env->match_braces);
}

ic_private const char* ic_env_get_auto_braces(ic_env_t* env) {
    return (env->auto_braces == NULL ? "()[]{}\"\"''" : env->auto_braces);
}

ic_private void ic_env_set_initial_input(ic_env_t* env, const char* initial_input) {
    if (env == NULL)
        return;
    mem_free(env->mem, (void*)env->initial_input);
    env->initial_input = NULL;
    if (initial_input != NULL) {
        env->initial_input = mem_strdup(env->mem, initial_input);
    }
}

ic_private void ic_env_clear_initial_input(ic_env_t* env) {
    if (env == NULL)
        return;
    mem_free(env->mem, (void*)env->initial_input);
    env->initial_input = NULL;
}

ic_public void ic_set_default_highlighter(ic_highlight_fun_t* highlighter, void* arg) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    env->highlighter = highlighter;
    env->highlighter_arg = arg;
}

ic_public void ic_free(void* p) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    mem_free(env->mem, p);
}

ic_public void* ic_malloc(size_t sz) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return NULL;
    return mem_malloc(env->mem, to_ssize_t(sz));
}

ic_public const char* ic_strdup(const char* s) {
    if (s == NULL)
        return NULL;
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return NULL;
    ssize_t len = ic_strlen(s);
    char* p = mem_malloc_tp_n(env->mem, char, len + 1);
    if (p == NULL)
        return NULL;
    ic_memcpy(p, s, len);
    p[len] = 0;
    return p;
}

//-------------------------------------------------------------
// Terminal
//-------------------------------------------------------------

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

ic_public void ic_print_prompt(const char* prompt_text, bool continuation_line) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->term == NULL || env->bbcode == NULL) {
        return;
    }

    // Make sure terminal is ready for output
    term_start_raw(env->term);

    // Create temporary editor state with the prompt text
    const char* text = (prompt_text != NULL ? prompt_text : "");

    // Style the prompt
    bbcode_style_open(env->bbcode, "ic-prompt");

    // Print the prompt text first
    if (!continuation_line) {
        bbcode_print(env->bbcode, text);
    } else if (!env->no_multiline_indent) {
        // Implement multiline continuation indentation like in
        // edit_write_prompt
        ssize_t textw = bbcode_column_width(env->bbcode, text);
        ssize_t markerw = bbcode_column_width(env->bbcode, env->prompt_marker);
        ssize_t cmarkerw = bbcode_column_width(env->bbcode, env->cprompt_marker);
        if (cmarkerw < markerw + textw) {
            term_write_repeat(env->term, " ", markerw + textw - cmarkerw);
        }
    }

    // Print the marker
    bbcode_print(env->bbcode, (continuation_line ? env->cprompt_marker : env->prompt_marker));

    // Close the style
    bbcode_style_close(env->bbcode, NULL);

    // Flush output to ensure it's displayed
    term_flush(env->term);
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

//-------------------------------------------------------------
// Readline with temporary completer and highlighter
//-------------------------------------------------------------

ic_public char* ic_readline_ex(const char* prompt_text, ic_completer_fun_t* completer,
                               void* completer_arg, ic_highlight_fun_t* highlighter,
                               void* highlighter_arg) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return NULL;
    // save previous
    ic_completer_fun_t* prev_completer;
    void* prev_completer_arg;
    completions_get_completer(env->completions, &prev_completer, &prev_completer_arg);
    ic_highlight_fun_t* prev_highlighter = env->highlighter;
    void* prev_highlighter_arg = env->highlighter_arg;
    // call with current
    if (completer != NULL) {
        ic_set_default_completer(completer, completer_arg);
    }
    if (highlighter != NULL) {
        ic_set_default_highlighter(highlighter, highlighter_arg);
    }
    char* res = ic_readline(prompt_text, "");
    // restore previous
    ic_set_default_completer(prev_completer, prev_completer_arg);
    ic_set_default_highlighter(prev_highlighter, prev_highlighter_arg);
    return res;
}

//-------------------------------------------------------------
// Initialize
//-------------------------------------------------------------

static void ic_atexit(void);

static void ic_env_free(ic_env_t* env) {
    if (env == NULL)
        return;
    // disable bracketed‑paste before exit
    if (env->term != NULL) {
        term_write(env->term, "\x1b[?2004l");
    }
    // history_save is append-only on each command; no full rewrite on exit
    history_free(env->history);
    completions_free(env->completions);
    bbcode_free(env->bbcode);
    term_free(env->term);
    tty_free(env->tty);
    mem_free(env->mem, env->cprompt_marker);
    mem_free(env->mem, env->prompt_marker);
    mem_free(env->mem, env->match_braces);
    mem_free(env->mem, env->auto_braces);
    mem_free(env->mem, (void*)env->initial_input);
    mem_free(env->mem, env->key_bindings);
    env->prompt_marker = NULL;

    // and deallocate ourselves
    alloc_t* mem = env->mem;
    mem_free(mem, env);

    // and finally the custom memory allocation structure
    mem_free(mem, mem);
}

static ic_env_t* ic_env_create(ic_malloc_fun_t* _malloc, ic_realloc_fun_t* _realloc,
                               ic_free_fun_t* _free) {
    if (_malloc == NULL)
        _malloc = &malloc;
    if (_realloc == NULL)
        _realloc = &realloc;
    if (_free == NULL)
        _free = &free;
    // allocate
    alloc_t* mem = (alloc_t*)_malloc(sizeof(alloc_t));
    if (mem == NULL)
        return NULL;
    mem->malloc = _malloc;
    mem->realloc = _realloc;
    mem->free = _free;
    ic_env_t* env = mem_zalloc_tp(mem, ic_env_t);
    if (env == NULL) {
        mem->free(mem);
        return NULL;
    }
    env->mem = mem;

    // Initialize
    env->tty = tty_new(env->mem, -1);
    env->term = term_new(env->mem, env->tty, false, false, -1);
    // enable bracketed‑paste in POSIX terminals
    if (env->term != NULL) {
        term_write(env->term, "\x1b[?2004h");
    }
    env->history = history_new(env->mem);
    env->completions = completions_new(env->mem);
    env->bbcode = bbcode_new(env->mem, env->term);

    env->hint_delay = 400;

    if (env->tty == NULL || env->term == NULL || env->completions == NULL || env->history == NULL ||
        env->bbcode == NULL || !term_is_interactive(env->term)) {
        env->noedit = true;
    }
    env->multiline_eol = '\\';

    bbcode_style_def(env->bbcode, "ic-prompt", "ansi-green");
    bbcode_style_def(env->bbcode, "ic-info", "ansi-darkgray");
    bbcode_style_def(env->bbcode, "ic-diminish", "ansi-lightgray");
    bbcode_style_def(env->bbcode, "ic-emphasis", "#ffffd7");
    bbcode_style_def(env->bbcode, "ic-hint", "ansi-darkgray");
    bbcode_style_def(env->bbcode, "ic-error", "#d70000");
    bbcode_style_def(env->bbcode, "ic-bracematch",
                     "ansi-white");  //  color = #F7DC6F" );

    bbcode_style_def(env->bbcode, "keyword", "#569cd6");
    bbcode_style_def(env->bbcode, "control", "#c586c0");
    bbcode_style_def(env->bbcode, "number", "#b5cea8");
    bbcode_style_def(env->bbcode, "string", "#ce9178");
    bbcode_style_def(env->bbcode, "comment", "#6A9955");
    bbcode_style_def(env->bbcode, "type", "darkcyan");
    bbcode_style_def(env->bbcode, "constant", "#569cd6");

    set_prompt_marker(env, NULL, NULL);
    env->key_binding_profile = &keybinding_profile_default;
    return env;
}

static ic_env_t* rpenv;

static void ic_atexit(void) {
    if (rpenv != NULL) {
        ic_env_free(rpenv);
        rpenv = NULL;
    }
}

ic_private ic_env_t* ic_get_env(void) {
    if (rpenv == NULL) {
        rpenv = ic_env_create(NULL, NULL, NULL);
        if (rpenv != NULL) {
            atexit(&ic_atexit);
        }
    }
    return rpenv;
}

ic_public void ic_init_custom_malloc(ic_malloc_fun_t* _malloc, ic_realloc_fun_t* _realloc,
                                     ic_free_fun_t* _free) {
    assert(rpenv == NULL);
    if (rpenv != NULL) {
        ic_env_free(rpenv);
        rpenv = ic_env_create(_malloc, _realloc, _free);
    } else {
        rpenv = ic_env_create(_malloc, _realloc, _free);
        if (rpenv != NULL) {
            atexit(&ic_atexit);
        }
    }
}

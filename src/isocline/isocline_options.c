/* ----------------------------------------------------------------------------
    Copyright (c) 2021, Daan Leijen
    Largely Modified by Caden Finley 2025 for CJ's Shell
    This is free software; you can redistribute it and/or modify it
    under the terms of the MIT License. A copy of the license can be
    found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/

/* ----------------------------------------------------------------------------
    Runtime configuration helpers split from the original isocline.c file.
-----------------------------------------------------------------------------*/

#include "common.h"
#include "env.h"
#include "env_internal.h"

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
    ic_env_apply_prompt_markers(env, prompt_marker, cprompt_marker);
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
    if (env->term == NULL)
        return false;
    return term_enable_beep(env->term, enable);
}

ic_public bool ic_enable_color(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    if (env->term == NULL)
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
        return 0;
    long prev = env->hint_delay;
    if (delay_ms < 0) {
        env->hint_delay = 0;
    } else if (delay_ms > 5000) {
        env->hint_delay = 5000;
    } else {
        env->hint_delay = delay_ms;
    }
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

ic_public bool ic_enable_prompt_cleanup(bool enable, size_t extra_lines) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->prompt_cleanup;
    env->prompt_cleanup = enable;
    env->prompt_cleanup_extra_lines = extra_lines;
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
    p[len] = '\0';
    return p;
}

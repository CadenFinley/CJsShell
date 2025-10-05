/* ----------------------------------------------------------------------------
    Copyright (c) 2021, Daan Leijen
    Largely Modified by Caden Finley 2025 for CJ's Shell
    This is free software; you can redistribute it and/or modify it
    under the terms of the MIT License. A copy of the license can be
    found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/

/* ----------------------------------------------------------------------------
    Environment lifecycle management extracted from the original isocline.c.
-----------------------------------------------------------------------------*/

#include <stdlib.h>

#include "common.h"
#include "env.h"
#include "env_internal.h"
#include "keybinding_internal.h"

//-------------------------------------------------------------
// Prompt helpers shared with other modules
//-------------------------------------------------------------

ic_private void ic_env_apply_prompt_markers(ic_env_t* env, const char* prompt_marker,
                                            const char* continuation_prompt_marker) {
    if (env == NULL)
        return;
    if (prompt_marker == NULL)
        prompt_marker = "> ";
    if (continuation_prompt_marker == NULL)
        continuation_prompt_marker = prompt_marker;
    mem_free(env->mem, env->prompt_marker);
    mem_free(env->mem, env->cprompt_marker);
    env->prompt_marker = mem_strdup(env->mem, prompt_marker);
    env->cprompt_marker = mem_strdup(env->mem, continuation_prompt_marker);
}

//-------------------------------------------------------------
// Environment allocation & teardown
//-------------------------------------------------------------

static void ic_atexit(void);

static ic_env_t* ic_env_create(ic_malloc_fun_t* _malloc, ic_realloc_fun_t* _realloc,
                               ic_free_fun_t* _free) {
    if (_malloc == NULL)
        _malloc = &malloc;
    if (_realloc == NULL)
        _realloc = &realloc;
    if (_free == NULL)
        _free = &free;
    // allocate allocator wrapper
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

    env->tty = tty_new(env->mem, -1);
    env->term = term_new(env->mem, env->tty, false, false, -1);
    if (env->term != NULL) {
        // enable bracketed-paste
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
    bbcode_style_def(env->bbcode, "ic-bracematch", "ansi-white");

    bbcode_style_def(env->bbcode, "keyword", "#569cd6");
    bbcode_style_def(env->bbcode, "control", "#c586c0");
    bbcode_style_def(env->bbcode, "number", "#b5cea8");
    bbcode_style_def(env->bbcode, "string", "#ce9178");
    bbcode_style_def(env->bbcode, "comment", "#6A9955");
    bbcode_style_def(env->bbcode, "type", "darkcyan");
    bbcode_style_def(env->bbcode, "constant", "#569cd6");

    ic_env_apply_prompt_markers(env, NULL, NULL);
    env->key_binding_profile = ic_keybinding_profile_default_ptr();

    return env;
}

static void ic_env_free(ic_env_t* env) {
    if (env == NULL)
        return;
    if (env->term != NULL) {
        term_write(env->term, "\x1b[?2004l");
    }
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

    alloc_t* mem = env->mem;
    mem_free(mem, env);
    mem_free(mem, mem);
}

//-------------------------------------------------------------
// Global environment accessors
//-------------------------------------------------------------

static ic_env_t* rpenv = NULL;

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

/*
  isocline_env.c

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

ic_private void ic_emit_continuation_indent(ic_env_t* env, const char* prompt_text) {
    if (env == NULL || env->no_multiline_indent || env->term == NULL || env->bbcode == NULL)
        return;
    const char* text = (prompt_text != NULL ? prompt_text : "");
    ssize_t textw = bbcode_column_width(env->bbcode, text);
    ssize_t markerw = bbcode_column_width(env->bbcode, env->prompt_marker);
    ssize_t cmarkerw = bbcode_column_width(env->bbcode, env->cprompt_marker);
    if (cmarkerw < markerw + textw) {
        term_write_repeat(env->term, " ", markerw + textw - cmarkerw);
    }
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

    // Set default enabled features
    env->hint_delay = 0;                        // hint delay (0)
    env->spell_correct = true;                  // completion spell fixing
    env->show_line_numbers = true;              // line numbers
    env->relative_line_numbers = false;         // absolute numbering by default
    env->highlight_current_line_number = true;  // highlight current line number by default
    env->allow_line_numbers_with_continuation_prompt = false;  // keep legacy suppression by default
    env->replace_prompt_line_with_line_number = false;  // keep final prompt line visible by default
    env->complete_nopreview = false;      // completion preview (inverted: false = enabled)
    env->no_hint = false;                 // hint (inverted: false = enabled)
    env->complete_autotab = false;        // auto tab (disabled by default)
    env->no_help = false;                 // inline help (inverted: false = enabled)
    env->no_multiline_indent = false;     // multiline indent (inverted: false = enabled)
    env->singleline_only = false;         // multiline (inverted: false = enabled)
    env->multiline_start_line_count = 1;  // preallocated prompt lines when multiline is on
    env->status_hint_mode = IC_STATUS_HINT_NORMAL;    // default to legacy behavior
    env->inline_right_prompt_follows_cursor = false;  // keep right prompt anchored at row 0

    if (env->tty == NULL || env->term == NULL || env->completions == NULL || env->history == NULL ||
        env->bbcode == NULL || !term_is_interactive(env->term)) {
        env->noedit = true;
    }
    env->multiline_eol = '\\';

    bbcode_style_def(env->bbcode, "ic-prompt", "ansi-white");
    bbcode_style_def(env->bbcode, "ic-linenumbers", "ansi-lightgray");
    bbcode_style_def(env->bbcode, "ic-linenumber-current", "ansi-yellow");
    bbcode_style_def(env->bbcode, "ic-info", "ansi-darkgray");
    bbcode_style_def(env->bbcode, "ic-status", "ansi-lightgray");
    bbcode_style_def(env->bbcode, "ic-source", "#ffffd7");
    bbcode_style_def(env->bbcode, "ic-diminish", "ansi-lightgray");
    bbcode_style_def(env->bbcode, "ic-emphasis", "#ffffd7");
    bbcode_style_def(env->bbcode, "ic-hint", "ansi-darkgray");
    bbcode_style_def(env->bbcode, "ic-error", "#d70000");
    bbcode_style_def(env->bbcode, "ic-bracematch", "ansi-white");
    bbcode_style_def(env->bbcode, "ic-whitespace-char", "ansi-lightgray");

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
    if (env->abbreviations != NULL) {
        for (ssize_t i = 0; i < env->abbreviation_count; ++i) {
            mem_free(env->mem, env->abbreviations[i].trigger);
            mem_free(env->mem, env->abbreviations[i].expansion);
        }
        mem_free(env->mem, env->abbreviations);
        env->abbreviations = NULL;
        env->abbreviation_count = 0;
        env->abbreviation_capacity = 0;
    }
    mem_free(env->mem, env->cprompt_marker);
    mem_free(env->mem, env->prompt_marker);
    mem_free(env->mem, env->match_braces);
    mem_free(env->mem, env->auto_braces);
    mem_free(env->mem, (void*)env->initial_input);
    mem_free(env->mem, env->key_bindings);
    mem_free(env->mem, env->whitespace_marker);
    env->prompt_marker = NULL;

    alloc_t* mem = env->mem;
    mem_free(mem, env);
    mem_free(mem, mem);
}

//-------------------------------------------------------------
// Global environment accessors
//-------------------------------------------------------------

static ic_env_t* rpenv = NULL;
static bool ic_default_abbreviations_initialized = false;

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
            (void)atexit(&ic_atexit);
        }
    }
    if (!ic_default_abbreviations_initialized && rpenv != NULL) {
        ic_default_abbreviations_initialized = true;
        (void)ic_add_abbreviation("abbr", "abbreviate");
        (void)ic_add_abbreviation("unabbr", "unabbreviate");
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
            (void)atexit(&ic_atexit);
        }
    }
}

ic_private const char* ic_env_get_match_braces(ic_env_t* env) {
    return (env->match_braces == NULL ? "()[]{}" : env->match_braces);
}

ic_private const char* ic_env_get_auto_braces(ic_env_t* env) {
    return (env->auto_braces == NULL ? "()[]{}\"\"''" : env->auto_braces);
}

ic_private const char* ic_env_get_whitespace_marker(ic_env_t* env) {
    static const char* default_marker = "\xC2\xB7";  // middle dot indicator
    if (env == NULL || env->whitespace_marker == NULL || env->whitespace_marker[0] == '\0') {
        return default_marker;
    }
    return env->whitespace_marker;
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

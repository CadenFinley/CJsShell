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

#pragma once
#ifndef IC_ENV_H
#define IC_ENV_H

#include <stddef.h>

#include "bbcode.h"
#include "common.h"
#include "completions.h"
#include "history.h"
#include "isocline.h"
#include "term.h"
#include "tty.h"

struct ic_keybinding_profile_s;
struct editor_s;

//-------------------------------------------------------------
// Environment
//-------------------------------------------------------------

typedef struct ic_abbreviation_entry_s {
    char* trigger;
    char* expansion;
    ssize_t trigger_len;
} ic_abbreviation_entry_t;

struct ic_env_s {
    alloc_t* mem;                     // potential custom allocator
    ic_env_t* next;                   // next environment (used for proper deallocation)
    term_t* term;                     // terminal
    tty_t* tty;                       // keyboard (NULL if stdin is a pipe, file, etc)
    struct editor_s* current_editor;  // pointer to active editor (NULL when not reading)
    completions_t* completions;       // current completions
    history_t* history;               // edit history
    bbcode_t* bbcode;                 // print with bbcodes
    const char* prompt_marker;        // the prompt marker (defaults to "> ")
    const char* cprompt_marker;       // prompt marker for continuation lines
                                      // (defaults to `prompt_marker`)
    ic_highlight_fun_t* highlighter;  // highlight callback
    void* highlighter_arg;            // user state for the highlighter.
    ic_unhandled_key_fun_t* unhandled_key_handler;     // callback for unhandled keys
    void* unhandled_key_arg;                           // user state for unhandled key handler
    ic_status_message_fun_t* status_message_callback;  // callback for status message text
    void* status_message_arg;                          // user state for status callback
    ic_status_hint_mode_t status_hint_mode;            // rendering behavior for default hints
    const char* match_braces;                          // matching braces, e.g "()[]{}"
    const char* auto_braces;                           // auto insertion braces, e.g "()[]{}\"\"''"
    const char* initial_input;                         // initial input text to insert into editor
    char multiline_eol;                  // character used for multiline input ("\") (set to 0
                                         // to disable)
    bool initialized;                    // are we initialized?
    bool noedit;                         // is rich editing possible (tty != NULL)
    bool singleline_only;                // allow only single line editing?
    bool complete_nopreview;             // do not show completion preview for each
                                         // selection in the completion menu?
    bool complete_autotab;               // try to keep completing after a completion?
    bool no_multiline_indent;            // indent continuation lines to line up under the
                                         // initial prompt
    bool no_help;                        // show short help line for history search etc.
    bool no_hint;                        // allow hinting?
    bool no_highlight;                   // enable highlighting?
    bool no_bracematch;                  // enable brace matching?
    bool no_autobrace;                   // enable automatic brace insertion?
    bool no_lscolors;                    // use LSCOLORS/LS_COLORS to colorize file name
                                         // completions?
    bool spell_correct;                  // enable spell correction on completions?
    bool show_line_numbers;              // show line numbers in multiline mode?
    bool relative_line_numbers;          // use relative line numbers when enabled?
    bool highlight_current_line_number;  // highlight the current line number differently?
    bool allow_line_numbers_with_continuation_prompt;  // keep line numbers when continuation
                                                       // prompts are active?
    bool replace_prompt_line_with_line_number;         // swap final prompt line with line numbers?
    bool show_whitespace_characters;                   // visualize spaces while editing?
    bool prompt_cleanup;                               // after enter, rewrite prompt inline?
    bool prompt_cleanup_add_empty_line;                // optionally add empty line after
                                                       // cleanup
    bool prompt_cleanup_truncate_multiline;            // during cleanup, collapse multiline input
    bool inline_right_prompt_follows_cursor;           // right prompt tracks cursor row
    size_t multiline_start_line_count;  // prefill multiline prompts with this many lines
    size_t prompt_cleanup_extra_lines;  // additional terminal lines to erase during cleanup
    long hint_delay;                    // delay before displaying a hint in milliseconds

    ic_key_binding_entry_t* key_bindings;  // dynamic array of custom key bindings
    ssize_t key_binding_count;
    ssize_t key_binding_capacity;
    const struct ic_keybinding_profile_s* key_binding_profile;

    ic_abbreviation_entry_t* abbreviations;
    ssize_t abbreviation_count;
    ssize_t abbreviation_capacity;

    char* whitespace_marker;  // custom marker used when visualizing spaces
};

ic_private char* ic_editline(ic_env_t* env, const char* prompt_text, const char* inline_right_text);

ic_private ic_env_t* ic_get_env(void);
ic_private const char* ic_env_get_auto_braces(ic_env_t* env);
ic_private const char* ic_env_get_match_braces(ic_env_t* env);
ic_private void ic_env_set_initial_input(ic_env_t* env, const char* initial_input);
ic_private void ic_env_clear_initial_input(ic_env_t* env);
ic_private const char* ic_env_get_whitespace_marker(ic_env_t* env);

#endif  // IC_ENV_H

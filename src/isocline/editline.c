/* ----------------------------------------------------------------------------
  Copyright (c) 2021, Daan Leijen
  Largely Modified by Caden Finley 2025 for CJ's Shell
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License. A copy of the license can be
  found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "completions.h"
#include "env.h"
#include "highlight.h"
#include "history.h"
#include "isocline.h"
#include "stringbuf.h"
#include "term.h"
#include "tty.h"
#include "undo.h"

//-------------------------------------------------------------
// The editor state
//-------------------------------------------------------------

// editor state
typedef struct editor_s {
    stringbuf_t* input;           // current user input
    stringbuf_t* extra;           // extra displayed info (for completion menu etc)
    stringbuf_t* hint;            // hint displayed as part of the input
    stringbuf_t* hint_help;       // help for a hint.
    stringbuf_t* history_prefix;  // cached prefix before history navigation
    ssize_t pos;                  // current cursor position in the input
    ssize_t cur_rows;             // current used rows to display our content (including
                                  // extra content)
    ssize_t cur_row;              // current row that has the cursor (0 based, relative to
                                  // the prompt)
    ssize_t termw;
    bool modified;                  // has a modification happened? (used for history navigation
                                    // for example)
    bool disable_undo;              // temporarily disable auto undo (for history search)
    bool history_prefix_active;     // whether prefix-prioritized history is active
    bool request_submit;            // request submission of current line
    ssize_t history_idx;            // current index in the history
    editstate_t* undo;              // undo buffer
    editstate_t* redo;              // redo buffer
    const char* prompt_text;        // text of the prompt before the prompt marker
    ssize_t prompt_prefix_lines;    // number of prefix lines emitted for prompt
    const char* inline_right_text;  // inline right-aligned text on input line
    const char* cached_inline_right_text;
    ssize_t inline_right_width;  // cached width of inline right text
    bool inline_right_width_valid;
    ssize_t line_number_column_width;  // cached total prefix width when line numbers are shown
    alloc_t* mem;                      // allocator
    bool prompt_width_cache_valid;
    ssize_t prompt_marker_width_cache;
    ssize_t prompt_text_width_cache;
    ssize_t prompt_total_width_cache;
    ssize_t cprompt_marker_width_cache;
    ssize_t indent_width_cache;
    unsigned long prompt_layout_generation_snapshot;
    stringbuf_t* inline_right_plain_cache;
    // caches
    attrbuf_t* attrs;  // reuse attribute buffers
    attrbuf_t* attrs_extra;
} editor_t;

static void edit_generate_completions(ic_env_t* env, editor_t* eb, bool autotab);
static void edit_history_search_with_current_word(ic_env_t* env, editor_t* eb);
static void edit_history_prev(ic_env_t* env, editor_t* eb);
static void edit_history_next(ic_env_t* env, editor_t* eb);
static void edit_clear_history_preview(editor_t* eb);
static void edit_clear(ic_env_t* env, editor_t* eb);
static void edit_clear_screen(ic_env_t* env, editor_t* eb);
static void edit_undo_restore(ic_env_t* env, editor_t* eb);
static void edit_redo_restore(ic_env_t* env, editor_t* eb);
static void edit_show_help(ic_env_t* env, editor_t* eb);
static void edit_cursor_left(ic_env_t* env, editor_t* eb);
static void edit_cursor_right(ic_env_t* env, editor_t* eb);
static void edit_cursor_row_up(ic_env_t* env, editor_t* eb);
static void edit_cursor_row_down(ic_env_t* env, editor_t* eb);
static void edit_cursor_line_start(ic_env_t* env, editor_t* eb);
static void edit_cursor_line_end(ic_env_t* env, editor_t* eb);
static void edit_cursor_prev_word(ic_env_t* env, editor_t* eb);
static void edit_cursor_next_word(ic_env_t* env, editor_t* eb);
static void edit_cursor_to_start(ic_env_t* env, editor_t* eb);
static void edit_cursor_to_end(ic_env_t* env, editor_t* eb);
static void edit_cursor_match_brace(ic_env_t* env, editor_t* eb);
static void edit_backspace(ic_env_t* env, editor_t* eb);
static void edit_delete_char(ic_env_t* env, editor_t* eb);
static void edit_delete_to_end_of_word(ic_env_t* env, editor_t* eb);
static void edit_delete_to_start_of_ws_word(ic_env_t* env, editor_t* eb);
static void edit_delete_to_start_of_word(ic_env_t* env, editor_t* eb);
static void edit_delete_to_start_of_line(ic_env_t* env, editor_t* eb);
static void edit_delete_to_end_of_line(ic_env_t* env, editor_t* eb);
static void edit_swap_char(ic_env_t* env, editor_t* eb);
static void edit_insert_char(ic_env_t* env, editor_t* eb, char c);
static bool edit_try_expand_abbreviation(ic_env_t* env, editor_t* eb, bool boundary_char_present,
                                         bool modification_started);

static bool key_action_execute(ic_env_t* env, editor_t* eb, ic_key_action_t action) {
    switch (action) {
        case IC_KEY_ACTION_NONE:
            return true;
        case IC_KEY_ACTION_COMPLETE:
            edit_generate_completions(env, eb, false);
            return true;
        case IC_KEY_ACTION_HISTORY_SEARCH:
            edit_history_search_with_current_word(env, eb);
            return true;
        case IC_KEY_ACTION_HISTORY_PREV:
            edit_history_prev(env, eb);
            return true;
        case IC_KEY_ACTION_HISTORY_NEXT:
            edit_history_next(env, eb);
            return true;
        case IC_KEY_ACTION_CLEAR_SCREEN:
            edit_clear_screen(env, eb);
            return true;
        case IC_KEY_ACTION_UNDO:
            edit_undo_restore(env, eb);
            return true;
        case IC_KEY_ACTION_REDO:
            edit_redo_restore(env, eb);
            return true;
        case IC_KEY_ACTION_SHOW_HELP:
            edit_show_help(env, eb);
            return true;
        case IC_KEY_ACTION_CURSOR_LEFT:
            edit_cursor_left(env, eb);
            return true;
        case IC_KEY_ACTION_CURSOR_RIGHT_OR_COMPLETE:
            if (eb->pos == sbuf_len(eb->input)) {
                edit_generate_completions(env, eb, false);
            } else {
                edit_cursor_right(env, eb);
            }
            return true;
        case IC_KEY_ACTION_CURSOR_UP:
            edit_cursor_row_up(env, eb);
            return true;
        case IC_KEY_ACTION_CURSOR_DOWN:
            edit_cursor_row_down(env, eb);
            return true;
        case IC_KEY_ACTION_CURSOR_LINE_START:
            edit_cursor_line_start(env, eb);
            return true;
        case IC_KEY_ACTION_CURSOR_LINE_END:
            edit_cursor_line_end(env, eb);
            return true;
        case IC_KEY_ACTION_CURSOR_WORD_PREV:
            edit_cursor_prev_word(env, eb);
            return true;
        case IC_KEY_ACTION_CURSOR_WORD_NEXT_OR_COMPLETE:
            if (eb->pos == sbuf_len(eb->input)) {
                edit_generate_completions(env, eb, false);
            } else {
                edit_cursor_next_word(env, eb);
            }
            return true;
        case IC_KEY_ACTION_CURSOR_INPUT_START:
            edit_cursor_to_start(env, eb);
            return true;
        case IC_KEY_ACTION_CURSOR_INPUT_END:
            edit_cursor_to_end(env, eb);
            return true;
        case IC_KEY_ACTION_CURSOR_MATCH_BRACE:
            edit_cursor_match_brace(env, eb);
            return true;
        case IC_KEY_ACTION_DELETE_BACKWARD:
            edit_backspace(env, eb);
            return true;
        case IC_KEY_ACTION_DELETE_FORWARD:
            edit_delete_char(env, eb);
            return true;
        case IC_KEY_ACTION_DELETE_WORD_END:
            edit_delete_to_end_of_word(env, eb);
            return true;
        case IC_KEY_ACTION_DELETE_WORD_START_WS:
            edit_delete_to_start_of_ws_word(env, eb);
            return true;
        case IC_KEY_ACTION_DELETE_WORD_START:
            edit_delete_to_start_of_word(env, eb);
            return true;
        case IC_KEY_ACTION_DELETE_LINE_START:
            edit_delete_to_start_of_line(env, eb);
            return true;
        case IC_KEY_ACTION_DELETE_LINE_END:
            edit_delete_to_end_of_line(env, eb);
            return true;
        case IC_KEY_ACTION_TRANSPOSE_CHARS:
            edit_swap_char(env, eb);
            return true;
        case IC_KEY_ACTION_INSERT_NEWLINE:
            if (!env->singleline_only) {
                edit_insert_char(env, eb, '\n');
            }
            return true;
        default:
            break;
    }
    return false;
}

static bool key_binding_execute(ic_env_t* env, editor_t* eb, code_t key) {
    if (env == NULL || env->key_binding_count <= 0)
        return false;
    for (ssize_t i = 0; i < env->key_binding_count; ++i) {
        ic_key_binding_entry_t* entry = &env->key_bindings[i];
        if (entry->key == key) {
            if (entry->action == IC_KEY_ACTION_NONE)
                return true;
            if (entry->action == IC_KEY_ACTION_RUNOFF) {
                // Call the unhandled key handler directly
                if (env->unhandled_key_handler != NULL) {
                    return env->unhandled_key_handler(key, env->unhandled_key_arg);
                }
                return false;
            }
            return key_action_execute(env, eb, entry->action);
        }
    }
    return false;
}

//-------------------------------------------------------------
// Main edit line
//-------------------------------------------------------------
static void insert_initial_input(const char* initial_input,
                                 editor_t* eb);  // defined at bottom

static char* edit_line(ic_env_t* env, const char* prompt_text,
                       const char* inline_right_text);  // defined at bottom
static void edit_refresh(ic_env_t* env, editor_t* eb);

ic_private char* ic_editline(ic_env_t* env, const char* prompt_text,
                             const char* inline_right_text) {
    tty_start_raw(env->tty);
    term_start_raw(env->term);
    char* line = edit_line(env, prompt_text, inline_right_text);
    term_end_raw(env->term, false);
    tty_end_raw(env->tty);
    term_writeln(env->term, "");
    term_flush(env->term);
    return line;
}

//-------------------------------------------------------------
// Undo/Redo
//-------------------------------------------------------------

// capture the current edit state
static void editor_capture(editor_t* eb, editstate_t** es) {
    if (!eb->disable_undo) {
        editstate_capture(eb->mem, es, sbuf_string(eb->input), eb->pos);
    }
}

static void editor_undo_capture(editor_t* eb) {
    editor_capture(eb, &eb->undo);
}

static void editor_undo_forget(editor_t* eb) {
    if (eb->disable_undo)
        return;
    const char* input = NULL;
    ssize_t pos = 0;
    editstate_restore(eb->mem, &eb->undo, &input, &pos);
    mem_free(eb->mem, input);
}

static void editor_restore(editor_t* eb, editstate_t** from, editstate_t** to) {
    if (eb->disable_undo)
        return;
    if (*from == NULL)
        return;
    const char* input;
    if (to != NULL) {
        editor_capture(eb, to);
    }
    if (!editstate_restore(eb->mem, from, &input, &eb->pos))
        return;
    sbuf_replace(eb->input, input);
    mem_free(eb->mem, input);
    eb->modified = false;
}

static void editor_undo_restore(editor_t* eb, bool with_redo) {
    editor_restore(eb, &eb->undo, (with_redo ? &eb->redo : NULL));
}

static void editor_redo_restore(editor_t* eb) {
    editor_restore(eb, &eb->redo, &eb->undo);
    eb->modified = false;
}

static void editor_start_modify(editor_t* eb) {
    editor_undo_capture(eb);
    editstate_done(eb->mem, &eb->redo);  // clear redo
    eb->modified = true;
    // Clear history preview when user starts modifying input
    edit_clear_history_preview(eb);
}

static bool editor_pos_is_at_end(editor_t* eb) {
    return (eb->pos == sbuf_len(eb->input));
}

static bool editor_input_has_unclosed_heredoc(editor_t* eb) {
    if (eb == NULL || eb->input == NULL) {
        return false;
    }

    const char* input = sbuf_string(eb->input);
    if (input == NULL) {
        return false;
    }

    ssize_t len = sbuf_len(eb->input);
    ssize_t pos = 0;
    bool in_single_quote = false;
    bool in_double_quote = false;

    while (pos < len) {
        char c = input[pos];

        if (c == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            pos++;
            continue;
        }
        if (c == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            pos++;
            continue;
        }
        if (in_single_quote || in_double_quote) {
            pos++;
            continue;
        }

        if (c == '<' && (pos + 1) < len && input[pos + 1] == '<') {
            ssize_t lookahead = pos + 2;
            if (lookahead < len && input[lookahead] == '-') {
                lookahead++;
            }

            while (lookahead < len && isspace((unsigned char)input[lookahead]) != 0) {
                lookahead++;
            }

            bool delimiter_quoted = false;
            char quote_char = '\0';

            if (lookahead < len &&
                (input[lookahead] == '\'' || input[lookahead] == '"' || input[lookahead] == '\\')) {
                delimiter_quoted = true;
                quote_char = input[lookahead];
                lookahead++;
            }

            ssize_t delimiter_length = 0;
            while (lookahead < len) {
                char dc = input[lookahead];
                if (delimiter_quoted) {
                    if (dc == quote_char) {
                        break;
                    }
                } else {
                    if (isspace((unsigned char)dc) != 0 || dc == ';' || dc == '&' || dc == '|' ||
                        dc == '<' || dc == '>') {
                        break;
                    }
                }
                delimiter_length++;
                lookahead++;
            }

            if (delimiter_quoted && lookahead < len && input[lookahead] == quote_char) {
                lookahead++;
            }

            if (delimiter_length > 0) {
                return true;
            }
        }

        pos++;
    }

    return false;
}

//-------------------------------------------------------------
// Row/Column width and positioning
//-------------------------------------------------------------

// Recompute prompt layout widths only when markers or indent settings change.
static void ensure_prompt_width_cache(ic_env_t* env, editor_t* eb) {
    if (env == NULL || eb == NULL) {
        return;
    }
    if (!eb->prompt_width_cache_valid ||
        eb->prompt_layout_generation_snapshot != env->prompt_layout_generation) {
        const char* prompt_text = (eb->prompt_text != NULL ? eb->prompt_text : "");
        eb->prompt_text_width_cache = bbcode_column_width(env->bbcode, prompt_text);
        eb->prompt_marker_width_cache = bbcode_column_width(env->bbcode, env->prompt_marker);
        eb->cprompt_marker_width_cache = bbcode_column_width(env->bbcode, env->cprompt_marker);
        eb->prompt_total_width_cache = eb->prompt_marker_width_cache + eb->prompt_text_width_cache;
        if (env->no_multiline_indent) {
            eb->indent_width_cache = eb->cprompt_marker_width_cache;
        } else {
            eb->indent_width_cache = (eb->prompt_total_width_cache > eb->cprompt_marker_width_cache
                                          ? eb->prompt_total_width_cache
                                          : eb->cprompt_marker_width_cache);
        }
        eb->prompt_layout_generation_snapshot = env->prompt_layout_generation;
        eb->prompt_width_cache_valid = true;
    }
}

// Derive the visible width of inline right-aligned text, falling back to a stripped copy if
// bbcode parsing yields no visible glyphs.
static ssize_t compute_inline_right_width(ic_env_t* env, editor_t* eb, const char* text) {
    if (env == NULL || eb == NULL || text == NULL || text[0] == 0) {
        if (eb != NULL && eb->inline_right_plain_cache != NULL) {
            sbuf_clear(eb->inline_right_plain_cache);
        }
        return 0;
    }

    ssize_t width = bbcode_column_width(env->bbcode, text);
    if (width > 0) {
        return width;
    }

    if (eb->inline_right_plain_cache == NULL) {
        eb->inline_right_plain_cache = sbuf_new(env->mem);
        if (eb->inline_right_plain_cache == NULL) {
            return 0;
        }
    } else {
        sbuf_clear(eb->inline_right_plain_cache);
    }

    bbcode_append(env->bbcode, text, eb->inline_right_plain_cache, NULL);
    if (sbuf_len(eb->inline_right_plain_cache) > 0) {
        const char* plain = sbuf_string(eb->inline_right_plain_cache);
        ssize_t plain_width = str_column_width(plain);
        sbuf_clear(eb->inline_right_plain_cache);
        if (plain_width > 0) {
            return plain_width;
        }
    } else {
        sbuf_clear(eb->inline_right_plain_cache);
    }

    const char* src = text;
    while (*src != 0) {
        if (*src == '\\' && src[1] != 0) {
            sbuf_append_char(eb->inline_right_plain_cache, src[1]);
            src += 2;
            continue;
        }
        sbuf_append_char(eb->inline_right_plain_cache, *src);
        src++;
    }
    const char* stripped = sbuf_string(eb->inline_right_plain_cache);
    ssize_t stripped_width = str_column_width(stripped);
    sbuf_clear(eb->inline_right_plain_cache);
    return stripped_width;
}

static ssize_t estimate_line_number_column_width(const editor_t* eb) {
    ssize_t baseline = (eb->cur_rows > 0 ? eb->cur_rows : 1);
    ssize_t digits = 0;
    ssize_t value = baseline;
    while (value > 0) {
        digits++;
        value /= 10;
    }
    if (digits == 0) {
        digits = 1;
    }
    return digits + 2;  // account for "| " separator
}

static void edit_get_prompt_width(ic_env_t* env, editor_t* eb, bool in_extra, ssize_t* promptw,
                                  ssize_t* cpromptw) {
    if (in_extra) {
        *promptw = 0;
        *cpromptw = 0;
    } else {
        ensure_prompt_width_cache(env, eb);
        *promptw = eb->prompt_total_width_cache;

        ssize_t indent_target = eb->indent_width_cache;
        if (env->show_line_numbers) {
            ssize_t cached_width =
                (eb->line_number_column_width > 0 ? eb->line_number_column_width
                                                  : estimate_line_number_column_width(eb));
            *cpromptw = (cached_width > indent_target ? cached_width : indent_target);
        } else {
            *cpromptw = indent_target;
        }

        // Update cached inline right text width
        if (eb->inline_right_text != NULL && eb->inline_right_text[0] != 0) {
            if (!eb->inline_right_width_valid ||
                eb->cached_inline_right_text != eb->inline_right_text) {
                eb->inline_right_width = compute_inline_right_width(env, eb, eb->inline_right_text);
                eb->inline_right_width_valid = true;
                eb->cached_inline_right_text = eb->inline_right_text;
            }
        } else {
            eb->inline_right_width = 0;
            eb->inline_right_width_valid = false;
            eb->cached_inline_right_text = eb->inline_right_text;
            if (eb->inline_right_plain_cache != NULL) {
                sbuf_clear(eb->inline_right_plain_cache);
            }
        }
    }
}

static ssize_t edit_get_rowcol(ic_env_t* env, editor_t* eb, rowcol_t* rc) {
    ssize_t promptw, cpromptw;
    edit_get_prompt_width(env, eb, false, &promptw, &cpromptw);
    return sbuf_get_rc_at_pos(eb->input, eb->termw, promptw, cpromptw, eb->pos, rc);
}

static void edit_set_pos_at_rowcol(ic_env_t* env, editor_t* eb, ssize_t row, ssize_t col) {
    ssize_t promptw, cpromptw;
    edit_get_prompt_width(env, eb, false, &promptw, &cpromptw);
    ssize_t pos = sbuf_get_pos_at_rc(eb->input, eb->termw, promptw, cpromptw, row, col);
    if (pos < 0)
        return;
    eb->pos = pos;
    edit_refresh(env, eb);
}

static bool edit_pos_is_at_row_end(ic_env_t* env, editor_t* eb) {
    rowcol_t rc;
    edit_get_rowcol(env, eb, &rc);
    return rc.last_on_row;
}

static bool edit_complete(ic_env_t* env, editor_t* eb, ssize_t idx);

static ssize_t edit_find_word_start(const char* input, ssize_t pos) {
    ssize_t start = pos;
    while (start > 0) {
        ssize_t prev = str_prev_ofs(input, start, NULL);
        if (prev <= 0)
            break;
        if (ic_char_is_separator(input + start - prev, (long)prev))
            break;
        start -= prev;
    }
    return start;
}

static inline char ascii_tolower_char(char c) {
    if (c >= 'A' && c <= 'Z')
        return (char)(c + ('a' - 'A'));
    return c;
}

static size_t levenshtein_casefold(alloc_t* mem, const char* left, const char* right) {
    if (left == NULL || right == NULL)
        return SIZE_MAX;
    size_t len_left = strlen(left);
    size_t len_right = strlen(right);
    if (len_left == 0)
        return len_right;
    if (len_right == 0)
        return len_left;

    size_t* prev = mem_malloc_tp_n(mem, size_t, len_right + 1);
    size_t* curr = mem_malloc_tp_n(mem, size_t, len_right + 1);
    if (prev == NULL || curr == NULL) {
        mem_free(mem, prev);
        mem_free(mem, curr);
        return SIZE_MAX;
    }

    for (size_t j = 0; j <= len_right; ++j) {
        prev[j] = j;
    }

    for (size_t i = 1; i <= len_left; ++i) {
        curr[0] = i;
        char cl = ascii_tolower_char(left[i - 1]);
        for (size_t j = 1; j <= len_right; ++j) {
            char cr = ascii_tolower_char(right[j - 1]);
            size_t cost = (cl == cr ? 0 : 1);
            size_t deletion = prev[j] + 1;
            size_t insertion = curr[j - 1] + 1;
            size_t substitution = prev[j - 1] + cost;
            size_t best = deletion;
            if (insertion < best)
                best = insertion;
            if (substitution < best)
                best = substitution;
            curr[j] = best;
        }
        size_t* tmp = prev;
        prev = curr;
        curr = tmp;
    }

    size_t result = prev[len_right];
    mem_free(mem, prev);
    mem_free(mem, curr);
    return result;
}

static size_t edit_spell_threshold(size_t left_len, size_t right_len) {
    size_t max_len = (left_len > right_len ? left_len : right_len);
    if (max_len <= 2)
        return 1;
    if (max_len <= 4)
        return 1;
    if (max_len <= 6)
        return 2;
    return max_len / 2;
}

static bool edit_try_spell_correct(ic_env_t* env, editor_t* eb) {
    if (!env->spell_correct)
        return false;

    const char* input = sbuf_string(eb->input);
    if (input == NULL)
        return false;
    ssize_t pos = eb->pos;
    if (pos <= 0)
        return false;

    ssize_t prev = str_prev_ofs(input, pos, NULL);
    if (prev <= 0)
        return false;
    if (ic_char_is_separator(input + pos - prev, (long)prev))
        return false;

    ssize_t word_start = edit_find_word_start(input, pos);
    if (word_start < 0 || word_start >= pos)
        return false;

    ssize_t word_len = pos - word_start;
    char* original_word = mem_strndup(env->mem, input + word_start, word_len);
    if (original_word == NULL)
        return false;

    editor_start_modify(eb);
    sbuf_delete_from_to(eb->input, word_start, pos);
    eb->pos = word_start;

    ssize_t candidate_count = completions_generate(env, env->completions, sbuf_string(eb->input),
                                                   eb->pos, IC_MAX_COMPLETIONS_TO_TRY);
    if (candidate_count <= 0) {
        editor_undo_restore(eb, false);
        completions_clear(env->completions);
        mem_free(env->mem, original_word);
        return false;
    }

    ssize_t best_index = -1;
    size_t best_distance = SIZE_MAX;
    long best_length_diff = LONG_MAX;
    ssize_t original_len = ic_strlen(original_word);

    for (ssize_t i = 0; i < candidate_count; ++i) {
        const char* replacement = completions_get_replacement(env->completions, i);
        if (replacement == NULL || *replacement == '\0')
            continue;
        size_t distance = levenshtein_casefold(env->mem, original_word, replacement);
        if (distance == SIZE_MAX)
            continue;
        ssize_t replacement_len = ic_strlen(replacement);
        long len_diff = labs((long)replacement_len - (long)original_len);
        if (distance < best_distance ||
            (distance == best_distance && len_diff < best_length_diff)) {
            best_distance = distance;
            best_length_diff = len_diff;
            best_index = i;
        }
    }

    bool applied = false;
    if (best_index >= 0) {
        const char* best_replacement = completions_get_replacement(env->completions, best_index);
        size_t replacement_len =
            (best_replacement == NULL ? 0 : (size_t)ic_strlen(best_replacement));
        size_t threshold = edit_spell_threshold((size_t)original_len, replacement_len);
        if (best_distance <= threshold) {
            applied = edit_complete(env, eb, best_index);
        }
    }

    if (!applied) {
        editor_undo_restore(eb, false);
    }
    completions_clear(env->completions);
    mem_free(env->mem, original_word);
    return applied;
}

// Helper function to extract the last line from a multi-line prompt
static char* extract_last_prompt_line(alloc_t* mem, const char* prompt_text) {
    if (prompt_text == NULL)
        return mem_strdup(mem, "");

    // Find the last newline in the prompt
    const char* last_newline = strrchr(prompt_text, '\n');
    if (last_newline == NULL) {
        // No newlines, return the whole prompt
        return mem_strdup(mem, prompt_text);
    }

    // Return everything after the last newline
    return mem_strdup(mem, last_newline + 1);
}

// Helper function to print all but the last line of a multi-line prompt
static ssize_t print_prompt_prefix_lines(ic_env_t* env, const char* prompt_text) {
    if (prompt_text == NULL)
        return 0;

    const char* last_newline = strrchr(prompt_text, '\n');
    if (last_newline == NULL) {
        // No newlines, nothing to print
        return 0;
    }

    // Print everything up to (but not including) the last newline
    size_t prefix_length = to_size_t(last_newline - prompt_text + 1);  // +1 to include newline
    char* prefix = (char*)malloc(prefix_length + 1);
    if (prefix == NULL)
        return 0;

    strncpy(prefix, prompt_text, prefix_length);
    prefix[prefix_length] = '\0';

    // Print the prefix lines directly to the terminal
    bbcode_print(env->bbcode, prefix);

    // Count how many lines we emitted (number of newline characters)
    ssize_t lines = 0;
    for (const char* p = prompt_text; p <= last_newline; ++p) {
        if (*p == '\n')
            lines++;
    }

    free(prefix);
    return lines;
}

static void format_line_number_prompt(char* buffer, size_t buffer_size, ssize_t row,
                                      ssize_t cursor_row, bool relative) {
    if (buffer == NULL || buffer_size == 0)
        return;
    if (relative) {
        if (cursor_row < 0) {
            snprintf(buffer, buffer_size, "%zd| ", row + 1);
            return;
        }
        ssize_t diff = (row >= cursor_row ? row - cursor_row : cursor_row - row);
        if (diff == 0) {
            // current line number
            snprintf(buffer, buffer_size, "%zd| ", row + 1);
        } else {
            snprintf(buffer, buffer_size, "%zd| ", diff);
        }
    } else {
        snprintf(buffer, buffer_size, "%zd| ", row + 1);
    }
}

static void edit_write_prompt(ic_env_t* env, editor_t* eb, ssize_t row, bool in_extra,
                              ssize_t cursor_row) {
    if (in_extra)
        return;
    bbcode_style_open(env->bbcode, "ic-prompt");
    if (row == 0) {
        // regular prompt text
        bbcode_print(env->bbcode, eb->prompt_text);
    } else if (env->show_line_numbers) {
        // show line numbers for multiline input
        bbcode_style_close(env->bbcode, NULL);
        // Use different color for current line number if highlighting is enabled
        const char* style = (env->highlight_current_line_number && row == cursor_row)
                                ? "ic-linenumber-current"
                                : "ic-linenumbers";
        bbcode_style_open(env->bbcode, style);
        char line_number_str[16];
        format_line_number_prompt(line_number_str, sizeof(line_number_str), row, cursor_row,
                                  env->relative_line_numbers);
        ensure_prompt_width_cache(env, eb);
        ssize_t promptw = eb->prompt_total_width_cache;
        ssize_t indent_target = eb->indent_width_cache;
        ssize_t line_number_width = (ssize_t)strlen(line_number_str);
        ssize_t desired_width = indent_target;
        if (eb->line_number_column_width > desired_width) {
            desired_width = eb->line_number_column_width;
        }
        if (line_number_width > desired_width) {
            desired_width = line_number_width;
        }

        ssize_t leading_spaces = desired_width - line_number_width;
        if (leading_spaces > 0) {
            term_write_repeat(env->term, " ", leading_spaces);
        }

        bbcode_print(env->bbcode, line_number_str);

        if (desired_width > eb->line_number_column_width) {
            eb->line_number_column_width = desired_width;
        }

        bbcode_style_close(env->bbcode, NULL);
        bbcode_style_open(env->bbcode, "ic-prompt");
    } else if (!env->no_multiline_indent) {
        // multiline continuation indentation
        ensure_prompt_width_cache(env, eb);
        if (eb->cprompt_marker_width_cache < eb->prompt_total_width_cache) {
            term_write_repeat(env->term, " ",
                              eb->prompt_total_width_cache - eb->cprompt_marker_width_cache);
        }
    }
    // the marker (skip for line numbers since we include our own separator)
    if (row == 0 || !env->show_line_numbers) {
        bbcode_print(env->bbcode, (row == 0 ? env->prompt_marker : env->cprompt_marker));
    }
    bbcode_style_close(env->bbcode, NULL);
}

static void edit_write_row_text(ic_env_t* env, const char* text, ssize_t len, const attr_t* attrs,
                                bool in_extra) {
    if (env == NULL || text == NULL || len <= 0) {
        return;
    }

    if (!env->show_whitespace_characters || in_extra) {
        if (attrs == NULL) {
            term_write_n(env->term, text, len);
        } else {
            term_write_formatted_n(env->term, text, attrs, len);
        }
        return;
    }

    const char* marker = ic_env_get_whitespace_marker(env);
    ssize_t marker_len = ic_strlen(marker);
    if (marker_len <= 0) {
        marker = " ";
        marker_len = 1;
    }

    const attr_t whitespace_attr = bbcode_style(env->bbcode, "ic-whitespace-char");
    const bool has_whitespace_style = !attr_is_none(whitespace_attr);
    const attr_t hint_attr = bbcode_style(env->bbcode, "ic-hint");

    if (attrs == NULL) {
        attr_t default_attr = attr_none();
        bool whitespace_active = false;
        if (has_whitespace_style) {
            term_start_raw(env->term);
            default_attr = term_get_attr(env->term);
        }
        ssize_t offset = 0;
        while (offset < len) {
            ssize_t char_len = 0;
            unicode_t code =
                unicode_from_qutf8((const uint8_t*)text + offset, len - offset, &char_len);
            if (char_len <= 0 || offset + char_len > len) {
                char_len = 1;
                code = (uint8_t)text[offset];
            }

            if (code == ' ') {
                if (has_whitespace_style && !whitespace_active) {
                    term_set_attr(env->term, attr_update_with(default_attr, whitespace_attr));
                    whitespace_active = true;
                }
                term_write_n(env->term, marker, marker_len);
            } else {
                if (has_whitespace_style && whitespace_active) {
                    term_set_attr(env->term, default_attr);
                    whitespace_active = false;
                }
                term_write_n(env->term, text + offset, char_len);
            }
            offset += char_len;
        }
        if (has_whitespace_style) {
            term_set_attr(env->term, default_attr);
        }
        return;
    }

    term_start_raw(env->term);
    attr_t default_attr = term_get_attr(env->term);
    attr_t current_attr = attr_none();
    bool whitespace_active = false;
    attr_t whitespace_base_attr = attr_none();
    ssize_t offset = 0;
    while (offset < len) {
        ssize_t char_len = 0;
        unicode_t code = unicode_from_qutf8((const uint8_t*)text + offset, len - offset, &char_len);
        if (char_len <= 0 || offset + char_len > len) {
            char_len = 1;
            code = (uint8_t)text[offset];
        }

        attr_t attr = attrs[offset];
        attr_t base_attr = attr_update_with(default_attr, attr);
        if (!attr_is_eq(current_attr, attr)) {
            term_set_attr(env->term, base_attr);
            current_attr = attr;
            whitespace_active = false;
        }

        bool is_hint = attr_is_eq(attr, hint_attr);

        if (code == ' ' && !is_hint) {
            if (has_whitespace_style) {
                if (!whitespace_active || !attr_is_eq(whitespace_base_attr, base_attr)) {
                    term_set_attr(env->term, attr_update_with(base_attr, whitespace_attr));
                    whitespace_active = true;
                    whitespace_base_attr = base_attr;
                }
            }
            term_write_n(env->term, marker, marker_len);
        } else {
            if (has_whitespace_style && whitespace_active) {
                term_set_attr(env->term, base_attr);
                whitespace_active = false;
            }
            term_write_n(env->term, text + offset, char_len);
        }
        offset += char_len;
    }
    term_set_attr(env->term, default_attr);
}

//-------------------------------------------------------------
// Refresh
//-------------------------------------------------------------

typedef struct refresh_info_s {
    ic_env_t* env;
    editor_t* eb;
    attrbuf_t* attrs;
    bool in_extra;
    ssize_t first_row;
    ssize_t last_row;
    ssize_t cursor_row;
} refresh_info_t;

static bool edit_refresh_rows_iter(const char* s, ssize_t row, ssize_t row_start, ssize_t row_len,
                                   ssize_t startw, bool is_wrap, const void* arg, void* res) {
    ic_unused(res);
    ic_unused(startw);
    const refresh_info_t* info = (const refresh_info_t*)(arg);
    term_t* term = info->env->term;

    // debug_msg("edit: line refresh: row %zd, len: %zd\n", row, row_len);
    if (row < info->first_row)
        return false;
    if (row > info->last_row)
        return true;  // should not occur

    // term_clear_line(term);
    edit_write_prompt(info->env, info->eb, row, info->in_extra, info->cursor_row);

    //' write output
    const bool use_attrs =
        !(info->env->no_highlight && info->env->no_bracematch) && info->attrs != NULL;
    const attr_t* row_attrs = NULL;
    if (use_attrs) {
        const attr_t* attrs = attrbuf_attrs(info->attrs, row_start + row_len);
        if (attrs != NULL) {
            row_attrs = attrs + row_start;
        }
    }
    edit_write_row_text(info->env, s + row_start, row_len, row_attrs, info->in_extra);

    // write line ending
    if (row < info->last_row) {
        if (is_wrap && tty_is_utf8(info->env->tty)) {
#ifndef __APPLE__
            bbcode_print(info->env->bbcode,
                         "[ic-dim]\xE2\x86\x90");  // left arrow
#else
            bbcode_print(info->env->bbcode,
                         "[ic-dim]\xE2\x86\xB5");  // return symbol
#endif
        }
        term_clear_to_end_of_line(term);
        term_writeln(term, "");
    } else {
        // Handle inline right-aligned text on the last (input) row
        if (row == 0 && !info->in_extra && info->eb->inline_right_text != NULL) {
            ssize_t promptw, cpromptw;
            edit_get_prompt_width(info->env, info->eb, info->in_extra, &promptw, &cpromptw);

            ssize_t current_pos = promptw + row_len;
            ssize_t right_text_width = info->eb->inline_right_width;
            ssize_t terminal_width = info->eb->termw;

            // Only show right text if there's enough space and input hasn't
            // reached it

            if (terminal_width > current_pos + right_text_width + 1) {
                ssize_t spaces_needed = terminal_width - current_pos - right_text_width;
                // Write spaces and then right-aligned text
                term_write_repeat(term, " ", spaces_needed);
                // Write the inline right text, extracting plain text from
                // bbcode if needed
                const char* text_to_write = info->eb->inline_right_text;
                const char* time_start = NULL;

                // Look for time pattern [HH:MM:SS] in the text to extract from
                // bbcode formatting
                for (const char* p = text_to_write; *p; p++) {
                    if (*p == '[' && p[1] >= '0' && p[1] <= '9' && p[2] >= '0' && p[2] <= '9' &&
                        p[3] == ':' && p[4] >= '0' && p[4] <= '9' && p[5] >= '0' && p[5] <= '9' &&
                        p[6] == ':' && p[7] >= '0' && p[7] <= '9' && p[8] >= '0' && p[8] <= '9' &&
                        p[9] == ']') {
                        time_start = p;
                        break;
                    }
                }

                if (time_start) {
                    // Found time pattern, write just the clean time without
                    // ANSI codes
                    term_write_n(info->env->term, time_start,
                                 10);  // [HH:MM:SS] is exactly 10 chars
                } else {
                    // Fallback: use bbcode_print for other content
                    bbcode_print(info->env->bbcode, info->eb->inline_right_text);
                }
                term_flush(info->env->term);  // Ensure text is flushed to terminal
                // DON'T clear to end of line after writing the text!
            } else {
                // Clear to end of line if no space for right text
                term_clear_to_end_of_line(term);
            }
        } else {
            term_clear_to_end_of_line(term);
        }
    }
    return (row >= info->last_row);
}

static void edit_refresh_rows(ic_env_t* env, editor_t* eb, stringbuf_t* input, attrbuf_t* attrs,
                              ssize_t promptw, ssize_t cpromptw, bool in_extra, ssize_t first_row,
                              ssize_t last_row, ssize_t cursor_row) {
    if (input == NULL)
        return;
    refresh_info_t info;
    info.env = env;
    info.eb = eb;
    info.attrs = attrs;
    info.in_extra = in_extra;
    info.first_row = first_row;
    info.last_row = last_row;
    info.cursor_row = cursor_row;
    sbuf_for_each_row(input, eb->termw, promptw, cpromptw, &edit_refresh_rows_iter, &info, NULL);
}

static void edit_refresh(ic_env_t* env, editor_t* eb) {
    // calculate the new cursor row and total rows needed
    ssize_t promptw, cpromptw;
    edit_get_prompt_width(env, eb, false, &promptw, &cpromptw);

    if (eb->attrs != NULL) {
        highlight(env->mem, env->bbcode, sbuf_string(eb->input), eb->attrs,
                  (env->no_highlight ? NULL : env->highlighter), env->highlighter_arg);
    }

    // highlight matching braces
    if (eb->attrs != NULL && !env->no_bracematch) {
        highlight_match_braces(
            sbuf_string(eb->input), eb->attrs, eb->pos, ic_env_get_match_braces(env),
            bbcode_style(env->bbcode, "ic-bracematch"), bbcode_style(env->bbcode, "ic-error"));
    }

    // insert hint
    if (sbuf_len(eb->hint) > 0) {
        if (eb->attrs != NULL) {
            attrbuf_insert_at(eb->attrs, eb->pos, sbuf_len(eb->hint),
                              bbcode_style(env->bbcode, "ic-hint"));
        }
        sbuf_insert_at(eb->input, sbuf_string(eb->hint), eb->pos);
    }

    // render extra (like a completion menu)
    stringbuf_t* extra = NULL;
    if (sbuf_len(eb->extra) > 0) {
        extra = sbuf_new(eb->mem);
        if (extra != NULL) {
            if (sbuf_len(eb->hint_help) > 0) {
                bbcode_append(env->bbcode, sbuf_string(eb->hint_help), extra, eb->attrs_extra);
            }
            bbcode_append(env->bbcode, sbuf_string(eb->extra), extra, eb->attrs_extra);
        }
    }

    // calculate rows and row/col position (account for dynamic line number width)
    rowcol_t rc = {0};
    rowcol_t rc_extra = {0};
    ssize_t rows_input = 0;
    ssize_t rows_extra = 0;
    ssize_t rows = 0;
    ensure_prompt_width_cache(env, eb);
    ssize_t indent_target = eb->indent_width_cache;
    int layout_adjustments = 0;

    while (true) {
        rc = (rowcol_t){0};
        rows_input = sbuf_get_rc_at_pos(eb->input, eb->termw, promptw, cpromptw, eb->pos, &rc);

        if (extra != NULL) {
            rc_extra = (rowcol_t){0};
            rows_extra = sbuf_get_rc_at_pos(extra, eb->termw, 0, 0, 0 /*pos*/, &rc_extra);
        } else {
            rows_extra = 0;
        }

        rows = rows_input + rows_extra;

        if (env->show_line_numbers) {
            ssize_t max_line_number_width = 0;
            if (rows_input > 0) {
                char line_number_str[16];
                format_line_number_prompt(line_number_str, sizeof(line_number_str), 0, rc.row,
                                          env->relative_line_numbers);
                max_line_number_width = (ssize_t)strlen(line_number_str);

                format_line_number_prompt(line_number_str, sizeof(line_number_str), rows_input - 1,
                                          rc.row, env->relative_line_numbers);
                ssize_t last_width = (ssize_t)strlen(line_number_str);
                if (last_width > max_line_number_width) {
                    max_line_number_width = last_width;
                }
            }

            ssize_t desired_cpromptw =
                (max_line_number_width > indent_target ? max_line_number_width : indent_target);

            if (desired_cpromptw != cpromptw) {
                cpromptw = desired_cpromptw;
                if (++layout_adjustments > 4) {
                    break;
                }
                continue;
            }

            eb->line_number_column_width = desired_cpromptw;
        } else {
            eb->line_number_column_width = 0;
        }

        break;
    }

    debug_msg(
        "edit: refresh: rows %zd, cursor: %zd,%zd (previous rows %zd, cursor "
        "row "
        "%zd)\n",
        rows, rc.row, rc.col, eb->cur_rows, eb->cur_row);

    // only render at most terminal height rows
    const ssize_t termh = term_get_height(env->term);
    ssize_t first_row = 0;        // first visible row
    ssize_t last_row = rows - 1;  // last visible row
    if (rows > termh) {
        first_row = rc.row - termh + 1;  // ensure cursor is visible
        if (first_row < 0)
            first_row = 0;
        last_row = first_row + termh - 1;
    }
    assert(last_row - first_row < termh);

    // reduce flicker
    buffer_mode_t bmode = term_set_buffer_mode(env->term, BUFFERED);

    // back up to the first line
    term_start_of_line(env->term);
    term_up(env->term, (eb->cur_row >= termh ? termh - 1 : eb->cur_row));
    // term_clear_lines_to_end(env->term);  // gives flicker in old Windows cmd
    // prompt

    // render rows
    edit_refresh_rows(env, eb, eb->input, eb->attrs, promptw, cpromptw, false, first_row, last_row,
                      rc.row);
    if (rows_extra > 0) {
        assert(extra != NULL);
        const ssize_t first_rowx = (first_row > rows_input ? first_row - rows_input : 0);
        const ssize_t last_rowx = last_row - rows_input;
        assert(last_rowx >= 0);
        edit_refresh_rows(env, eb, extra, eb->attrs_extra, 0, 0, true, first_rowx, last_rowx,
                          rc.row);
    }

    // overwrite trailing rows we do not use anymore
    ssize_t rrows = last_row - first_row + 1;  // rendered rows
    if (rrows < termh && rows < eb->cur_rows) {
        ssize_t clear = eb->cur_rows - rows;
        while (rrows < termh && clear > 0) {
            clear--;
            rrows++;
            term_writeln(env->term, "");
            term_clear_line(env->term);
        }
    }

    // move cursor back to edit position
    term_start_of_line(env->term);
    term_up(env->term, first_row + rrows - 1 - rc.row);

    // Calculate the actual prompt width for the current row
    ssize_t actual_prompt_width;
    if (rc.row == 0) {
        actual_prompt_width = promptw;
    } else if (env->show_line_numbers) {
        // Calculate the actual width of the line number for this specific row
        char line_number_str[16];
        format_line_number_prompt(line_number_str, sizeof(line_number_str), rc.row, rc.row,
                                  env->relative_line_numbers);
        ssize_t line_number_width = (ssize_t)strlen(line_number_str);
        actual_prompt_width = indent_target;
        if (line_number_width > actual_prompt_width) {
            actual_prompt_width = line_number_width;
        }
        if (eb->line_number_column_width > actual_prompt_width) {
            actual_prompt_width = eb->line_number_column_width;
        }
    } else {
        actual_prompt_width = cpromptw;
    }

    term_right(env->term, rc.col + actual_prompt_width);

    // and refresh
    term_flush(env->term);

    // stop buffering
    term_set_buffer_mode(env->term, bmode);

    // restore input by removing the hint
    sbuf_delete_at(eb->input, eb->pos, sbuf_len(eb->hint));
    sbuf_delete_at(eb->extra, 0, sbuf_len(eb->hint_help));
    attrbuf_clear(eb->attrs);
    attrbuf_clear(eb->attrs_extra);
    sbuf_free(extra);

    // update previous
    eb->cur_rows = rows;
    eb->cur_row = rc.row;
}

// clear current output
static void edit_clear(ic_env_t* env, editor_t* eb) {
    term_attr_reset(env->term);
    term_up(env->term, eb->cur_row);

    // overwrite all rows
    for (ssize_t i = 0; i < eb->cur_rows; i++) {
        term_clear_line(env->term);
        term_writeln(env->term, "");
    }

    // move cursor back
    term_up(env->term, eb->cur_rows - eb->cur_row);
}

// clear screen and refresh
static void edit_clear_screen(ic_env_t* env, editor_t* eb) {
    ssize_t cur_rows = eb->cur_rows;
    eb->cur_rows = term_get_height(env->term) - 1;
    edit_clear(env, eb);
    eb->cur_rows = cur_rows;
    edit_refresh(env, eb);
}

static void edit_cleanup_erase_prompt(ic_env_t* env, editor_t* eb) {
    if (env == NULL || eb == NULL)
        return;
    ssize_t extra = to_ssize_t(env->prompt_cleanup_extra_lines);
    if (eb->cur_rows <= 0 && eb->prompt_prefix_lines <= 0 && extra <= 0)
        return;

    term_attr_reset(env->term);
    term_start_of_line(env->term);

    ssize_t rows = (eb->cur_rows < 0 ? 0 : eb->cur_rows);
    ssize_t prefixes = (eb->prompt_prefix_lines < 0 ? 0 : eb->prompt_prefix_lines);
    ssize_t total = rows + prefixes + (extra > 0 ? extra : 0);
    if (total <= 0)
        return;

    ssize_t up = (eb->cur_row < 0 ? 0 : eb->cur_row) + prefixes;
    if (extra > 0) {
        up += extra;
    }
    if (up > 0) {
        term_up(env->term, up);
        term_start_of_line(env->term);
    }

    term_delete_lines(env->term, total);
    term_start_of_line(env->term);
}

static void edit_cleanup_print(ic_env_t* env, editor_t* eb, const char* final_input) {
    if (env == NULL || eb == NULL)
        return;

    const bool add_empty_line = env->prompt_cleanup_add_empty_line;
    const char* prompt_line = (eb->prompt_text != NULL ? eb->prompt_text : "");
    const char* prompt_marker = (env->prompt_marker != NULL ? env->prompt_marker : "");
    ssize_t promptw = bbcode_column_width(env->bbcode, prompt_line) +
                      bbcode_column_width(env->bbcode, prompt_marker);
    if (promptw < 0)
        promptw = 0;

    bbcode_style_open(env->bbcode, "ic-prompt");
    bbcode_print(env->bbcode, prompt_line);
    bbcode_print(env->bbcode, prompt_marker);
    bbcode_style_close(env->bbcode, NULL);

    attrbuf_t* cleanup_attrs = NULL;
    const attr_t* cleanup_attr_data = NULL;
    ssize_t final_len = 0;

    if (final_input != NULL && final_input[0] != '\0') {
        final_len = ic_strlen(final_input);
        if (final_len > 0) {
            cleanup_attrs = attrbuf_new(env->mem);
            if (cleanup_attrs != NULL) {
                highlight(env->mem, env->bbcode, final_input, cleanup_attrs,
                          (env->no_highlight ? NULL : env->highlighter), env->highlighter_arg);
                if (!env->no_bracematch) {
                    highlight_match_braces(final_input, cleanup_attrs, final_len,
                                           ic_env_get_match_braces(env),
                                           bbcode_style(env->bbcode, "ic-bracematch"),
                                           bbcode_style(env->bbcode, "ic-error"));
                }
                if (attrbuf_len(cleanup_attrs) >= final_len) {
                    cleanup_attr_data = attrbuf_attrs(cleanup_attrs, final_len);
                }
            }

            bool should_truncate = false;
            ssize_t first_line_len = 0;
            if (env->prompt_cleanup_truncate_multiline) {
                const char* first_newline = memchr(final_input, '\n', final_len);
                if (first_newline != NULL) {
                    should_truncate = true;
                    first_line_len = (ssize_t)(first_newline - final_input);
                    if (first_line_len < 0) {
                        first_line_len = 0;
                    }
                }
            }

            if (should_truncate) {
                if (first_line_len > 0) {
                    const attr_t* first_line_attrs =
                        (cleanup_attr_data != NULL ? cleanup_attr_data : NULL);
                    term_write_formatted_n(env->term, final_input, first_line_attrs,
                                           first_line_len);
                }
                term_write(env->term, "...");
            } else {
                ssize_t offset = 0;
                ssize_t line_number = 1;  // continuation lines start counting at 1
                while (offset < final_len) {
                    const char* segment_start = final_input + offset;
                    const char* newline = memchr(segment_start, '\n', final_len - offset);
                    ssize_t segment_len =
                        (newline == NULL ? (final_len - offset)
                                         : to_ssize_t(newline - segment_start + 1));
                    const attr_t* segment_attrs =
                        (cleanup_attr_data != NULL ? cleanup_attr_data + offset : NULL);

                    term_write_formatted_n(env->term, segment_start, segment_attrs, segment_len);
                    offset += segment_len;

                    if (newline != NULL && offset < final_len) {
                        // Print line number prefix for continuation lines if line numbers are
                        // enabled
                        if (env->show_line_numbers) {
                            bbcode_style_open(env->bbcode, "ic-linenumbers");
                            char line_number_str[16];
                            format_line_number_prompt(line_number_str, sizeof(line_number_str),
                                                      line_number, -1, env->relative_line_numbers);

                            ssize_t line_number_width = (ssize_t)strlen(line_number_str);
                            ensure_prompt_width_cache(env, eb);
                            ssize_t indent_target = eb->indent_width_cache;
                            ssize_t desired_width = indent_target;
                            if (eb->line_number_column_width > desired_width) {
                                desired_width = eb->line_number_column_width;
                            }
                            if (line_number_width > desired_width) {
                                desired_width = line_number_width;
                            }

                            ssize_t leading_spaces = desired_width - line_number_width;
                            if (leading_spaces > 0) {
                                term_write_repeat(env->term, " ", leading_spaces);
                            }

                            bbcode_print(env->bbcode, line_number_str);
                            bbcode_style_close(env->bbcode, NULL);
                        } else if (promptw > 0) {
                            term_write_repeat(env->term, " ", promptw);
                        }
                        line_number++;
                    }
                }
            }
        }
    }

    attrbuf_free(cleanup_attrs);

    if (add_empty_line) {
        term_write_char(env->term, '\n');
    }
    term_flush(env->term);
}

static void edit_apply_prompt_cleanup(ic_env_t* env, editor_t* eb, const char* final_input) {
    if (env == NULL || eb == NULL)
        return;
    edit_cleanup_erase_prompt(env, eb);
    edit_cleanup_print(env, eb, final_input);
}

// refresh after a terminal window resized (but before doing further edit
// operations!)
static bool edit_resize(ic_env_t* env, editor_t* eb) {
    // update dimensions
    term_update_dim(env->term);
    ssize_t newtermw = term_get_width(env->term);
    if (eb->termw == newtermw)
        return false;

    // recalculate the row layout assuming the hardwrapping for the new terminal
    // width
    ssize_t promptw, cpromptw;
    edit_get_prompt_width(env, eb, false, &promptw, &cpromptw);
    sbuf_insert_at(eb->input, sbuf_string(eb->hint),
                   eb->pos);  // insert used hint

    // render extra (like a completion menu)
    stringbuf_t* extra = NULL;
    if (sbuf_len(eb->extra) > 0) {
        extra = sbuf_new(eb->mem);
        if (extra != NULL) {
            if (sbuf_len(eb->hint_help) > 0) {
                bbcode_append(env->bbcode, sbuf_string(eb->hint_help), extra, NULL);
            }
            bbcode_append(env->bbcode, sbuf_string(eb->extra), extra, NULL);
        }
    }
    rowcol_t rc = {0};
    const ssize_t rows_input =
        sbuf_get_wrapped_rc_at_pos(eb->input, eb->termw, newtermw, promptw, cpromptw, eb->pos, &rc);
    rowcol_t rc_extra = {0};
    ssize_t rows_extra = 0;
    if (extra != NULL) {
        rows_extra =
            sbuf_get_wrapped_rc_at_pos(extra, eb->termw, newtermw, 0, 0, 0 /*pos*/, &rc_extra);
    }
    ssize_t rows = rows_input + rows_extra;
    debug_msg(
        "edit: resize: new rows: %zd, cursor row: %zd (previous: rows: %zd, "
        "cursor row %zd)\n",
        rows, rc.row, eb->cur_rows, eb->cur_row);

    // update the newly calculated row and rows
    eb->cur_row = rc.row;
    if (rows > eb->cur_rows) {
        eb->cur_rows = rows;
    }
    eb->termw = newtermw;
    edit_refresh(env, eb);

    // remove hint again
    sbuf_delete_at(eb->input, eb->pos, sbuf_len(eb->hint));
    sbuf_free(extra);
    return true;
}

static void editor_append_hint_help(editor_t* eb, const char* help) {
    sbuf_clear(eb->hint_help);
    if (help != NULL) {
        sbuf_replace(eb->hint_help, "[ic-info]");
        sbuf_append(eb->hint_help, help);
        sbuf_append(eb->hint_help, "[/ic-info]\n");
    }
}

// refresh with possible hint
static void edit_refresh_hint(ic_env_t* env, editor_t* eb) {
    if (env->no_hint || env->hint_delay > 0) {
        // refresh without hint first
        edit_refresh(env, eb);
        if (env->no_hint)
            return;
    }

    // and see if we can construct a hint (displayed after a delay)
    ssize_t count = completions_generate(env, env->completions, sbuf_string(eb->input), eb->pos, 2);
    if (count >= 1) {
        const char* help = NULL;
        const char* hint = completions_get_hint(env->completions, 0, &help);
        if (hint != NULL) {
            sbuf_replace(eb->hint, hint);
            editor_append_hint_help(eb, help);
            // do auto-tabbing?
            if (env->complete_autotab) {
                stringbuf_t* sb = sbuf_new(env->mem);  // temporary buffer for completion
                if (sb != NULL) {
                    sbuf_replace(sb, sbuf_string(eb->input));
                    ssize_t pos = eb->pos;
                    const char* extra_hint = hint;
                    do {
                        ssize_t newpos = sbuf_insert_at(sb, extra_hint, pos);
                        if (newpos <= pos)
                            break;
                        pos = newpos;
                        count =
                            completions_generate(env, env->completions, sbuf_string(sb), pos, 2);
                        if (count == 1) {
                            const char* extra_help = NULL;
                            extra_hint = completions_get_hint(env->completions, 0, &extra_help);
                            if (extra_hint != NULL) {
                                editor_append_hint_help(eb, extra_help);
                                sbuf_append(eb->hint, extra_hint);
                            }
                        }
                    } while (count == 1);
                    sbuf_free(sb);
                }
            }
        }
    }

    if (env->hint_delay <= 0) {
        // refresh with hint directly
        edit_refresh(env, eb);
    }
}

//-------------------------------------------------------------
// Edit operations
//-------------------------------------------------------------

static void edit_history_prev(ic_env_t* env, editor_t* eb);
static void edit_history_next(ic_env_t* env, editor_t* eb);

static void edit_undo_restore(ic_env_t* env, editor_t* eb) {
    editor_undo_restore(eb, true);
    edit_refresh(env, eb);
}

static void edit_redo_restore(ic_env_t* env, editor_t* eb) {
    editor_redo_restore(eb);
    edit_refresh(env, eb);
}

static void edit_cursor_left(ic_env_t* env, editor_t* eb) {
    ssize_t cwidth = 1;
    ssize_t prev = sbuf_prev(eb->input, eb->pos, &cwidth);
    if (prev < 0)
        return;
    rowcol_t rc;
    edit_get_rowcol(env, eb, &rc);
    eb->pos = prev;
    edit_refresh(env, eb);
}

static void edit_cursor_right(ic_env_t* env, editor_t* eb) {
    ssize_t cwidth = 1;
    ssize_t next = sbuf_next(eb->input, eb->pos, &cwidth);
    if (next < 0)
        return;
    rowcol_t rc;
    edit_get_rowcol(env, eb, &rc);
    eb->pos = next;
    edit_refresh(env, eb);
}

static void edit_cursor_line_end(ic_env_t* env, editor_t* eb) {
    ssize_t end = sbuf_find_line_end(eb->input, eb->pos);
    if (end < 0)
        return;
    eb->pos = end;
    edit_refresh(env, eb);
}

static void edit_cursor_line_start(ic_env_t* env, editor_t* eb) {
    ssize_t start = sbuf_find_line_start(eb->input, eb->pos);
    if (start < 0)
        return;
    eb->pos = start;
    edit_refresh(env, eb);
}

static void edit_cursor_next_word(ic_env_t* env, editor_t* eb) {
    ssize_t end = sbuf_find_word_end(eb->input, eb->pos);
    if (end < 0)
        return;
    eb->pos = end;
    edit_refresh(env, eb);
}

static void edit_cursor_prev_word(ic_env_t* env, editor_t* eb) {
    ssize_t start = sbuf_find_word_start(eb->input, eb->pos);
    if (start < 0)
        return;
    eb->pos = start;
    edit_refresh(env, eb);
}

static void edit_cursor_next_ws_word(ic_env_t* env, editor_t* eb) {
    ssize_t end = sbuf_find_ws_word_end(eb->input, eb->pos);
    if (end < 0)
        return;
    eb->pos = end;
    edit_refresh(env, eb);
}

static void edit_cursor_prev_ws_word(ic_env_t* env, editor_t* eb) {
    ssize_t start = sbuf_find_ws_word_start(eb->input, eb->pos);
    if (start < 0)
        return;
    eb->pos = start;
    edit_refresh(env, eb);
}

static void edit_cursor_to_start(ic_env_t* env, editor_t* eb) {
    eb->pos = 0;
    edit_refresh(env, eb);
}

static void edit_cursor_to_end(ic_env_t* env, editor_t* eb) {
    eb->pos = sbuf_len(eb->input);
    edit_refresh(env, eb);
}

static void edit_cursor_row_up(ic_env_t* env, editor_t* eb) {
    rowcol_t rc;
    edit_get_rowcol(env, eb, &rc);
    if (rc.row == 0) {
        edit_history_prev(env, eb);
    } else {
        edit_set_pos_at_rowcol(env, eb, rc.row - 1, rc.col);
    }
}

static void edit_cursor_row_down(ic_env_t* env, editor_t* eb) {
    rowcol_t rc;
    ssize_t rows = edit_get_rowcol(env, eb, &rc);
    if (rc.row + 1 >= rows) {
        edit_history_next(env, eb);
    } else {
        edit_set_pos_at_rowcol(env, eb, rc.row + 1, rc.col);
    }
}

static void edit_cursor_match_brace(ic_env_t* env, editor_t* eb) {
    ssize_t match =
        find_matching_brace(sbuf_string(eb->input), eb->pos, ic_env_get_match_braces(env), NULL);
    if (match < 0)
        return;
    eb->pos = match;
    edit_refresh(env, eb);
}

static void edit_backspace(ic_env_t* env, editor_t* eb) {
    if (eb->pos <= 0)
        return;
    editor_start_modify(eb);
    eb->pos = sbuf_delete_char_before(eb->input, eb->pos);
    edit_refresh(env, eb);
}

static void edit_delete_char(ic_env_t* env, editor_t* eb) {
    if (eb->pos >= sbuf_len(eb->input))
        return;
    editor_start_modify(eb);
    sbuf_delete_char_at(eb->input, eb->pos);
    edit_refresh(env, eb);
}

static void edit_delete_all(ic_env_t* env, editor_t* eb) {
    if (sbuf_len(eb->input) <= 0)
        return;
    editor_start_modify(eb);
    sbuf_clear(eb->input);
    eb->pos = 0;
    edit_refresh(env, eb);
}

static void edit_delete_to_end_of_line(ic_env_t* env, editor_t* eb) {
    ssize_t start = sbuf_find_line_start(eb->input, eb->pos);
    if (start < 0)
        return;
    ssize_t end = sbuf_find_line_end(eb->input, eb->pos);
    if (end < 0)
        return;
    editor_start_modify(eb);
    // if on an empty line, remove it completely
    if (start == end && sbuf_char_at(eb->input, end) == '\n') {
        end++;
    } else if (start == end && sbuf_char_at(eb->input, start - 1) == '\n') {
        eb->pos--;
    }
    sbuf_delete_from_to(eb->input, eb->pos, end);
    edit_refresh(env, eb);
}

static void edit_delete_to_start_of_line(ic_env_t* env, editor_t* eb) {
    ssize_t start = sbuf_find_line_start(eb->input, eb->pos);
    if (start < 0)
        return;
    ssize_t end = sbuf_find_line_end(eb->input, eb->pos);
    if (end < 0)
        return;
    editor_start_modify(eb);
    // delete start newline if it was an empty line
    bool goright = false;
    if (start > 0 && sbuf_char_at(eb->input, start - 1) == '\n' && start == end) {
        // if it is an empty line remove it
        start--;
        // afterwards, move to start of next line if it exists (so the cursor
        // stays on the same row)
        goright = true;
    }
    sbuf_delete_from_to(eb->input, start, eb->pos);
    eb->pos = start;
    if (goright)
        edit_cursor_right(env, eb);
    edit_refresh(env, eb);
}

static void edit_delete_line(ic_env_t* env, editor_t* eb) {
    ssize_t start = sbuf_find_line_start(eb->input, eb->pos);
    if (start < 0)
        return;
    ssize_t end = sbuf_find_line_end(eb->input, eb->pos);
    if (end < 0)
        return;
    editor_start_modify(eb);
    // delete newline as well so no empty line is left;
    bool goright = false;
    if (start > 0 && sbuf_char_at(eb->input, start - 1) == '\n') {
        start--;
        // afterwards, move to start of next line if it exists (so the cursor
        // stays on the same row)
        goright = true;
    } else if (sbuf_char_at(eb->input, end) == '\n') {
        end++;
    }
    sbuf_delete_from_to(eb->input, start, end);
    eb->pos = start;
    if (goright)
        edit_cursor_right(env, eb);
    edit_refresh(env, eb);
}

static void edit_delete_to_start_of_word(ic_env_t* env, editor_t* eb) {
    ssize_t start = sbuf_find_word_start(eb->input, eb->pos);
    if (start < 0)
        return;
    editor_start_modify(eb);
    sbuf_delete_from_to(eb->input, start, eb->pos);
    eb->pos = start;
    edit_refresh(env, eb);
}

static void edit_delete_to_end_of_word(ic_env_t* env, editor_t* eb) {
    ssize_t end = sbuf_find_word_end(eb->input, eb->pos);
    if (end < 0)
        return;
    editor_start_modify(eb);
    sbuf_delete_from_to(eb->input, eb->pos, end);
    edit_refresh(env, eb);
}

static void edit_delete_to_start_of_ws_word(ic_env_t* env, editor_t* eb) {
    ssize_t start = sbuf_find_ws_word_start(eb->input, eb->pos);
    if (start < 0)
        return;
    editor_start_modify(eb);
    sbuf_delete_from_to(eb->input, start, eb->pos);
    eb->pos = start;
    edit_refresh(env, eb);
}

static void edit_delete_to_end_of_ws_word(ic_env_t* env, editor_t* eb) {
    ssize_t end = sbuf_find_ws_word_end(eb->input, eb->pos);
    if (end < 0)
        return;
    editor_start_modify(eb);
    sbuf_delete_from_to(eb->input, eb->pos, end);
    edit_refresh(env, eb);
}

static void edit_delete_word(ic_env_t* env, editor_t* eb) {
    ssize_t start = sbuf_find_word_start(eb->input, eb->pos);
    if (start < 0)
        return;
    ssize_t end = sbuf_find_word_end(eb->input, eb->pos);
    if (end < 0)
        return;
    editor_start_modify(eb);
    sbuf_delete_from_to(eb->input, start, end);
    eb->pos = start;
    edit_refresh(env, eb);
}

static void edit_swap_char(ic_env_t* env, editor_t* eb) {
    if (eb->pos <= 0 || eb->pos == sbuf_len(eb->input))
        return;
    editor_start_modify(eb);
    eb->pos = sbuf_swap_char(eb->input, eb->pos);
    edit_refresh(env, eb);
}

static void edit_multiline_eol(ic_env_t* env, editor_t* eb) {
    if (eb->pos <= 0)
        return;
    if (sbuf_string(eb->input)[eb->pos - 1] != env->multiline_eol)
        return;
    editor_start_modify(eb);
    // replace line continuation with a real newline
    sbuf_delete_at(eb->input, eb->pos - 1, 1);
    sbuf_insert_at(eb->input, "\n", eb->pos - 1);
    edit_refresh(env, eb);
}

static void edit_insert_unicode(ic_env_t* env, editor_t* eb, unicode_t u) {
    editor_start_modify(eb);
    ssize_t nextpos = sbuf_insert_unicode_at(eb->input, u, eb->pos);
    if (nextpos >= 0)
        eb->pos = nextpos;
    edit_refresh_hint(env, eb);
}

static void edit_auto_brace(ic_env_t* env, editor_t* eb, char c) {
    if (env->no_autobrace)
        return;
    const char* braces = ic_env_get_auto_braces(env);
    for (const char* b = braces; *b != 0; b += 2) {
        if (*b == c) {
            const char close = b[1];
            // if (sbuf_char_at(eb->input, eb->pos) != close) {
            sbuf_insert_char_at(eb->input, close, eb->pos);
            bool balanced = false;
            find_matching_brace(sbuf_string(eb->input), eb->pos, braces, &balanced);
            if (!balanced) {
                // don't insert if it leads to an unbalanced expression.
                sbuf_delete_char_at(eb->input, eb->pos);
            }
            //}
            return;
        } else if (b[1] == c) {
            // close brace, check if there we don't overwrite to the right
            if (sbuf_char_at(eb->input, eb->pos) == c) {
                sbuf_delete_char_at(eb->input, eb->pos);
            }
            return;
        }
    }
}

static void editor_auto_indent(editor_t* eb, const char* pre, const char* post) {
    assert(eb->pos > 0 && sbuf_char_at(eb->input, eb->pos - 1) == '\n');
    ssize_t prelen = ic_strlen(pre);
    if (prelen > 0) {
        if (eb->pos - 1 < prelen)
            return;
        if (!ic_starts_with(sbuf_string(eb->input) + eb->pos - 1 - prelen, pre))
            return;
        if (!ic_starts_with(sbuf_string(eb->input) + eb->pos, post))
            return;
        eb->pos = sbuf_insert_at(eb->input, "  ", eb->pos);
        sbuf_insert_char_at(eb->input, '\n', eb->pos);
    }
}

static bool edit_try_expand_abbreviation(ic_env_t* env, editor_t* eb, bool boundary_char_present,
                                         bool modification_started) {
    if (env == NULL || eb == NULL)
        return false;
    if (env->abbreviation_count <= 0 || env->abbreviations == NULL)
        return false;

    ssize_t boundary_offset = (boundary_char_present ? 1 : 0);
    if (eb->pos <= boundary_offset)
        return false;

    const char* buffer = sbuf_string(eb->input);
    if (buffer == NULL)
        return false;

    if (boundary_char_present) {
        ssize_t boundary_index = eb->pos - 1;
        if (boundary_index < 0)
            return false;
        if (!ic_char_is_white(buffer + boundary_index, 1))
            return false;
    }

    ssize_t word_end = eb->pos - boundary_offset;
    if (word_end <= 0)
        return false;

    if (word_end > 0 && ic_char_is_white(buffer + word_end - 1, 1))
        return false;

    ssize_t word_start = sbuf_find_ws_word_start(eb->input, word_end);
    if (word_start < 0)
        word_start = 0;

    if (word_start > 0 && !ic_char_is_white(buffer + word_start - 1, 1))
        return false;

    ssize_t word_len = word_end - word_start;
    if (word_len <= 0)
        return false;

    for (ssize_t i = 0; i < env->abbreviation_count; ++i) {
        ic_abbreviation_entry_t* entry = &env->abbreviations[i];
        if (entry->trigger_len == word_len &&
            strncmp(entry->trigger, buffer + word_start, (size_t)word_len) == 0) {
            if (!modification_started) {
                editor_start_modify(eb);
            }
            sbuf_delete_at(eb->input, word_start, word_len);
            eb->pos -= word_len;
            ssize_t new_pos = sbuf_insert_at(eb->input, entry->expansion, word_start);
            ssize_t expansion_len = new_pos - word_start;
            eb->pos += expansion_len;
            return true;
        }
    }

    return false;
}

static void edit_insert_char(ic_env_t* env, editor_t* eb, char c) {
    editor_start_modify(eb);
    ssize_t nextpos = sbuf_insert_char_at(eb->input, c, eb->pos);
    if (nextpos >= 0)
        eb->pos = nextpos;
    if (c == ' ' || c == '\n' || c == '\r') {
        edit_try_expand_abbreviation(env, eb, true, true);
    }
    edit_auto_brace(env, eb, c);
    if (c == '\n') {
        editor_auto_indent(eb, "{", "}");  // todo: custom auto indent tokens?
    }
    edit_refresh_hint(env, eb);
}

//-------------------------------------------------------------
// Help
//-------------------------------------------------------------

#include "editline_help.c"

//-------------------------------------------------------------
// History
//-------------------------------------------------------------

#include "editline_history.c"

//-------------------------------------------------------------
// Completion
//-------------------------------------------------------------

#include "editline_completion.c"

//-------------------------------------------------------------
// Edit line: main edit loop
//-------------------------------------------------------------

static void insert_initial_input(const char* initial_input, editor_t* eb) {
    if (initial_input != NULL) {
        sbuf_replace(eb->input, initial_input);
        eb->pos = sbuf_len(eb->input);
    }
}

static char* edit_line(ic_env_t* env, const char* prompt_text, const char* inline_right_text) {
    // set up an edit buffer
    editor_t eb;
    memset(&eb, 0, sizeof(eb));
    eb.mem = env->mem;
    eb.input = sbuf_new(env->mem);
    eb.extra = sbuf_new(env->mem);
    eb.hint = sbuf_new(env->mem);
    eb.hint_help = sbuf_new(env->mem);
    eb.history_prefix = sbuf_new(env->mem);
    eb.termw = term_get_width(env->term);
    eb.pos = 0;
    eb.cur_rows = 1;
    eb.cur_row = 0;
    eb.modified = false;

    // Handle multi-line prompts: print prefix lines and use only the last line
    // as the prompt
    const char* original_prompt = (prompt_text != NULL ? prompt_text : "");
    eb.prompt_prefix_lines = print_prompt_prefix_lines(env, original_prompt);
    char* last_line_prompt = extract_last_prompt_line(env->mem, original_prompt);
    eb.prompt_text = last_line_prompt;

    eb.inline_right_text = inline_right_text;
    eb.cached_inline_right_text = NULL;
    eb.inline_right_width = 0;
    eb.inline_right_width_valid = false;
    eb.line_number_column_width = 0;
    eb.prompt_width_cache_valid = false;
    eb.prompt_marker_width_cache = 0;
    eb.prompt_text_width_cache = 0;
    eb.prompt_total_width_cache = 0;
    eb.cprompt_marker_width_cache = 0;
    eb.indent_width_cache = 0;
    eb.prompt_layout_generation_snapshot = 0;
    eb.inline_right_plain_cache = NULL;

    eb.history_idx = 0;
    editstate_init(&eb.undo);
    editstate_init(&eb.redo);

    // Set this editor as the current active editor
    env->current_editor = &eb;

    // Insert initial input if present
    if (env->initial_input != NULL) {
        insert_initial_input(env->initial_input, &eb);
    }

    if (eb.input == NULL || eb.extra == NULL || eb.hint == NULL || eb.hint_help == NULL ||
        eb.history_prefix == NULL) {
        sbuf_free(eb.input);
        sbuf_free(eb.extra);
        sbuf_free(eb.hint);
        sbuf_free(eb.hint_help);
        sbuf_free(eb.history_prefix);
        mem_free(env->mem, (void*)eb.prompt_text);
        return NULL;
    }

    // caching
    if (!(env->no_highlight && env->no_bracematch)) {
        eb.attrs = attrbuf_new(env->mem);
        eb.attrs_extra = attrbuf_new(env->mem);
    }

    // show prompt
    edit_write_prompt(env, &eb, 0, false, 0);

    // Force refresh if initial input was provided to display it immediately
    if (env->initial_input != NULL) {
        edit_refresh(env, &eb);
    } else if (inline_right_text != NULL) {
        edit_refresh(env, &eb);
    }

    // always a history entry for the current input
    history_push(env->history, "");

    // process keys
    code_t c;  // current key code
    bool ctrl_c_pressed = false;
    bool ctrl_d_pressed = false;

    while (true) {
        // read a character
        term_flush(env->term);
        if (env->hint_delay <= 0 || sbuf_len(eb.hint) == 0) {
            // blocking read
            c = tty_read(env->tty);
        } else {
            // timeout to display hint
            if (!tty_read_timeout(env->tty, env->hint_delay, &c)) {
                // timed-out
                if (sbuf_len(eb.hint) > 0) {
                    // display hint
                    edit_refresh(env, &eb);
                }
                c = tty_read(env->tty);
            } else {
                // clear the pending hint if we got input before the delay
                // expired
                sbuf_clear(eb.hint);
                sbuf_clear(eb.hint_help);
            }
        }

        // update terminal in case of a resize
        if (tty_term_resize_event(env->tty)) {
            edit_resize(env, &eb);
        }

        // clear hint only after a potential resize (so resize row calculations
        // are correct)
        const bool had_hint = (sbuf_len(eb.hint) > 0);
        sbuf_clear(eb.hint);
        sbuf_clear(eb.hint_help);

        bool request_submit = false;

        if (c == KEY_CTRL_O) {
            c = KEY_ENTER;
        }

        // if the user tries to move into a hint with left-cursor or end, we
        // complete it first
        if ((c == KEY_RIGHT || c == KEY_END) && had_hint) {
            edit_generate_completions(env, &eb, true);
            c = KEY_NONE;
        }

        if ((c < IC_KEY_EVENT_BASE || c >= IC_KEY_UNICODE_MAX) &&
            key_binding_execute(env, &eb, c)) {
            continue;
        }

        // Operations that may return
        if (c == KEY_ENTER) {
            // Clear history preview when submitting
            edit_clear_history_preview(&eb);
            if (!env->singleline_only && eb.pos > 0 &&
                sbuf_string(eb.input)[eb.pos - 1] == env->multiline_eol &&
                edit_pos_is_at_row_end(env, &eb)) {
                if (editor_input_has_unclosed_heredoc(&eb)) {
                    editor_start_modify(&eb);
                    sbuf_delete_at(eb.input, eb.pos - 1, 1);
                    eb.pos--;
                    edit_refresh(env, &eb);
                    request_submit = true;
                } else {
                    // replace line-continuation with newline
                    edit_multiline_eol(env, &eb);
                }
            } else {
                // otherwise done
                if (edit_try_expand_abbreviation(env, &eb, false, false)) {
                    edit_refresh(env, &eb);
                }
                request_submit = true;
            }
        } else if (c == KEY_CTRL_D) {
            if (eb.pos == 0 && editor_pos_is_at_end(&eb)) {
                ctrl_d_pressed = true;
                break;  // ctrl+D on empty quits with CTRL+D token
            }
            edit_delete_char(env, &eb);  // otherwise it is like delete
        } else if (c == KEY_CTRL_C || c == KEY_EVENT_STOP) {
            // Clear history preview when cancelling
            edit_clear_history_preview(&eb);
            ctrl_c_pressed = true;
            break;  // ctrl+C or STOP event quits with CTRL+C token
        } else if (c == KEY_ESC) {
            // Clear history preview on ESC
            edit_clear_history_preview(&eb);
            if (eb.pos == 0 && editor_pos_is_at_end(&eb))
                break;                  // ESC on empty input returns with empty input
            edit_delete_all(env, &eb);  // otherwise delete the current input
            // edit_delete_line(env,&eb);  // otherwise delete the current line
        } else if (c == KEY_BELL /* ^G */) {
            edit_delete_all(env, &eb);
            break;  // ctrl+G cancels (and returns empty input)
        }

        // Editing Operations
        else
            switch (c) {
                // events
                case KEY_EVENT_RESIZE:  // not used
                    edit_resize(env, &eb);
                    break;
                case KEY_EVENT_AUTOTAB:
                    edit_generate_completions(env, &eb, true);
                    break;
                case IC_KEY_PASTE_START:  // bracketed paste start marker
                case IC_KEY_PASTE_END:    // bracketed paste end marker
                    // Ignore these event markers - they're handled at the TTY level
                    // to control NUL byte interpretation
                    break;

                // completion, history, help, undo
                case KEY_TAB:
                case WITH_ALT('?'):
                    edit_generate_completions(env, &eb, false);
                    break;
                case KEY_CTRL_R:
                case KEY_CTRL_S:
                    edit_history_search_with_current_word(env, &eb);
                    break;
                case KEY_CTRL_P:
                    edit_history_prev(env, &eb);
                    break;
                case KEY_CTRL_N:
                    edit_history_next(env, &eb);
                    break;
                case KEY_CTRL_L:
                    edit_clear_screen(env, &eb);
                    break;
                case KEY_CTRL_Z:
                case WITH_CTRL('_'):
                    edit_undo_restore(env, &eb);
                    break;
                case KEY_CTRL_Y:
                    edit_redo_restore(env, &eb);
                    break;
                case KEY_F1:
                    edit_show_help(env, &eb);
                    break;

                // navigation
                case KEY_LEFT:
                case KEY_CTRL_B:
                    edit_cursor_left(env, &eb);
                    break;
                case KEY_RIGHT:
                case KEY_CTRL_F:
                    if (eb.pos == sbuf_len(eb.input)) {
                        edit_generate_completions(env, &eb, false);
                    } else {
                        edit_cursor_right(env, &eb);
                    }
                    break;
                case KEY_UP:
                    edit_cursor_row_up(env, &eb);
                    break;
                case KEY_DOWN:
                    edit_cursor_row_down(env, &eb);
                    break;
                case KEY_HOME:
                case KEY_CTRL_A:
                    edit_cursor_line_start(env, &eb);
                    break;
                case KEY_END:
                case KEY_CTRL_E:
                    edit_cursor_line_end(env, &eb);
                    break;
                case KEY_CTRL_LEFT:
                case WITH_SHIFT(KEY_LEFT):
                case WITH_ALT('b'):
                    edit_cursor_prev_word(env, &eb);
                    break;
                case KEY_CTRL_RIGHT:
                case WITH_SHIFT(KEY_RIGHT):
                case WITH_ALT('f'):
                    if (eb.pos == sbuf_len(eb.input)) {
                        edit_generate_completions(env, &eb, false);
                    } else {
                        edit_cursor_next_word(env, &eb);
                    }
                    break;
                case KEY_CTRL_HOME:
                case WITH_SHIFT(KEY_HOME):
                case KEY_PAGEUP:
                case WITH_ALT('<'):
                    edit_cursor_to_start(env, &eb);
                    break;
                case KEY_CTRL_END:
                case WITH_SHIFT(KEY_END):
                case KEY_PAGEDOWN:
                case WITH_ALT('>'):
                    edit_cursor_to_end(env, &eb);
                    break;
                case WITH_ALT('m'):
                    edit_cursor_match_brace(env, &eb);
                    break;

                // deletion
                case KEY_BACKSP:
                    edit_backspace(env, &eb);
                    break;
                case KEY_DEL:
                    edit_delete_char(env, &eb);
                    break;
                case WITH_ALT('d'):
                    edit_delete_to_end_of_word(env, &eb);
                    break;
                case KEY_CTRL_W:
                    edit_delete_to_start_of_ws_word(env, &eb);
                    break;
                case WITH_ALT(KEY_DEL):
                case WITH_ALT(KEY_BACKSP):
                    edit_delete_to_start_of_word(env, &eb);
                    break;
                case KEY_CTRL_U:
                    edit_delete_to_start_of_line(env, &eb);
                    break;
                case KEY_CTRL_K:
                    edit_delete_to_end_of_line(env, &eb);
                    break;
                case KEY_CTRL_T:
                    edit_swap_char(env, &eb);
                    break;

                // Editing
                case KEY_SHIFT_TAB:
                case KEY_LINEFEED:  // '\n' (ctrl+J, shift+enter)
                    if (!env->singleline_only) {
                        if (editor_input_has_unclosed_heredoc(&eb)) {
                            request_submit = true;
                        } else {
                            edit_insert_char(env, &eb, '\n');
                        }
                    }
                    break;
                default: {
                    char chr;
                    unicode_t uchr;
                    if (code_is_ascii_char(c, &chr)) {
                        edit_insert_char(env, &eb, chr);
                    } else if (code_is_unicode(c, &uchr)) {
                        edit_insert_unicode(env, &eb, uchr);
                    } else {
                        // Try the unhandled key callback before ignoring
                        // bool handled = false;
                        // if (env->unhandled_key_handler != NULL) {
                        //     handled = env->unhandled_key_handler(c, env->unhandled_key_arg);
                        // }
                        // if (!handled) {
                        //     debug_msg("edit: ignore code: 0x%04x\n", c);
                        // }
                        // debug_msg("edit: ignore code: 0x%04x\n", c);
                    }
                    break;
                }
            }

        if (request_submit || eb.request_submit) {
            c = KEY_ENTER;
            break;
        }
    }

    // goto end
    eb.pos = sbuf_len(eb.input);

    // refresh once more but without brace matching
    bool bm = env->no_bracematch;
    env->no_bracematch = true;
    edit_refresh(env, &eb);
    env->no_bracematch = bm;

    // save result
    char* res;
    if (ctrl_d_pressed) {
        res = mem_strdup(env->mem, IC_READLINE_TOKEN_CTRL_D);
    } else if (ctrl_c_pressed) {
        res = mem_strdup(env->mem, IC_READLINE_TOKEN_CTRL_C);
    } else if ((c == KEY_CTRL_D && sbuf_len(eb.input) == 0) || c == KEY_CTRL_C ||
               c == KEY_EVENT_STOP) {
        res = NULL;
    } else if (!tty_is_utf8(env->tty)) {
        res = sbuf_strdup_from_utf8(eb.input);
    } else {
        res = sbuf_strdup(eb.input);
    }

    if (env->prompt_cleanup && res != NULL && c == KEY_ENTER) {
        edit_apply_prompt_cleanup(env, &eb, res);
    }

    // update history in memory (file saving handled after execution)
    history_update(env->history, sbuf_string(eb.input));
    if (res == NULL || sbuf_len(eb.input) <= 1) {
        ic_history_remove_last();
    }

    // Clear the current editor pointer
    env->current_editor = NULL;

    // free resources
    editstate_done(env->mem, &eb.undo);
    editstate_done(env->mem, &eb.redo);
    attrbuf_free(eb.attrs);
    attrbuf_free(eb.attrs_extra);
    sbuf_free(eb.input);
    sbuf_free(eb.extra);
    sbuf_free(eb.hint);
    sbuf_free(eb.hint_help);
    sbuf_free(eb.history_prefix);
    sbuf_free(eb.inline_right_plain_cache);
    mem_free(env->mem,
             (void*)eb.prompt_text);  // Free the allocated last line prompt

    return res;
}

//-------------------------------------------------------------
// Public API for buffer control during readline
//-------------------------------------------------------------

ic_public bool ic_set_buffer(const char* buffer) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->current_editor == NULL)
        return false;

    editor_t* eb = env->current_editor;

    // Clear or set the buffer
    if (buffer == NULL) {
        sbuf_clear(eb->input);
        eb->pos = 0;
    } else {
        sbuf_replace(eb->input, buffer);
        eb->pos = sbuf_len(eb->input);  // Move cursor to end
    }

    // Mark as modified
    eb->modified = true;

    // Refresh the display
    edit_refresh(env, eb);

    return true;
}

ic_public const char* ic_get_buffer(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->current_editor == NULL)
        return NULL;

    editor_t* eb = env->current_editor;
    return sbuf_string(eb->input);
}

ic_public bool ic_get_cursor_pos(size_t* out_pos) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->current_editor == NULL || out_pos == NULL)
        return false;

    editor_t* eb = env->current_editor;
    *out_pos = (size_t)(eb->pos >= 0 ? eb->pos : 0);
    return true;
}

ic_public bool ic_set_cursor_pos(size_t pos) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->current_editor == NULL)
        return false;

    editor_t* eb = env->current_editor;
    ssize_t len = sbuf_len(eb->input);

    // Clamp position to valid range
    if ((ssize_t)pos > len) {
        pos = (size_t)len;
    }

    eb->pos = (ssize_t)pos;
    edit_refresh(env, eb);
    return true;
}

ic_public bool ic_request_submit(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->current_editor == NULL)
        return false;

    editor_t* eb = env->current_editor;
    eb->request_submit = true;
    return true;
}

ic_public bool ic_current_loop_reset(const char* new_buffer, const char* new_prompt,
                                     const char* new_inline_right) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->current_editor == NULL)
        return false;

    editor_t* eb = env->current_editor;

    // Update buffer if provided
    if (new_buffer != NULL) {
        sbuf_replace(eb->input, new_buffer);
        eb->pos = sbuf_len(eb->input);  // Move cursor to end
        eb->modified = true;
    }

    // Update prompt if provided
    if (new_prompt != NULL) {
        // Free the old prompt text
        mem_free(env->mem, (void*)eb->prompt_text);

        // Extract and set the new prompt (handle multi-line prompts)
        char* last_line_prompt = extract_last_prompt_line(env->mem, new_prompt);
        eb->prompt_text = last_line_prompt;

        // Print prefix lines if any
        eb->prompt_prefix_lines = print_prompt_prefix_lines(env, new_prompt);
        eb->prompt_width_cache_valid = false;
    }

    // Update inline right text if provided
    if (new_inline_right != NULL) {
        eb->inline_right_text = new_inline_right;
        eb->cached_inline_right_text = NULL;
        eb->inline_right_width = 0;
        eb->inline_right_width_valid = false;
        if (eb->inline_right_plain_cache != NULL) {
            sbuf_clear(eb->inline_right_plain_cache);
        }
    }

    // Clear current display and reset cursor tracking
    edit_clear(env, eb);
    eb->cur_row = 0;
    eb->cur_rows = 1;

    // Rewrite the prompt
    edit_write_prompt(env, eb, 0, false, 0);

    // Refresh the entire display
    edit_refresh(env, eb);

    return true;
}

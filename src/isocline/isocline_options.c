/*
  isocline_options.c

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
    Runtime configuration helpers split from the original isocline.c file.
-----------------------------------------------------------------------------*/

#include "common.h"
#include "env.h"
#include "env_internal.h"

#include <string.h>

static ic_abbreviation_entry_t* ic_env_find_abbreviation(ic_env_t* env, const char* trigger,
                                                         ssize_t trigger_len, ssize_t* out_index) {
    if (env == NULL || trigger == NULL || trigger_len <= 0)
        return NULL;
    for (ssize_t i = 0; i < env->abbreviation_count; ++i) {
        ic_abbreviation_entry_t* entry = &env->abbreviations[i];
        if (entry->trigger_len == trigger_len &&
            strncmp(entry->trigger, trigger, (size_t)trigger_len) == 0) {
            if (out_index != NULL) {
                *out_index = i;
            }
            return entry;
        }
    }
    return NULL;
}

static bool ic_env_ensure_abbreviation_capacity(ic_env_t* env, ssize_t needed) {
    if (env->abbreviation_capacity >= needed)
        return true;
    ssize_t new_capacity = (env->abbreviation_capacity == 0 ? 4 : env->abbreviation_capacity * 2);
    while (new_capacity < needed) {
        new_capacity *= 2;
    }
    ic_abbreviation_entry_t* resized =
        mem_realloc_tp(env->mem, ic_abbreviation_entry_t, env->abbreviations, new_capacity);
    if (resized == NULL)
        return false;
    env->abbreviations = resized;
    env->abbreviation_capacity = new_capacity;
    return true;
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

ic_public bool ic_enable_history_fuzzy_case_sensitive(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return true;
    return history_set_fuzzy_case_sensitive(env->history, enable);
}

ic_public bool ic_history_fuzzy_search_is_case_sensitive(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return true;
    return history_is_fuzzy_case_sensitive(env->history);
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

ic_public void ic_history_add_with_exit_code(const char* entry, int exit_code) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    history_push_with_exit_code(env->history, entry, exit_code);
}

ic_public void ic_history_add(const char* entry) {
    ic_history_add_with_exit_code(entry, IC_HISTORY_EXIT_CODE_UNKNOWN);
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

ic_public size_t ic_set_multiline_start_line_count(size_t line_count) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return 1;

    size_t prev = env->multiline_start_line_count;
    if (line_count < 1) {
        line_count = 1;
    }

    const size_t max_lines = 256;  // prevent pathological allocations
    if (line_count > max_lines) {
        line_count = max_lines;
    }

    env->multiline_start_line_count = line_count;
    return prev;
}

ic_public size_t ic_get_multiline_start_line_count(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return 1;
    return env->multiline_start_line_count;
}

ic_public bool ic_enable_line_numbers(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->show_line_numbers;
    env->show_line_numbers = enable;
    if (!enable) {
        env->relative_line_numbers = false;
    }
    return prev;
}

ic_public bool ic_enable_relative_line_numbers(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->relative_line_numbers;
    env->relative_line_numbers = enable;
    if (enable) {
        env->show_line_numbers = true;
    }
    return prev;
}

ic_public bool ic_line_numbers_are_enabled(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    return env->show_line_numbers;
}

ic_public bool ic_line_numbers_are_relative(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    return env->relative_line_numbers;
}

ic_public bool ic_enable_line_numbers_with_continuation_prompt(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->allow_line_numbers_with_continuation_prompt;
    env->allow_line_numbers_with_continuation_prompt = enable;
    return prev;
}

ic_public bool ic_line_numbers_with_continuation_prompt_are_enabled(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    return env->allow_line_numbers_with_continuation_prompt;
}

ic_public bool ic_enable_line_number_prompt_replacement(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->replace_prompt_line_with_line_number;
    env->replace_prompt_line_with_line_number = enable;
    return prev;
}

ic_public bool ic_line_number_prompt_replacement_is_enabled(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    return env->replace_prompt_line_with_line_number;
}

ic_public bool ic_enable_current_line_number_highlight(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->highlight_current_line_number;
    env->highlight_current_line_number = enable;
    return prev;
}

ic_public bool ic_current_line_number_highlight_is_enabled(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    return env->highlight_current_line_number;
}

ic_public bool ic_enable_visible_whitespace(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->show_whitespace_characters;
    env->show_whitespace_characters = enable;
    return prev;
}

ic_public void ic_set_whitespace_marker(const char* marker) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    mem_free(env->mem, env->whitespace_marker);
    env->whitespace_marker = NULL;
    if (marker != NULL && marker[0] != '\0') {
        env->whitespace_marker = mem_strdup(env->mem, marker);
    }
}

ic_public const char* ic_get_whitespace_marker(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return NULL;
    return ic_env_get_whitespace_marker(env);
}

ic_public bool ic_enable_hint(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->no_hint;
    env->no_hint = !enable;
    return !prev;
}

ic_public bool ic_enable_spell_correct(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->spell_correct;
    env->spell_correct = enable;
    return prev;
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

ic_public ic_status_hint_mode_t ic_set_status_hint_mode(ic_status_hint_mode_t mode) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return IC_STATUS_HINT_NORMAL;

    ic_status_hint_mode_t prev = env->status_hint_mode;
    switch (mode) {
        case IC_STATUS_HINT_OFF:
        case IC_STATUS_HINT_NORMAL:
        case IC_STATUS_HINT_TRANSIENT:
        case IC_STATUS_HINT_PERSISTENT:
            env->status_hint_mode = mode;
            break;
        default:
            env->status_hint_mode = IC_STATUS_HINT_NORMAL;
            break;
    }
    return prev;
}

ic_public ic_status_hint_mode_t ic_get_status_hint_mode(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return IC_STATUS_HINT_NORMAL;
    return env->status_hint_mode;
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

ic_public bool ic_prompt_cleanup_is_enabled(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    return env->prompt_cleanup;
}

ic_public size_t ic_prompt_cleanup_extra_lines(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return 0;
    return env->prompt_cleanup_extra_lines;
}

ic_public bool ic_enable_prompt_cleanup_newline(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->prompt_cleanup_newline_after_execution;
    env->prompt_cleanup_newline_after_execution = enable;
    return prev;
}

ic_public bool ic_prompt_cleanup_newline_is_enabled(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    return env->prompt_cleanup_newline_after_execution;
}

ic_public bool ic_enable_prompt_cleanup_empty_line(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->prompt_cleanup_add_empty_line;
    env->prompt_cleanup_add_empty_line = enable;
    return prev;
}

ic_public bool ic_prompt_cleanup_empty_line_is_enabled(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    return env->prompt_cleanup_add_empty_line;
}

ic_public bool ic_enable_prompt_cleanup_truncate_multiline(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->prompt_cleanup_truncate_multiline;
    env->prompt_cleanup_truncate_multiline = enable;
    return prev;
}

ic_public bool ic_prompt_cleanup_truncate_multiline_is_enabled(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    return env->prompt_cleanup_truncate_multiline;
}

ic_public bool ic_enable_inline_right_prompt_cursor_follow(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    bool prev = env->inline_right_prompt_follows_cursor;
    env->inline_right_prompt_follows_cursor = enable;
    return prev;
}

ic_public bool ic_inline_right_prompt_follows_cursor(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    return env->inline_right_prompt_follows_cursor;
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

ic_public bool ic_add_abbreviation(const char* trigger, const char* expansion) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || trigger == NULL || expansion == NULL)
        return false;

    ssize_t trigger_len = ic_strlen(trigger);
    if (trigger_len <= 0)
        return false;

    for (ssize_t i = 0; i < trigger_len; ++i) {
        if (ic_char_is_white(trigger + i, 1)) {
            return false;
        }
    }

    ic_abbreviation_entry_t* existing = ic_env_find_abbreviation(env, trigger, trigger_len, NULL);
    if (existing != NULL) {
        char* expansion_copy = mem_strdup(env->mem, expansion);
        if (expansion_copy == NULL) {
            return false;
        }
        mem_free(env->mem, existing->expansion);
        existing->expansion = expansion_copy;
        return true;
    }

    if (!ic_env_ensure_abbreviation_capacity(env, env->abbreviation_count + 1)) {
        return false;
    }

    char* trigger_copy = mem_strdup(env->mem, trigger);
    if (trigger_copy == NULL) {
        return false;
    }

    char* expansion_copy = mem_strdup(env->mem, expansion);
    if (expansion_copy == NULL) {
        mem_free(env->mem, trigger_copy);
        return false;
    }

    ic_abbreviation_entry_t* entry = &env->abbreviations[env->abbreviation_count];
    entry->trigger = trigger_copy;
    entry->expansion = expansion_copy;
    entry->trigger_len = trigger_len;
    env->abbreviation_count += 1;
    return true;
}

ic_public bool ic_remove_abbreviation(const char* trigger) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || trigger == NULL || env->abbreviation_count <= 0)
        return false;

    ssize_t trigger_len = ic_strlen(trigger);
    if (trigger_len <= 0)
        return false;

    ssize_t index = -1;
    ic_abbreviation_entry_t* entry = ic_env_find_abbreviation(env, trigger, trigger_len, &index);
    if (entry == NULL)
        return false;

    mem_free(env->mem, entry->trigger);
    mem_free(env->mem, entry->expansion);

    ssize_t remaining = env->abbreviation_count - index - 1;
    if (remaining > 0) {
        ic_memmove(&env->abbreviations[index], &env->abbreviations[index + 1],
                   remaining * ssizeof(ic_abbreviation_entry_t));
    }

    env->abbreviation_count -= 1;
    return true;
}

ic_public void ic_clear_abbreviations(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->abbreviation_count <= 0 || env->abbreviations == NULL)
        return;

    for (ssize_t i = 0; i < env->abbreviation_count; ++i) {
        mem_free(env->mem, env->abbreviations[i].trigger);
        mem_free(env->mem, env->abbreviations[i].expansion);
    }

    mem_free(env->mem, env->abbreviations);
    env->abbreviations = NULL;
    env->abbreviation_count = 0;
    env->abbreviation_capacity = 0;
}

ic_public void ic_set_default_highlighter(ic_highlight_fun_t* highlighter, void* arg) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    env->highlighter = highlighter;
    env->highlighter_arg = arg;
}

ic_public void ic_set_unhandled_key_handler(ic_unhandled_key_fun_t* callback, void* arg) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    env->unhandled_key_handler = callback;
    env->unhandled_key_arg = arg;
}

ic_public void ic_set_status_message_callback(ic_status_message_fun_t* callback, void* arg) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    env->status_message_callback = callback;
    env->status_message_arg = arg;
}

ic_public void ic_set_check_for_continuation_or_return_callback(
    ic_check_for_continuation_or_return_fun_t* callback, void* arg) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    env->continuation_check_callback = callback;
    env->continuation_check_arg = arg;
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

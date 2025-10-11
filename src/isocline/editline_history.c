/* ----------------------------------------------------------------------------
  Copyright (c) 2021, Daan Leijen
  Largely Modified by Caden Finley 2025 for CJ's Shell
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License. A copy of the license can be
  found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/

//-------------------------------------------------------------
// History search: this file is included in editline.c
//-------------------------------------------------------------

static void edit_history_at(ic_env_t* env, editor_t* eb, int ofs) {
    if (eb->modified) {
        history_update(env->history, sbuf_string(eb->input));
        eb->history_idx = 0;
        eb->modified = false;
    }
    const char* entry = history_get(env->history, eb->history_idx + ofs);

    if (entry == NULL) {
        term_beep(env->term);
    } else {
        eb->history_idx += ofs;
        sbuf_replace(eb->input, entry);
        if (ofs > 0) {
            ssize_t end = sbuf_find_line_end(eb->input, 0);
            eb->pos = (end < 0 ? 0 : end);
        } else {
            eb->pos = sbuf_len(eb->input);
        }
        edit_refresh(env, eb);
    }
}

static void edit_history_prev(ic_env_t* env, editor_t* eb) {
    edit_history_at(env, eb, 1);
}

static void edit_history_next(ic_env_t* env, editor_t* eb) {
    edit_history_at(env, eb, -1);
}

static void edit_history_prefix_search(ic_env_t* env, editor_t* eb, bool backward) {
    if (eb->modified) {
        history_update(env->history, sbuf_string(eb->input));
        eb->history_idx = 0;
        eb->modified = false;
    }

    const char* current_input = sbuf_string(eb->input);
    if (current_input == NULL)
        current_input = "";

    if (eb->history_idx > 0 || strlen(current_input) == 0) {
        if (backward) {
            edit_history_prev(env, eb);
        } else {
            edit_history_next(env, eb);
        }
        return;
    }

    const char* prefix = current_input;
    ssize_t start_idx = backward ? 1 : 0;

    ssize_t found_idx;
    if (history_search_prefix(env->history, start_idx, prefix, backward, &found_idx)) {
        const char* entry = history_get(env->history, found_idx);
        if (entry != NULL) {
            eb->history_idx = found_idx;
            sbuf_replace(eb->input, entry);
            eb->pos = sbuf_len(eb->input);
            edit_refresh(env, eb);
        } else {
            term_beep(env->term);
        }
    } else {
        if (backward) {
            edit_history_prev(env, eb);
        } else {
            edit_history_next(env, eb);
        }
    }
}

static void edit_history_prefix_prev(ic_env_t* env, editor_t* eb) {
    edit_history_prefix_search(env, eb, true);
}

static void edit_history_prefix_next(ic_env_t* env, editor_t* eb) {
    edit_history_prefix_search(env, eb, false);
}

typedef struct hsearch_s {
    struct hsearch_s* next;
    ssize_t hidx;
    ssize_t match_pos;
    ssize_t match_len;
    bool cinsert;
} hsearch_t;

static void hsearch_push(alloc_t* mem, hsearch_t** hs, ssize_t hidx, ssize_t mpos, ssize_t mlen,
                         bool cinsert) {
    hsearch_t* h = mem_zalloc_tp(mem, hsearch_t);
    if (h == NULL)
        return;
    h->hidx = hidx;
    h->match_pos = mpos;
    h->match_len = mlen;
    h->cinsert = cinsert;
    h->next = *hs;
    *hs = h;
}

static bool hsearch_pop(alloc_t* mem, hsearch_t** hs, ssize_t* hidx, ssize_t* match_pos,
                        ssize_t* match_len, bool* cinsert) {
    hsearch_t* h = *hs;
    if (h == NULL)
        return false;
    *hs = h->next;
    if (hidx != NULL)
        *hidx = h->hidx;
    if (match_pos != NULL)
        *match_pos = h->match_pos;
    if (match_len != NULL)
        *match_len = h->match_len;
    if (cinsert != NULL)
        *cinsert = h->cinsert;
    mem_free(mem, h);
    return true;
}

static void hsearch_done(alloc_t* mem, hsearch_t* hs) {
    while (hs != NULL) {
        hsearch_t* next = hs->next;
        mem_free(mem, hs);
        hs = next;
    }
}

#define MAX_FUZZY_RESULTS 50

static const char* get_first_line_end(const char* str) {
    if (str == NULL)
        return NULL;
    const char* p = str;
    while (*p && *p != '\n' && *p != '\r') {
        p++;
    }
    return p;
}

static void edit_history_fuzzy_search(ic_env_t* env, editor_t* eb, char* initial) {
    if (history_count(env->history) <= 0) {
        term_beep(env->term);
        return;
    }

    if (eb->modified) {
        history_update(env->history, sbuf_string(eb->input));
        eb->history_idx = 0;
        eb->modified = false;
    }

    editor_undo_capture(eb);
    eb->disable_undo = true;
    bool old_hint = ic_enable_hint(false);
    const char* prompt_text = eb->prompt_text;
    eb->prompt_text = "history search: ";

    history_match_t* matches =
        (history_match_t*)mem_zalloc_tp_n(env->mem, history_match_t, MAX_FUZZY_RESULTS);
    if (matches == NULL) {
        term_beep(env->term);
        eb->disable_undo = false;
        eb->prompt_text = prompt_text;
        ic_enable_hint(old_hint);
        return;
    }

    ssize_t match_count = 0;
    ssize_t selected_idx = 0;

    if (initial != NULL) {
        sbuf_replace(eb->input, initial);
        eb->pos = ic_strlen(initial);
    } else {
        sbuf_clear(eb->input);
        eb->pos = 0;
    }

again:

{
    const char* query = sbuf_string(eb->input);
    history_fuzzy_search(env->history, query ? query : "", matches, MAX_FUZZY_RESULTS,
                         &match_count);
}

    if (selected_idx >= match_count) {
        selected_idx = match_count > 0 ? match_count - 1 : 0;
    }
    if (selected_idx < 0) {
        selected_idx = 0;
    }

    sbuf_clear(eb->extra);

    if (match_count > 0) {
        const char* query = sbuf_string(eb->input);
        bool is_filtered = (query != NULL && query[0] != '\0');

        if (is_filtered) {
            sbuf_appendf(eb->extra, "[ic-info]%zd match%s found[/]\n", match_count,
                         match_count == 1 ? "" : "es");
        } else {
            ssize_t total_history = history_count(env->history);
            sbuf_appendf(eb->extra, "[ic-info]History (%zd entr%s)[/]\n", total_history,
                         total_history == 1 ? "y" : "ies");
        }

        ssize_t term_height = term_get_height(env->term);
        ssize_t available_lines = term_height - 4;
        if (available_lines < 3) {
            available_lines = 3;
        }

        ssize_t display_count = (match_count > available_lines) ? available_lines : match_count;

        ssize_t scroll_offset = 0;
        if (selected_idx >= display_count) {
            scroll_offset = selected_idx - display_count + 1;
        }

        if (scroll_offset + display_count > match_count) {
            scroll_offset = match_count - display_count;
            if (scroll_offset < 0)
                scroll_offset = 0;
        }

        for (ssize_t i = 0; i < display_count; i++) {
            ssize_t match_idx = scroll_offset + i;
            if (match_idx >= match_count)
                break;

            const char* entry = history_get(env->history, matches[match_idx].hidx);
            if (entry == NULL)
                continue;

            const char* line_end = get_first_line_end(entry);
            ssize_t entry_len = line_end ? (line_end - entry) : (ssize_t)strlen(entry);
            bool is_multiline = (line_end && (*line_end == '\n' || *line_end == '\r'));

            if (match_idx == selected_idx) {
                sbuf_append(eb->extra, "[ic-emphasis][reverse]> [/][!pre]");
            } else {
                sbuf_append(eb->extra, "[ic-diminish]  [/][!pre]");
            }

            if (is_filtered && matches[match_idx].match_len > 0 &&
                matches[match_idx].match_pos >= 0) {
                if (matches[match_idx].match_pos < entry_len) {
                    sbuf_append_n(eb->extra, entry, matches[match_idx].match_pos);

                    ssize_t match_end = matches[match_idx].match_pos + matches[match_idx].match_len;
                    ssize_t highlight_len = (match_end > entry_len)
                                                ? (entry_len - matches[match_idx].match_pos)
                                                : matches[match_idx].match_len;
                    sbuf_append(eb->extra, "[/pre][u ic-emphasis][!pre]");
                    sbuf_append_n(eb->extra, entry + matches[match_idx].match_pos, highlight_len);
                    sbuf_append(eb->extra, "[/pre][/u][!pre]");

                    if (match_end < entry_len) {
                        sbuf_append_n(eb->extra, entry + match_end, entry_len - match_end);
                    }
                } else {
                    sbuf_append_n(eb->extra, entry, entry_len);
                }
            } else {
                sbuf_append_n(eb->extra, entry, entry_len);
            }

            if (is_multiline) {
                sbuf_append(eb->extra, "...");
            }

            sbuf_append(eb->extra, "[/pre]");

            if (match_idx == selected_idx) {
                sbuf_append(eb->extra, "[/reverse][/ic-emphasis]");
            } else {
                sbuf_append(eb->extra, "[/ic-diminish]");
            }

            sbuf_append(eb->extra, "\n");
        }

        if (match_count > display_count) {
            ssize_t hidden_above = scroll_offset;
            ssize_t hidden_below = match_count - (scroll_offset + display_count);
            if (hidden_above > 0 && hidden_below > 0) {
                sbuf_appendf(eb->extra, "[ic-info]  (%zd above, %zd below)[/]\n", hidden_above,
                             hidden_below);
            } else if (hidden_above > 0) {
                sbuf_appendf(eb->extra, "[ic-info]  (%zd more above)[/]\n", hidden_above);
            } else if (hidden_below > 0) {
                sbuf_appendf(eb->extra, "[ic-info]  (%zd more below)[/]\n", hidden_below);
            }
        }
    } else {
        sbuf_append(eb->extra, "[ic-info]No matches found[/]\n");
    }

    if (!env->no_help) {
        sbuf_append(eb->extra, "[ic-diminish](↑↓:navigate enter:run tab:edit esc:cancel)[/]");
    }

    edit_refresh(env, eb);

    code_t c = tty_read(env->tty);
    if (tty_term_resize_event(env->tty)) {
        edit_resize(env, eb);
    }
    sbuf_clear(eb->extra);

    if (c == KEY_ESC || c == KEY_BELL || c == KEY_CTRL_C) {
        sbuf_clear(eb->extra);
        eb->disable_undo = false;
        editor_undo_restore(eb, false);
        mem_free(env->mem, matches);
        eb->prompt_text = prompt_text;
        ic_enable_hint(old_hint);
        edit_refresh(env, eb);
        return;
    } else if (c == KEY_ENTER) {
        sbuf_clear(eb->extra);
        if (match_count > 0 && selected_idx >= 0 && selected_idx < match_count) {
            const char* selected = history_get(env->history, matches[selected_idx].hidx);
            if (selected != NULL) {
                editor_undo_forget(eb);
                sbuf_replace(eb->input, selected);
                eb->pos = sbuf_len(eb->input);
                eb->modified = false;
                eb->history_idx = matches[selected_idx].hidx;
            }
        }
        mem_free(env->mem, matches);
        eb->disable_undo = false;
        eb->prompt_text = prompt_text;
        ic_enable_hint(old_hint);
        edit_refresh(env, eb);

        tty_code_pushback(env->tty, KEY_ENTER);
        return;
    } else if (c == KEY_TAB) {
        sbuf_clear(eb->extra);
        if (match_count > 0 && selected_idx >= 0 && selected_idx < match_count) {
            const char* selected = history_get(env->history, matches[selected_idx].hidx);
            if (selected != NULL) {
                editor_undo_forget(eb);
                sbuf_replace(eb->input, selected);
                eb->pos = sbuf_len(eb->input);
                eb->modified = false;
                eb->history_idx = matches[selected_idx].hidx;
            }
        }
        mem_free(env->mem, matches);
        eb->disable_undo = false;
        eb->prompt_text = prompt_text;
        ic_enable_hint(old_hint);
        edit_refresh(env, eb);
        return;
    } else if (c == KEY_UP || c == KEY_CTRL_P) {
        if (selected_idx > 0) {
            selected_idx--;
        } else {
            term_beep(env->term);
        }
        goto again;
    } else if (c == KEY_DOWN || c == KEY_CTRL_N) {
        if (selected_idx < match_count - 1) {
            selected_idx++;
        } else {
            term_beep(env->term);
        }
        goto again;
    } else if (c == KEY_BACKSP) {
        if (eb->pos > 0) {
            edit_backspace(env, eb);
            selected_idx = 0;
        }
        goto again;
    } else if (c == KEY_DEL) {
        edit_delete_char(env, eb);
        selected_idx = 0;
        goto again;
    } else if (c == KEY_F1) {
        edit_show_help(env, eb);
        goto again;
    } else {
        char chr;
        unicode_t uchr;
        if (code_is_ascii_char(c, &chr)) {
            edit_insert_char(env, eb, chr);
            selected_idx = 0;
            goto again;
        } else if (code_is_unicode(c, &uchr)) {
            edit_insert_unicode(env, eb, uchr);
            selected_idx = 0;
            goto again;
        } else {
            term_beep(env->term);
            goto again;
        }
    }
}

static void edit_history_fuzzy_search_with_current_word(ic_env_t* env, editor_t* eb) {
    char* initial = NULL;
    ssize_t start = sbuf_find_word_start(eb->input, eb->pos);
    if (start >= 0) {
        const ssize_t next = sbuf_next(eb->input, start, NULL);
        if (!ic_char_is_idletter(sbuf_string(eb->input) + start, (long)(next - start))) {
            start = next;
        }
        if (start >= 0 && start < eb->pos) {
            initial = mem_strndup(eb->mem, sbuf_string(eb->input) + start, eb->pos - start);
        }
    }
    edit_history_fuzzy_search(env, eb, initial);
    mem_free(env->mem, initial);
}

static void edit_history_search_incremental(ic_env_t* env, editor_t* eb, char* initial) {
    if (history_count(env->history) <= 0) {
        term_beep(env->term);
        return;
    }

    if (eb->modified) {
        history_update(env->history, sbuf_string(eb->input));
        eb->history_idx = 0;
        eb->modified = false;
    }

    editor_undo_capture(eb);
    eb->disable_undo = true;
    bool old_hint = ic_enable_hint(false);
    const char* prompt_text = eb->prompt_text;
    eb->prompt_text = "history search";

    hsearch_t* hs = NULL;
    ssize_t hidx = 1;
    ssize_t match_pos = 0;
    ssize_t match_len = 0;
    const char* hentry = NULL;

    if (initial != NULL) {
        const ssize_t initial_len = ic_strlen(initial);
        ssize_t ipos = 0;
        while (ipos < initial_len) {
            ssize_t next = str_next_ofs(initial, initial_len, ipos, NULL);
            if (next < 0)
                break;
            hsearch_push(eb->mem, &hs, hidx, match_pos, match_len, true);
            char c = initial[ipos + next];
            initial[ipos + next] = 0;
            if (history_search(env->history, hidx, initial, true, &hidx, &match_pos)) {
                match_len = ipos + next;
            } else if (ipos + next >= initial_len) {
                term_beep(env->term);
            }
            initial[ipos + next] = c;
            ipos += next;
        }
        sbuf_replace(eb->input, initial);
        eb->pos = ipos;
    } else {
        sbuf_clear(eb->input);
        eb->pos = 0;
    }

again:
    hentry = history_get(env->history, hidx);
    if (hentry != NULL) {
        sbuf_appendf(eb->extra, "[ic-info]%zd. [/][ic-diminish][!pre]", hidx);
        sbuf_append_n(eb->extra, hentry, match_pos);
        sbuf_append(eb->extra, "[/pre][u ic-emphasis][!pre]");
        sbuf_append_n(eb->extra, hentry + match_pos, match_len);
        sbuf_append(eb->extra, "[/pre][/u][!pre]");
        sbuf_append(eb->extra, hentry + match_pos + match_len);
        sbuf_append(eb->extra, "[/pre][/ic-diminish]");
        if (!env->no_help) {
            sbuf_append(eb->extra, "\n[ic-info](use tab for the next match)[/]");
        }
        sbuf_append(eb->extra, "\n");
    }
    edit_refresh(env, eb);

    code_t c = (hentry == NULL ? KEY_ESC : tty_read(env->tty));
    if (tty_term_resize_event(env->tty)) {
        edit_resize(env, eb);
    }
    sbuf_clear(eb->extra);

    if (c == KEY_ESC || c == KEY_BELL || c == KEY_CTRL_C) {
        c = 0;
        eb->disable_undo = false;
        editor_undo_restore(eb, false);
    } else if (c == KEY_ENTER) {
        c = 0;
        editor_undo_forget(eb);
        sbuf_replace(eb->input, hentry);
        eb->pos = sbuf_len(eb->input);
        eb->modified = false;
        eb->history_idx = hidx;
    } else if (c == KEY_BACKSP || c == KEY_CTRL_Z) {
        bool cinsert;
        if (hsearch_pop(env->mem, &hs, &hidx, &match_pos, &match_len, &cinsert)) {
            if (cinsert)
                edit_backspace(env, eb);
        }
        goto again;
    } else if (c == KEY_CTRL_R || c == KEY_TAB || c == KEY_UP) {
        hsearch_push(env->mem, &hs, hidx, match_pos, match_len, false);
        if (!history_search(env->history, hidx + 1, sbuf_string(eb->input), true, &hidx,
                            &match_pos)) {
            hsearch_pop(env->mem, &hs, NULL, NULL, NULL, NULL);
            term_beep(env->term);
        };
        goto again;
    } else if (c == KEY_CTRL_S || c == KEY_SHIFT_TAB || c == KEY_DOWN) {
        hsearch_push(env->mem, &hs, hidx, match_pos, match_len, false);
        if (!history_search(env->history, hidx - 1, sbuf_string(eb->input), false, &hidx,
                            &match_pos)) {
            hsearch_pop(env->mem, &hs, NULL, NULL, NULL, NULL);
            term_beep(env->term);
        };
        goto again;
    } else if (c == KEY_F1) {
        edit_show_help(env, eb);
        goto again;
    } else {
        char chr;
        unicode_t uchr;
        if (code_is_ascii_char(c, &chr)) {
            hsearch_push(env->mem, &hs, hidx, match_pos, match_len, true);
            edit_insert_char(env, eb, chr);
        } else if (code_is_unicode(c, &uchr)) {
            hsearch_push(env->mem, &hs, hidx, match_pos, match_len, true);
            edit_insert_unicode(env, eb, uchr);
        } else {
            term_beep(env->term);
            goto again;
        }

        if (history_search(env->history, hidx, sbuf_string(eb->input), true, &hidx, &match_pos)) {
            match_len = sbuf_len(eb->input);
        } else {
            term_beep(env->term);
        };
        goto again;
    }

    eb->disable_undo = false;
    hsearch_done(env->mem, hs);
    eb->prompt_text = prompt_text;
    ic_enable_hint(old_hint);
    edit_refresh(env, eb);
    if (c != 0)
        tty_code_pushback(env->tty, c);
}

static void edit_history_search_with_current_word(ic_env_t* env, editor_t* eb) {
    char* initial = NULL;
    ssize_t start = sbuf_find_word_start(eb->input, eb->pos);
    if (start >= 0) {
        const ssize_t next = sbuf_next(eb->input, start, NULL);
        if (!ic_char_is_idletter(sbuf_string(eb->input) + start, (long)(next - start))) {
            start = next;
        }
        if (start >= 0 && start < eb->pos) {
            initial = mem_strndup(eb->mem, sbuf_string(eb->input) + start, eb->pos - start);
        }
    }
    edit_history_fuzzy_search(env, eb, initial);
    mem_free(env->mem, initial);
}

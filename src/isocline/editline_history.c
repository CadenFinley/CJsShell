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

//-------------------------------------------------------------
// History search: this file is included in editline.c
//-------------------------------------------------------------

// Helper function to clear history preview
static void edit_clear_history_preview(editor_t* eb) {
    if (eb == NULL) {
        return;
    }
    if (sbuf_len(eb->extra) > 0) {
        sbuf_clear(eb->extra);
    }
    if (eb->history_prefix != NULL) {
        sbuf_clear(eb->history_prefix);
    }
    eb->history_prefix_active = false;
}

static void edit_history_at(ic_env_t* env, editor_t* eb, int ofs) {
    if (ofs == 0) {
        return;
    }

    if (eb->modified) {
        const char* current_input = sbuf_string(eb->input);
        if (eb->history_prefix != NULL) {
            sbuf_replace(eb->history_prefix, current_input != NULL ? current_input : "");
            eb->history_prefix_active = (sbuf_len(eb->history_prefix) > 0);
        } else {
            eb->history_prefix_active = false;
        }

        history_update(env->history, current_input != NULL ? current_input : "");
        eb->history_idx = 0;
        eb->modified = false;
    }

    history_snapshot_t snap = (history_snapshot_t){0};
    if (!history_snapshot_load(env->history, &snap, true)) {
        term_beep(env->term);
        return;
    }

    ssize_t total_history = history_snapshot_count(&snap);
    if (total_history <= 0) {
        term_beep(env->term);
        history_snapshot_free(env->history, &snap);
        return;
    }

    ssize_t steps = (ofs > 0) ? ofs : -ofs;
    int direction = (ofs > 0) ? 1 : -1;

    const char* prefix = NULL;
    ssize_t prefix_len = 0;
    if (eb->history_prefix_active && eb->history_prefix != NULL) {
        prefix = sbuf_string(eb->history_prefix);
        if (prefix != NULL) {
            prefix_len = (ssize_t)strlen(prefix);
            if (prefix_len == 0) {
                prefix = NULL;
            }
        } else {
            prefix_len = 0;
        }
    }

    ssize_t current_idx = eb->history_idx;
    for (ssize_t step_idx = 0; step_idx < steps; ++step_idx) {
        ssize_t candidate_idx = current_idx + direction;

        if (prefix != NULL) {
            ssize_t search_idx = current_idx + direction;
            bool match_found = false;
            while (search_idx >= 0 && search_idx < total_history) {
                const history_entry_t* candidate_entry = history_snapshot_get(&snap, search_idx);
                if (candidate_entry == NULL || candidate_entry->command == NULL) {
                    break;
                }
                if (strncmp(candidate_entry->command, prefix, (size_t)prefix_len) == 0 &&
                    candidate_entry->command[prefix_len] != '\0') {
                    candidate_idx = search_idx;
                    match_found = true;
                    break;
                }
                search_idx += direction;
            }

            if (!match_found && direction > 0) {
                if (eb->history_prefix != NULL) {
                    sbuf_clear(eb->history_prefix);
                }
                eb->history_prefix_active = false;
                prefix = NULL;
                prefix_len = 0;
                eb->history_idx = 0;
                current_idx = 0;
                --step_idx;
                continue;
            }
        }

        if (candidate_idx < 0 || candidate_idx >= total_history) {
            term_beep(env->term);
            history_snapshot_free(env->history, &snap);
            return;
        }

        current_idx = candidate_idx;
    }

    const history_entry_t* entry = history_snapshot_get(&snap, current_idx);
    if (entry == NULL || entry->command == NULL) {
        term_beep(env->term);
        history_snapshot_free(env->history, &snap);
        return;
    }

    eb->history_idx = current_idx;
    sbuf_replace(eb->input, entry->command);
    if (direction > 0) {
        ssize_t end = sbuf_find_line_end(eb->input, 0);
        eb->pos = (end < 0 ? 0 : end);
    } else {
        eb->pos = sbuf_len(eb->input);
    }

    sbuf_clear(eb->extra);

    edit_refresh(env, eb);
    history_snapshot_free(env->history, &snap);
}

static void edit_history_prev(ic_env_t* env, editor_t* eb) {
    edit_history_at(env, eb, 1);
}

static void edit_history_next(ic_env_t* env, editor_t* eb) {
    edit_history_at(env, eb, -1);
}

static ssize_t history_visible_prefix(const char* s, ssize_t len, ssize_t max_columns,
                                      ssize_t* width_out) {
    if (s == NULL || len <= 0 || max_columns <= 0) {
        if (width_out != NULL)
            *width_out = 0;
        return 0;
    }

    ssize_t pos = 0;
    ssize_t width = 0;
    while (pos < len) {
        ssize_t cw = 0;
        ssize_t next = str_next_ofs(s, len, pos, &cw);
        if (next <= 0)
            break;

        if (cw <= 0) {
            pos += next;
            continue;
        }

        if (width + cw > max_columns)
            break;

        width += cw;
        pos += next;
    }

    if (width_out != NULL)
        *width_out = width;
    return pos;
}

#define MAX_FUZZY_RESULTS 5000

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
    history_snapshot_t snap = {0};
    if (!history_snapshot_load(env->history, &snap, true)) {
        term_beep(env->term);
        return;
    }

    if (history_snapshot_count(&snap) <= 0) {
        term_beep(env->term);
        history_snapshot_free(env->history, &snap);
        return;
    }

    if (eb->modified) {
        history_update(env->history, sbuf_string(eb->input));
        eb->history_idx = 0;
        eb->modified = false;
        history_snapshot_free(env->history, &snap);
        if (!history_snapshot_load(env->history, &snap, true)) {
            term_beep(env->term);
            return;
        }
        if (history_snapshot_count(&snap) <= 0) {
            term_beep(env->term);
            history_snapshot_free(env->history, &snap);
            return;
        }
    }

    history_snapshot_free(env->history, &snap);

    editor_undo_capture(eb);
    eb->disable_undo = true;
    bool old_hint = ic_enable_hint(false);
    const char* prompt_text = eb->prompt_text;
    bool prompt_replacement = eb->replace_prompt_line_with_number;
    bool force_prompt_visibility = eb->force_prompt_text_visible;
    const ssize_t saved_line_number_width = eb->line_number_column_width;
    eb->force_prompt_text_visible = true;
    eb->replace_prompt_line_with_number = false;
    eb->prompt_text = "history search: ";

    history_match_t* matches =
        (history_match_t*)mem_zalloc_tp_n(env->mem, history_match_t, MAX_FUZZY_RESULTS);
    if (matches == NULL) {
        term_beep(env->term);
        eb->disable_undo = false;
        eb->prompt_text = prompt_text;
        eb->replace_prompt_line_with_number = prompt_replacement;
        eb->force_prompt_text_visible = force_prompt_visibility;
        eb->line_number_column_width = saved_line_number_width;
        ic_enable_hint(old_hint);
        return;
    }

    ssize_t match_count = 0;
    ssize_t selected_idx = 0;
    ssize_t scroll_offset = 0;
    ssize_t last_display_count = 0;
    ssize_t last_max_scroll = 0;

    if (initial != NULL) {
        sbuf_replace(eb->input, initial);
        eb->pos = ic_strlen(initial);
    } else {
        sbuf_clear(eb->input);
        eb->pos = 0;
    }

again:;

    last_display_count = 0;
    last_max_scroll = 0;

    bool showing_all_due_to_no_matches = false;
    bool exit_filter_applied = false;
    int exit_filter_code = IC_HISTORY_EXIT_CODE_UNKNOWN;
    bool case_sensitive_mode = ic_history_fuzzy_search_is_case_sensitive();

    {
        const char* query = sbuf_string(eb->input);
        history_fuzzy_search(env->history, query ? query : "", matches, MAX_FUZZY_RESULTS,
                             &match_count, &exit_filter_applied, &exit_filter_code);

        if (match_count == 0 && query != NULL && query[0] != '\0' && !exit_filter_applied) {
            history_fuzzy_search(env->history, "", matches, MAX_FUZZY_RESULTS, &match_count, NULL,
                                 NULL);
            showing_all_due_to_no_matches = true;
        }
    }

    history_snapshot_free(env->history, &snap);
    if (!history_snapshot_load(env->history, &snap, true)) {
        term_beep(env->term);
        match_count = 0;
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
        ssize_t total_history = history_snapshot_count(&snap);

        if (showing_all_due_to_no_matches) {
            sbuf_appendf(eb->extra, "[ic-info]No matches - showing all history (%zd entr%s)[/]\n",
                         total_history, total_history == 1 ? "y" : "ies");
        } else if (is_filtered) {
            if (exit_filter_applied && exit_filter_code != IC_HISTORY_EXIT_CODE_UNKNOWN) {
                sbuf_appendf(eb->extra, "[ic-info]%zd match%s found (exit %d)[/]\n", match_count,
                             match_count == 1 ? "" : "es", exit_filter_code);
            } else {
                sbuf_appendf(eb->extra, "[ic-info]%zd match%s found[/]\n", match_count,
                             match_count == 1 ? "" : "es");
            }
        } else {
            sbuf_appendf(eb->extra, "[ic-info]History (%zd entr%s)[/]\n", total_history,
                         total_history == 1 ? "y" : "ies");
        }

        sbuf_appendf(eb->extra, "[ic-info]Case sensitivity: %s (alt+c toggles)[/]\n",
                     case_sensitive_mode ? "sensitive" : "insensitive");

        ssize_t term_height = term_get_height(env->term);
        ssize_t term_width = term_get_width(env->term);
        ssize_t available_lines = term_height - 4;
        if (eb->prompt_prefix_lines > 0) {
            available_lines -= eb->prompt_prefix_lines;
        }
        if (available_lines < 3) {
            available_lines = 3;
        }

        ssize_t display_count = (match_count > available_lines) ? available_lines : match_count;
        if (display_count < 1) {
            display_count = 1;
        }

        ssize_t max_scroll = (match_count > display_count) ? (match_count - display_count) : 0;
        if (scroll_offset > max_scroll) {
            scroll_offset = max_scroll;
        }
        if (scroll_offset < 0) {
            scroll_offset = 0;
        }

        if (selected_idx < scroll_offset) {
            scroll_offset = selected_idx;
        } else if (selected_idx >= scroll_offset + display_count) {
            scroll_offset = selected_idx - display_count + 1;
        }

        if (scroll_offset < 0) {
            scroll_offset = 0;
        }
        if (scroll_offset > max_scroll) {
            scroll_offset = max_scroll;
        }

        last_display_count = display_count;
        last_max_scroll = max_scroll;

        for (ssize_t i = 0; i < display_count; i++) {
            ssize_t match_idx = scroll_offset + i;
            if (match_idx >= match_count)
                break;

            const history_entry_t* entry = history_snapshot_get(&snap, matches[match_idx].hidx);
            if (entry == NULL || entry->command == NULL)
                continue;

            char exit_buf[32] = {0};
            ssize_t exit_reserved_columns = 0;
            if (entry->exit_code != IC_HISTORY_EXIT_CODE_UNKNOWN) {
                int formatted =
                    snprintf(exit_buf, sizeof(exit_buf), " (exit %d)", entry->exit_code);
                if (formatted > 0) {
                    if (formatted >= (int)sizeof(exit_buf))
                        formatted = (int)sizeof(exit_buf) - 1;
                    exit_buf[formatted] = '\0';
                    exit_reserved_columns = formatted;
                } else {
                    exit_buf[0] = '\0';
                }
            }

            const char* display = entry->command;
            const char* line_end = get_first_line_end(display);
            ssize_t entry_len = line_end ? (line_end - display) : (ssize_t)strlen(display);
            bool is_multiline = (line_end && (*line_end == '\n' || *line_end == '\r'));

            ssize_t marker_columns = 4;
            ssize_t max_columns = term_width - marker_columns - exit_reserved_columns;
            if (max_columns < 4) {
                max_columns = 4;
            }

            // Limit preview width so wrapped entries do not push the prompt off-screen.
            ssize_t visible_width = 0;
            ssize_t visible_len =
                history_visible_prefix(display, entry_len, max_columns, &visible_width);
            bool truncated = (visible_len < entry_len);
            bool append_ellipsis = (is_multiline || truncated);

            if (append_ellipsis && max_columns > 3) {
                if (visible_width + 3 > max_columns) {
                    ssize_t adjusted_columns = max_columns - 3;
                    if (adjusted_columns < 1)
                        adjusted_columns = 1;
                    visible_len = history_visible_prefix(display, entry_len, adjusted_columns,
                                                         &visible_width);
                    truncated = (visible_len < entry_len) || truncated;
                }
            } else if (!truncated && !is_multiline) {
                append_ellipsis = false;
            }

            if (match_idx == selected_idx) {
                const char* arrow = tty_is_utf8(env->tty) ? "\xE2\x86\x92" : ">";
                sbuf_append(eb->extra, "[ic-emphasis]");
                sbuf_appendf(eb->extra, "%s ", arrow);
                sbuf_append(eb->extra, "[!pre]");
            } else {
                sbuf_append(eb->extra, "[ic-diminish]  [/][!pre]");
            }

            if (is_filtered && !showing_all_due_to_no_matches && matches[match_idx].match_len > 0 &&
                matches[match_idx].match_pos >= 0) {
                ssize_t match_pos = matches[match_idx].match_pos;
                ssize_t match_len = matches[match_idx].match_len;
                if (match_pos < 0)
                    match_pos = 0;
                if (match_pos < visible_len) {
                    if (match_pos > 0) {
                        ssize_t prefix_len = (match_pos <= visible_len) ? match_pos : visible_len;
                        sbuf_append_n(eb->extra, display, prefix_len);
                    }

                    if (match_len > 0) {
                        if (match_pos + match_len > entry_len) {
                            match_len = entry_len - match_pos;
                        }
                        if (match_pos + match_len > visible_len) {
                            match_len = visible_len - match_pos;
                        }
                    }

                    if (match_len > 0) {
                        sbuf_append(eb->extra, "[/pre][u ic-emphasis][!pre]");
                        sbuf_append_n(eb->extra, display + match_pos, match_len);
                        sbuf_append(eb->extra, "[/pre][/u][!pre]");
                    }

                    ssize_t suffix_start = match_pos + match_len;
                    if (suffix_start < visible_len) {
                        sbuf_append_n(eb->extra, display + suffix_start,
                                      visible_len - suffix_start);
                    }
                } else {
                    sbuf_append_n(eb->extra, display, visible_len);
                }
            } else {
                sbuf_append_n(eb->extra, display, visible_len);
            }

            if (append_ellipsis && max_columns > 3) {
                sbuf_append(eb->extra, "...");
            }

            sbuf_append(eb->extra, "[/pre]");

            if (exit_buf[0] != '\0') {
                sbuf_appendf(eb->extra, "[ic-diminish]%s[/]", exit_buf);
            }

            if (match_idx == selected_idx) {
                sbuf_append(eb->extra, "[/ic-emphasis]");
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
        scroll_offset = 0;
        if (exit_filter_applied && exit_filter_code != IC_HISTORY_EXIT_CODE_UNKNOWN) {
            sbuf_appendf(eb->extra, "[ic-info]No history entries with exit %d[/]\n",
                         exit_filter_code);
        } else {
            sbuf_append(eb->extra, "[ic-info]No matches found[/]\n");
        }
        sbuf_appendf(eb->extra, "[ic-info]Case sensitivity: %s (alt+c toggles)[/]\n",
                     case_sensitive_mode ? "sensitive" : "insensitive");
    }

    if (!env->no_help) {
        sbuf_append(eb->extra,
                    "[ic-diminish](↑↓:navigate shift+↑/↓:page enter:run tab:edit alt+c:case "
                    "esc:cancel)[/]");
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
        history_snapshot_free(env->history, &snap);
        mem_free(env->mem, matches);
        eb->prompt_text = prompt_text;
        eb->replace_prompt_line_with_number = prompt_replacement;
        eb->force_prompt_text_visible = force_prompt_visibility;
        eb->line_number_column_width = saved_line_number_width;
        ic_enable_hint(old_hint);
        edit_refresh(env, eb);
        return;
    } else if (c == KEY_ENTER) {
        sbuf_clear(eb->extra);
        if (match_count > 0 && selected_idx >= 0 && selected_idx < match_count) {
            const history_entry_t* selected =
                history_snapshot_get(&snap, matches[selected_idx].hidx);
            if (selected != NULL && selected->command != NULL) {
                editor_undo_forget(eb);
                sbuf_replace(eb->input, selected->command);
                eb->pos = sbuf_len(eb->input);
                bool expanded = edit_expand_abbreviation_if_needed(env, eb, false);
                eb->modified = expanded;
                eb->history_idx = matches[selected_idx].hidx;
            }
        }
        history_snapshot_free(env->history, &snap);
        mem_free(env->mem, matches);
        eb->disable_undo = false;
        eb->prompt_text = prompt_text;
        eb->replace_prompt_line_with_number = prompt_replacement;
        eb->force_prompt_text_visible = force_prompt_visibility;
        eb->line_number_column_width = saved_line_number_width;
        ic_enable_hint(old_hint);
        edit_refresh(env, eb);

        tty_code_pushback(env->tty, KEY_ENTER);
        return;
    } else if (c == KEY_TAB) {
        sbuf_clear(eb->extra);
        if (match_count > 0 && selected_idx >= 0 && selected_idx < match_count) {
            const history_entry_t* selected =
                history_snapshot_get(&snap, matches[selected_idx].hidx);
            if (selected != NULL && selected->command != NULL) {
                editor_undo_forget(eb);
                sbuf_replace(eb->input, selected->command);
                eb->pos = sbuf_len(eb->input);
                bool expanded = edit_expand_abbreviation_if_needed(env, eb, false);
                eb->modified = expanded;
                eb->history_idx = matches[selected_idx].hidx;
            }
        }
        history_snapshot_free(env->history, &snap);
        mem_free(env->mem, matches);
        eb->disable_undo = false;
        eb->prompt_text = prompt_text;
        eb->replace_prompt_line_with_number = prompt_replacement;
        eb->force_prompt_text_visible = force_prompt_visibility;
        eb->line_number_column_width = saved_line_number_width;
        ic_enable_hint(old_hint);
        edit_refresh(env, eb);
        return;
    } else if ((KEY_MODS(c) & KEY_MOD_SHIFT) && KEY_NO_MODS(c) == KEY_DOWN) {
        if (match_count > 0 && last_display_count > 0) {
            if (scroll_offset < last_max_scroll) {
                scroll_offset += last_display_count;
                if (scroll_offset > last_max_scroll) {
                    scroll_offset = last_max_scroll;
                }
                selected_idx = scroll_offset;
                if (selected_idx >= match_count) {
                    selected_idx = match_count - 1;
                }
            } else {
                term_beep(env->term);
            }
        } else {
            term_beep(env->term);
        }
        goto again;
    } else if ((KEY_MODS(c) & KEY_MOD_SHIFT) && KEY_NO_MODS(c) == KEY_UP) {
        if (match_count > 0 && last_display_count > 0) {
            if (scroll_offset > 0) {
                if (scroll_offset > last_display_count) {
                    scroll_offset -= last_display_count;
                } else {
                    scroll_offset = 0;
                }
                selected_idx = scroll_offset;
            } else {
                term_beep(env->term);
            }
        } else {
            term_beep(env->term);
        }
        goto again;
    } else if ((KEY_MODS(c) & KEY_MOD_ALT) && (KEY_NO_MODS(c) == 'c' || KEY_NO_MODS(c) == 'C')) {
        bool next_state = !ic_history_fuzzy_search_is_case_sensitive();
        ic_enable_history_fuzzy_case_sensitive(next_state);
        goto again;
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

static void edit_history_search_with_current_word(ic_env_t* env, editor_t* eb) {
    char* initial = NULL;
    const ssize_t input_len = sbuf_len(eb->input);
    if (input_len > 0) {
        initial = mem_strndup(eb->mem, sbuf_string(eb->input), input_len);
    }
    edit_history_fuzzy_search(env, eb, initial);
    mem_free(env->mem, initial);
}

/*
  editline_custom_menu.c

  This file is part of isocline

  MIT License

  Copyright (c) 2026 Caden Finley

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

//-------------------------------------------------------------
// Application-provided menu: this file is included in editline.c
//-------------------------------------------------------------

typedef struct custom_menu_match_s {
    ssize_t item_idx;
    int score;
    ssize_t match_pos;
    ssize_t match_len;
} custom_menu_match_t;

static int custom_menu_compare_matches(const void* left, const void* right) {
    const custom_menu_match_t* a = (const custom_menu_match_t*)left;
    const custom_menu_match_t* b = (const custom_menu_match_t*)right;
    if (a == NULL || b == NULL) {
        return 0;
    }
    if (b->score != a->score) {
        return (b->score - a->score);
    }
    if (a->item_idx < b->item_idx) {
        return -1;
    }
    if (a->item_idx > b->item_idx) {
        return 1;
    }
    return 0;
}

static ssize_t custom_menu_search(const ic_menu_item_t* items, ssize_t item_count,
                                  const char* query, bool case_sensitive,
                                  custom_menu_match_t* matches) {
    if (items == NULL || item_count <= 0 || matches == NULL) {
        return 0;
    }

    ssize_t count = 0;
    for (ssize_t i = 0; i < item_count; ++i) {
        const ic_menu_item_t* item = &items[i];
        command_palette_action_entry_t score_entry = {
            IC_KEY_ACTION_NONE,
            item->label,
            (item->description != NULL ? item->description : ""),
            (item->keywords != NULL ? item->keywords : ""),
        };
        int score = 0;
        ssize_t match_pos = -1;
        ssize_t match_len = 0;
        if (!command_palette_score_action(&score_entry, query, case_sensitive, &score, &match_pos,
                                          &match_len)) {
            continue;
        }
        matches[count].item_idx = i;
        matches[count].score = score;
        matches[count].match_pos = match_pos;
        matches[count].match_len = match_len;
        count++;
    }

    if (count > 1 && query != NULL && query[0] != '\0') {
        qsort(matches, (size_t)count, sizeof(matches[0]), custom_menu_compare_matches);
    }
    return count;
}

static void custom_menu_render_item(ic_env_t* env, editor_t* eb, stringbuf_t* display_buffer,
                                    const ic_menu_item_t* item, const custom_menu_match_t* match,
                                    bool is_filtered, bool is_selected) {
    if (env == NULL || eb == NULL || display_buffer == NULL || item == NULL || match == NULL) {
        return;
    }

    sbuf_clear(display_buffer);
    (void)sbuf_append(display_buffer, item->label);
    if (item->description != NULL && item->description[0] != '\0') {
        (void)sbuf_append(display_buffer, " - ");
        (void)sbuf_append(display_buffer, item->description);
    }

    const char* display = sbuf_string(display_buffer);
    if (display == NULL) {
        return;
    }
    const char* line_end = edit_menu_first_line_end(display);
    ssize_t entry_len = line_end ? (line_end - display) : ic_strlen(display);
    bool is_multiline = (line_end != NULL && (*line_end == '\n' || *line_end == '\r'));
    ssize_t max_columns = term_get_width(env->term) - 4;
    if (max_columns < 4) {
        max_columns = 4;
    }

    ssize_t visible_width = 0;
    ssize_t visible_len = edit_menu_visible_prefix(display, entry_len, max_columns, &visible_width);
    bool truncated = (visible_len < entry_len);
    bool append_ellipsis = (is_multiline || truncated);
    if (append_ellipsis && max_columns > 3 && visible_width + 3 > max_columns) {
        ssize_t adjusted_columns = max_columns - 3;
        if (adjusted_columns < 1) {
            adjusted_columns = 1;
        }
        visible_len =
            edit_menu_visible_prefix(display, entry_len, adjusted_columns, &visible_width);
    }

    if (is_selected) {
        (void)sbuf_append(eb->extra, "[ic-menu-selected]");
    }
    const char* arrow = (tty_is_utf8(env->tty) ? "\xE2\x86\x92" : ">");
    (void)sbuf_appendf(eb->extra, "[!pre]%s ", (is_selected ? arrow : " "));
    bool highlight_match = (is_filtered && match->match_len > 0 && match->match_pos >= 0);
    edit_menu_append_highlighted_prefix(eb->extra, display, visible_len, entry_len,
                                        match->match_pos, match->match_len, is_selected,
                                        highlight_match, NULL, false);
    if (append_ellipsis && max_columns > 3) {
        (void)sbuf_append(eb->extra, "...");
    }
    (void)sbuf_append(eb->extra, "[/pre]");
    if (is_selected) {
        (void)sbuf_append(eb->extra, "[/ic-menu-selected]");
    }
    (void)sbuf_append(eb->extra, "\n");
}

static bool edit_custom_menu(ic_env_t* env, editor_t* eb, const char* prompt_text,
                             const ic_menu_item_t* items, ssize_t item_count,
                             size_t* selected_index) {
    if (env == NULL || eb == NULL || env->tty == NULL || items == NULL || item_count <= 0 ||
        item_count > SSIZE_MAX / ssizeof(custom_menu_match_t)) {
        return false;
    }

    custom_menu_match_t* matches = mem_zalloc_tp_n(env->mem, custom_menu_match_t, item_count);
    stringbuf_t* display_buffer = sbuf_new(env->mem);
    if (matches == NULL || display_buffer == NULL) {
        mem_free(env->mem, matches);
        sbuf_free(display_buffer);
        return false;
    }

    edit_menu_session_t menu_session = edit_menu_begin(env, eb, prompt_text, true);
    sbuf_clear(eb->input);
    eb->pos = 0;

    ssize_t match_count = 0;
    ssize_t selected_idx = 0;
    ssize_t scroll_offset = 0;
    ssize_t last_display_count = 0;
    ssize_t last_max_scroll = 0;
    bool session_case_sensitive = false;
    bool accepted = false;

again:;

    const char* query = sbuf_string(eb->input);
    bool is_filtered = (query != NULL && query[0] != '\0');
    match_count = custom_menu_search(items, item_count, query, session_case_sensitive, matches);
    if (selected_idx >= match_count) {
        selected_idx = (match_count > 0 ? match_count - 1 : 0);
    }
    if (selected_idx < 0) {
        selected_idx = 0;
    }

    last_display_count = 0;
    last_max_scroll = 0;
    sbuf_clear(eb->extra);
    const char* mouse_suffix =
        (menu_session.mouse_scroll_enabled ? " | Mouse clicking is enabled" : "");

    if (match_count > 0) {
        if (is_filtered) {
            (void)sbuf_appendf(eb->extra, "[ic-info]%zd item%s found - case %s%s[/]\n", match_count,
                               match_count == 1 ? "" : "s",
                               session_case_sensitive ? "sensitive" : "insensitive", mouse_suffix);
        } else {
            (void)sbuf_appendf(eb->extra, "[ic-info]Items (%zd total) - case %s%s[/]\n", item_count,
                               session_case_sensitive ? "sensitive" : "insensitive", mouse_suffix);
        }

        ssize_t available_lines = edit_menu_available_lines(env, eb, 4, 3);
        edit_menu_window_t window =
            edit_menu_window_for(match_count, available_lines, selected_idx, scroll_offset);
        last_display_count = window.display_count;
        last_max_scroll = window.max_scroll;
        scroll_offset = window.scroll_offset;

        for (ssize_t i = 0; i < last_display_count; ++i) {
            ssize_t match_idx = scroll_offset + i;
            if (match_idx >= match_count) {
                break;
            }
            const custom_menu_match_t* match = &matches[match_idx];
            if (match->item_idx < 0 || match->item_idx >= item_count) {
                continue;
            }
            custom_menu_render_item(env, eb, display_buffer, &items[match->item_idx], match,
                                    is_filtered, match_idx == selected_idx);
        }
        edit_menu_append_scroll_hint(eb->extra, match_count, last_display_count, scroll_offset);
    } else {
        scroll_offset = 0;
        (void)sbuf_appendf(eb->extra, "[ic-info]No items found - case %s%s[/]\n",
                           session_case_sensitive ? "sensitive" : "insensitive", mouse_suffix);
    }

    if (!env->no_help) {
        (void)sbuf_append(eb->extra,
                          "[ic-diminish](↑↓/wheel:navigate shift+↑/↓:page enter/tab:select "
                          "alt+c:case esc:cancel)[/]");
    }
    edit_refresh(env, eb);

    code_t c = tty_read(env->tty);
    if (tty_term_resize_event(env->tty)) {
        (void)edit_resize(env, eb);
    }
    sbuf_clear(eb->extra);

    code_t key_no_mods = KEY_NO_MODS(c);
    if (menu_session.mouse_scroll_enabled && key_no_mods == KEY_EVENT_MOUSE_OTHER) {
        bool accept_selection = false;
        if (edit_menu_mouse_select_vertical(env, eb, match_count, scroll_offset, last_display_count,
                                            1, &selected_idx, &accept_selection)) {
            if (accept_selection) {
                c = KEY_ENTER;
                key_no_mods = KEY_ENTER;
            } else {
                goto again;
            }
        } else {
            goto again;
        }
    }

    if (c == KEY_ESC || c == KEY_BELL || c == KEY_CTRL_C) {
        goto done;
    }
    if (c == KEY_ENTER || c == KEY_TAB) {
        if (match_count <= 0 || selected_idx < 0 || selected_idx >= match_count) {
            term_beep(env->term);
            goto again;
        }
        ssize_t item_idx = matches[selected_idx].item_idx;
        if (item_idx < 0 || item_idx >= item_count) {
            term_beep(env->term);
            goto again;
        }
        if (selected_index != NULL) {
            *selected_index = to_size_t(item_idx);
        }
        accepted = true;
        goto done;
    }

    if ((KEY_MODS(c) & KEY_MOD_SHIFT) && key_no_mods == KEY_DOWN) {
        (void)edit_menu_page_down(env, match_count, last_display_count, last_max_scroll,
                                  &scroll_offset, &selected_idx);
    } else if ((KEY_MODS(c) & KEY_MOD_SHIFT) && key_no_mods == KEY_UP) {
        (void)edit_menu_page_up(env, match_count, last_display_count, &scroll_offset,
                                &selected_idx);
    } else if ((KEY_MODS(c) & KEY_MOD_ALT) && (key_no_mods == 'c' || key_no_mods == 'C')) {
        session_case_sensitive = !session_case_sensitive;
        selected_idx = 0;
        scroll_offset = 0;
    } else if (key_no_mods == KEY_UP || c == KEY_CTRL_P ||
               (menu_session.mouse_scroll_enabled && key_no_mods == KEY_EVENT_MOUSE_WHEEL_UP)) {
        (void)edit_menu_move_selection(env, match_count, -1, &selected_idx);
    } else if (key_no_mods == KEY_DOWN || c == KEY_CTRL_N ||
               (menu_session.mouse_scroll_enabled && key_no_mods == KEY_EVENT_MOUSE_WHEEL_DOWN)) {
        (void)edit_menu_move_selection(env, match_count, 1, &selected_idx);
    } else if (c == KEY_BACKSP) {
        if (eb->pos > 0) {
            edit_backspace(env, eb);
            selected_idx = 0;
            scroll_offset = 0;
        }
    } else if (c == KEY_DEL) {
        edit_delete_char(env, eb);
        selected_idx = 0;
        scroll_offset = 0;
    } else if (c == KEY_F1) {
        edit_show_help(env, eb);
    } else {
        char chr;
        unicode_t uchr;
        if (code_is_ascii_char(c, &chr)) {
            edit_insert_char(env, eb, chr);
            selected_idx = 0;
            scroll_offset = 0;
        } else if (code_is_unicode(c, &uchr)) {
            edit_insert_unicode(env, eb, uchr);
            selected_idx = 0;
            scroll_offset = 0;
        }
    }
    goto again;

done:
    sbuf_free(display_buffer);
    mem_free(env->mem, matches);
    edit_menu_finish(env, eb, &menu_session, true, true);
    return accepted;
}

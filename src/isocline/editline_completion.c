/* ----------------------------------------------------------------------------
  Copyright (c) 2021, Daan Leijen
  Largely Modified by Caden Finley 2025 for CJ's Shell
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License. A copy of the license can be
  found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/

//-------------------------------------------------------------

static bool edit_try_spell_correct(ic_env_t* env, editor_t* eb);
// Completion menu: this file is included in editline.c
//-------------------------------------------------------------

// return true if anything changed
static bool edit_complete(ic_env_t* env, editor_t* eb, ssize_t idx) {
    editor_start_modify(eb);
    ssize_t newpos = completions_apply(env->completions, idx, eb->input, eb->pos);
    if (newpos < 0) {
        editor_undo_restore(eb, false);
        return false;
    }
    eb->pos = newpos;
    edit_refresh(env, eb);
    return true;
}

static bool edit_complete_longest_prefix(ic_env_t* env, editor_t* eb) {
    editor_start_modify(eb);
    ssize_t newpos = completions_apply_longest_prefix(env->completions, eb->input, eb->pos);
    if (newpos < 0) {
        editor_undo_restore(eb, false);
        return false;
    }
    eb->pos = newpos;
    edit_refresh(env, eb);
    return true;
}

ic_private void sbuf_append_tagged(stringbuf_t* sb, const char* tag, const char* content) {
    sbuf_appendf(sb, "[%s]", tag);
    sbuf_append(sb, content);
    sbuf_append(sb, "[/]");
}

static void editor_append_completion(ic_env_t* env, editor_t* eb, ssize_t idx, ssize_t width,
                                     bool numbered, bool selected) {
    const char* help = NULL;
    const char* display = completions_get_display(env->completions, idx, &help);
    const char* source = completions_get_source(env->completions, idx);
    if (display == NULL)
        return;
    if (numbered) {
        sbuf_appendf(eb->extra, "[ic-info]%s%zd [/]",
                     (selected ? (tty_is_utf8(env->tty) ? "\xE2\x86\x92" : "*") : " "), 1 + idx);
        width -= 3;
    }

    if (width > 0) {
        sbuf_appendf(eb->extra, "[width=\"%zd;left; ;on\"]", width);
    }
    if (selected) {
        sbuf_append(eb->extra, "[ic-emphasis]");
    }
    sbuf_append(eb->extra, display);
    if (selected) {
        sbuf_append(eb->extra, "[/ic-emphasis]");
    }

    // Add source information if available
    if (source != NULL) {
        sbuf_append(eb->extra, " ");
        sbuf_append_tagged(eb->extra, "ic-info", "(");
        sbuf_append_tagged(eb->extra, "ic-info", source);
        sbuf_append_tagged(eb->extra, "ic-info", ")");
    }

    if (help != NULL) {
        sbuf_append(eb->extra, "  ");
        sbuf_append_tagged(eb->extra, "ic-info", help);
    }
    if (width > 0) {
        sbuf_append(eb->extra, "[/width]");
    }
}

// 2 and 3 column output up to 80 wide
#define IC_DISPLAY2_MAX 34
#define IC_DISPLAY2_COL (3 + IC_DISPLAY2_MAX)
#define IC_DISPLAY2_WIDTH (2 * IC_DISPLAY2_COL + 2)  // 75

#define IC_DISPLAY3_MAX 21
#define IC_DISPLAY3_COL (3 + IC_DISPLAY3_MAX)
#define IC_DISPLAY3_WIDTH (3 * IC_DISPLAY3_COL + 2 * 2)  // 76

static void editor_append_completion2(ic_env_t* env, editor_t* eb, ssize_t col_width, ssize_t idx1,
                                      ssize_t idx2, ssize_t selected) {
    editor_append_completion(env, eb, idx1, col_width, true, (idx1 == selected));
    sbuf_append(eb->extra, "  ");
    editor_append_completion(env, eb, idx2, col_width, true, (idx2 == selected));
}

static void editor_append_completion3(ic_env_t* env, editor_t* eb, ssize_t col_width, ssize_t idx1,
                                      ssize_t idx2, ssize_t idx3, ssize_t selected) {
    editor_append_completion(env, eb, idx1, col_width, true, (idx1 == selected));
    sbuf_append(eb->extra, "  ");
    editor_append_completion(env, eb, idx2, col_width, true, (idx2 == selected));
    sbuf_append(eb->extra, "  ");
    editor_append_completion(env, eb, idx3, col_width, true, (idx3 == selected));
}

static ssize_t edit_completions_max_width(ic_env_t* env, ssize_t count) {
    ssize_t max_width = 0;
    for (ssize_t i = 0; i < count; i++) {
        const char* help = NULL;
        const char* source = completions_get_source(env->completions, i);
        ssize_t w =
            bbcode_column_width(env->bbcode, completions_get_display(env->completions, i, &help));

        // Add space for source information if available
        if (source != NULL) {
            w += 3 + bbcode_column_width(env->bbcode,
                                         source);  // space + ( + source + )
        }

        if (help != NULL) {
            w += 2 + bbcode_column_width(env->bbcode, help);
        }
        if (w > max_width) {
            max_width = w;
        }
    }
    return max_width;
}

static void edit_completion_menu(ic_env_t* env, editor_t* eb, bool more_available) {
    ssize_t count = completions_count(env->completions);
    assert(count > 1);
    ssize_t selected = (env->complete_nopreview ? 0 : -1);
    bool expanded_mode = false;
    ssize_t scroll_offset = 0;
    ssize_t last_rows_visible = 0;
    ssize_t last_max_scroll_offset = 0;
    ssize_t count_displayed = count;
    code_t c = 0;

again:
    sbuf_clear(eb->extra);
    last_rows_visible = 0;
    last_max_scroll_offset = 0;

    if (count <= 0) {
        edit_refresh(env, eb);
        goto read_key;
    }

    count_displayed = (expanded_mode ? count : (count > 9 ? 9 : count));
    if (count_displayed <= 0) {
        count_displayed = count;
    }
    if (count_displayed <= 0) {
        count_displayed = 1;
    }
    if (selected >= count_displayed) {
        selected = (count_displayed > 0 ? count_displayed - 1 : -1);
    }

    ssize_t twidth = term_get_width(env->term) - 1;
    ssize_t colwidth = -1;
    ssize_t percolumn = 0;
    bool show_instructions = false;
    ssize_t visible_count = 0;

    if (!expanded_mode) {
        ssize_t max_display_width = edit_completions_max_width(env, count_displayed);
        if (count_displayed > 3 && ((colwidth = 3 + max_display_width) * 3 + 2 * 2) < twidth) {
            percolumn = (count_displayed + 2) / 3;
            for (ssize_t rw = 0; rw < percolumn; rw++) {
                if (rw > 0) {
                    sbuf_append(eb->extra, "\n");
                }
                editor_append_completion3(env, eb, colwidth, rw, percolumn + rw,
                                          (2 * percolumn) + rw, selected);
            }
        } else if (count_displayed > 4 && ((colwidth = 3 + max_display_width) * 2 + 2) < twidth) {
            percolumn = (count_displayed + 1) / 2;
            for (ssize_t rw = 0; rw < percolumn; rw++) {
                if (rw > 0) {
                    sbuf_append(eb->extra, "\n");
                }
                editor_append_completion2(env, eb, colwidth, rw, percolumn + rw, selected);
            }
        } else {
            for (ssize_t i = 0; i < count_displayed; i++) {
                if (i > 0) {
                    sbuf_append(eb->extra, "\n");
                }
                editor_append_completion(env, eb, i, -1, true, (selected == i));
            }
        }
        if (count > count_displayed) {
            sbuf_append(eb->extra,
                        "\n[ic-info](press PgDn or ctrl-j to expand the completion list)[/]");
        }
    } else {
        ssize_t max_display_width = edit_completions_max_width(env, count_displayed);
        colwidth = max_display_width + 6;  // extra space for numbering and padding

        // force a single-column layout in expanded mode for readability
        const ssize_t columns = 1;
        ssize_t total_rows = count_displayed;
        if (total_rows <= 0) {
            total_rows = 1;
        }

        ssize_t promptw = 0;
        ssize_t cpromptw = 0;
        edit_get_prompt_width(env, eb, false, &promptw, &cpromptw);

        ssize_t input_len = sbuf_len(eb->input);
        if (input_len < 0) {
            input_len = 0;
        }
        rowcol_t rc_dummy;
        memset(&rc_dummy, 0, sizeof(rc_dummy));
        ssize_t input_rows =
            sbuf_get_rc_at_pos(eb->input, eb->termw, promptw, cpromptw, input_len, &rc_dummy);
        if (input_rows <= 0) {
            input_rows = 1;
        }

        ssize_t term_height = term_get_height(env->term);
        ssize_t available_rows = term_height - input_rows;
        if (eb->prompt_prefix_lines > 0) {
            available_rows -= eb->prompt_prefix_lines;
        }
        if (available_rows < 3) {
            available_rows = 3;
        }

        ssize_t rows_for_items = available_rows - 1;
        if (rows_for_items < 1) {
            rows_for_items = 1;
        }

        bool need_scroll_hint = (total_rows > rows_for_items);
        bool need_more_hint = more_available;
        show_instructions = false;
        if ((need_scroll_hint || need_more_hint) && rows_for_items > 1) {
            rows_for_items--;
            show_instructions = true;
        }

        ssize_t rows_visible = (rows_for_items < total_rows ? rows_for_items : total_rows);
        if (rows_visible < 1) {
            rows_visible = 1;
        }

        ssize_t max_scroll_offset = (total_rows > rows_visible ? total_rows - rows_visible : 0);
        if (scroll_offset > max_scroll_offset) {
            scroll_offset = max_scroll_offset;
        }
        if (scroll_offset < 0) {
            scroll_offset = 0;
        }

        if (selected >= 0) {
            ssize_t selected_row = selected % total_rows;
            if (selected_row < scroll_offset) {
                scroll_offset = selected_row;
            } else if (selected_row >= scroll_offset + rows_visible) {
                scroll_offset = selected_row - rows_visible + 1;
            }
            if (scroll_offset < 0) {
                scroll_offset = 0;
            }
            if (scroll_offset > max_scroll_offset) {
                scroll_offset = max_scroll_offset;
            }
        }

        ssize_t row_start = scroll_offset;
        ssize_t row_end = row_start + rows_visible - 1;
        if (row_end >= total_rows) {
            row_end = total_rows - 1;
        }

        bool wrote_any_row = false;
        for (ssize_t row = row_start; row <= row_end; row++) {
            ssize_t idx = row;
            if (idx >= count_displayed) {
                continue;
            }
            if (wrote_any_row) {
                sbuf_append(eb->extra, "\n");
            }
            wrote_any_row = true;
            editor_append_completion(env, eb, idx, colwidth, true, (selected == idx));
            visible_count++;
        }

        if (visible_count <= 0 && count_displayed > 0) {
            editor_append_completion(env, eb, 0, -1, true, (selected == 0));
            visible_count = 1;
        }

        if (show_instructions) {
            if (sbuf_len(eb->extra) > 0) {
                sbuf_append(eb->extra, "\n");
            }
            if (more_available) {
                sbuf_append(eb->extra, "[ic-info]Press PgDn or ctrl-j to load more completions[/]");
            } else {
                sbuf_append(
                    eb->extra,
                    "[ic-info]Use up/down or tab/shift-tab to move; PgUp/PgDn to scroll[/]");
            }
        }

        ssize_t visible_start = 0;
        ssize_t visible_end = 0;
        if (visible_count > 0) {
            visible_start = row_start + 1;
            visible_end = row_start + visible_count;
            if (visible_end > count) {
                visible_end = count;
            }
        }

        char header[192];
        const char* hint_suffix = "";
        if (more_available && max_scroll_offset > 0) {
            hint_suffix = " (more available; PgUp/PgDn to scroll)";
        } else if (more_available) {
            hint_suffix = " (more available)";
        } else if (max_scroll_offset > 0) {
            hint_suffix = " (PgUp/PgDn to scroll)";
        }

        if (visible_start > 0 && visible_end >= visible_start) {
            snprintf(header, sizeof(header),
                     "[ic-info]Showing %zd-%zd of %zd completions%s[/]\n", visible_start,
                     visible_end, count, hint_suffix);
        } else {
            snprintf(header, sizeof(header),
                     "[ic-info]Showing %zd of %zd completions%s[/]\n",
                     (visible_count > 0 ? visible_count : count_displayed), count, hint_suffix);
        }
        sbuf_insert_at(eb->extra, header, 0);

        last_rows_visible = rows_visible;
        last_max_scroll_offset = max_scroll_offset;
    }

    if (!env->complete_nopreview && selected >= 0 && selected < count_displayed) {
        const char* saved_menu = sbuf_strdup(eb->extra);

        editor_start_modify(eb);
        ssize_t newpos = completions_apply(env->completions, selected, eb->input, eb->pos);
        if (newpos >= 0) {
            eb->pos = newpos;
        }

        if (saved_menu != NULL) {
            sbuf_replace(eb->extra, saved_menu);
            mem_free(eb->mem, saved_menu);
        }

        edit_refresh(env, eb);

        editor_undo_restore(eb, false);
    } else {
        edit_refresh(env, eb);
    }

read_key:
    c = tty_read(env->tty);
    if (tty_term_resize_event(env->tty)) {
        edit_resize(env, eb);
    }
    sbuf_clear(eb->extra);

    if (c >= '1' && c <= '9') {
        ssize_t i = (c - '1');
        if (i < count_displayed) {
            selected = i;
            c = KEY_ENTER;
        }
    }

    if (c == KEY_DOWN || c == KEY_TAB) {
        if (count_displayed > 0) {
            if (selected < 0) {
                selected = 0;
            } else {
                selected++;
                if (selected >= count_displayed) {
                    selected = 0;
                }
            }
        }
        goto again;
    } else if (c == KEY_UP || c == KEY_SHIFT_TAB) {
        if (count_displayed > 0) {
            if (selected < 0) {
                selected = count_displayed - 1;
            } else {
                selected--;
                if (selected < 0) {
                    selected = count_displayed - 1;
                }
            }
        }
        goto again;
    } else if (c == KEY_PAGEUP && expanded_mode) {
        c = 0;
        if (last_rows_visible > 0 && scroll_offset > 0) {
            ssize_t prev_offset = scroll_offset;
            if (scroll_offset > last_rows_visible) {
                scroll_offset -= last_rows_visible;
            } else {
                scroll_offset = 0;
            }
            if (scroll_offset == prev_offset) {
                term_beep(env->term);
            }
        } else {
            term_beep(env->term);
        }
        goto again;
    } else if (c == KEY_F1) {
        edit_show_help(env, eb);
        goto again;
    } else if (c == KEY_ESC) {
        completions_clear(env->completions);
        edit_refresh(env, eb);
        c = 0;
    } else if (selected >= 0 && (c == KEY_ENTER || c == KEY_RIGHT || c == KEY_END)) {
        assert(selected < count);
        c = 0;
        edit_complete(env, eb, selected);
        if (env->complete_autotab) {
            tty_code_pushback(env->tty, KEY_EVENT_AUTOTAB);
        }
    } else if (!env->complete_nopreview && !code_is_virt_key(c)) {
        assert(selected < count);
        edit_complete(env, eb, selected);
    } else if ((c == KEY_PAGEDOWN || c == KEY_LINEFEED) && count > 9) {
        c = 0;
        if (!expanded_mode || more_available) {
            if (more_available) {
                count = completions_generate(env, env->completions, sbuf_string(eb->input), eb->pos,
                                             IC_MAX_COMPLETIONS_TO_SHOW);
                completions_sort(env->completions);
                more_available = false;
                if (selected >= count) {
                    selected = (env->complete_nopreview ? 0 : -1);
                }
            }
            expanded_mode = true;
            scroll_offset = 0;
        } else if (expanded_mode && last_rows_visible > 0) {
            if (scroll_offset < last_max_scroll_offset) {
                scroll_offset += last_rows_visible;
                if (scroll_offset > last_max_scroll_offset) {
                    scroll_offset = last_max_scroll_offset;
                }
            } else {
                term_beep(env->term);
            }
        }
        goto again;
    } else {
        edit_refresh(env, eb);
    }

    completions_clear(env->completions);
    if (c != 0) {
        tty_code_pushback(env->tty, c);
    }
}

static void edit_generate_completions(ic_env_t* env, editor_t* eb, bool autotab) {
    debug_msg("edit: complete: %zd: %s\n", eb->pos, sbuf_string(eb->input));
    if (eb->pos < 0)
        return;
    ssize_t count = completions_generate(env, env->completions, sbuf_string(eb->input), eb->pos,
                                         IC_MAX_COMPLETIONS_TO_TRY);
    bool more_available = (count >= IC_MAX_COMPLETIONS_TO_TRY);
    if (count <= 0) {
        // no completions
        if (!autotab) {
            if (!edit_try_spell_correct(env, eb)) {
                term_beep(env->term);
            }
        }
    } else if (count == 1) {
        // complete if only one match
        if (edit_complete(env, eb, 0 /*idx*/) && env->complete_autotab) {
            tty_code_pushback(env->tty, KEY_EVENT_AUTOTAB);
        }
    } else {
        // term_beep(env->term);
        if (!more_available) {
            edit_complete_longest_prefix(env, eb);
        }
        completions_sort(env->completions);
        edit_completion_menu(env, eb, more_available);
    }
}

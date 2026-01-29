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

// Completion menu: this file is included in editline.c
//-------------------------------------------------------------

#define IC_SMALL_MENU_SOURCE_LIMIT 35
#define IC_LARGE_MENU_SOURCE_LIMIT 70

// return true if anything changed
static bool edit_complete(ic_env_t* env, editor_t* eb, ssize_t idx) {
    editor_start_modify(eb);
    ssize_t newpos = completions_apply(env->completions, idx, eb->input, eb->pos);
    if (newpos < 0) {
        editor_undo_restore(eb, false);
        return false;
    }
    eb->pos = newpos;
    edit_expand_abbreviation_if_needed(env, eb, true);
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
    edit_expand_abbreviation_if_needed(env, eb, true);
    edit_refresh(env, eb);
    return true;
}

ic_private void sbuf_append_tagged(stringbuf_t* sb, const char* tag, const char* content) {
    sbuf_appendf(sb, "[%s]", tag);
    sbuf_append(sb, content);
    sbuf_append(sb, "[/]");
}

static const char* completion_single_line_view(alloc_t* mem, const char* display,
                                               char** allocated) {
    if (allocated != NULL) {
        *allocated = NULL;
    }
    if (display == NULL) {
        return "";
    }
    const char* newline = strchr(display, '\n');
    if (newline == NULL) {
        return display;
    }
    size_t prefix_len = (size_t)(newline - display);
    size_t truncated_len = prefix_len + 3;  // account for "..."
    char* truncated = mem_malloc_tp_n(mem, char, (ssize_t)(truncated_len + 1));
    if (truncated == NULL) {
        return display;
    }
    if (prefix_len > 0) {
        memcpy(truncated, display, prefix_len);
    }
    memcpy(truncated + prefix_len, "...", 3);
    truncated[truncated_len] = '\0';
    if (allocated != NULL) {
        *allocated = truncated;
    }
    return truncated;
}

static const char* completion_source_view(alloc_t* mem, const char* source, ssize_t max_chars,
                                          bool allow_full_length, char** allocated) {
    if (allocated != NULL) {
        *allocated = NULL;
    }
    if (source == NULL) {
        return NULL;
    }
    if (max_chars <= 0 || allow_full_length) {
        return source;
    }
    ssize_t len = ic_strlen(source);
    if (len <= max_chars) {
        return source;
    }
    ssize_t ellipsis = (max_chars >= 3 ? 3 : 0);
    ssize_t copy_len = max_chars - ellipsis;
    if (copy_len < 0) {
        copy_len = 0;
    }
    ssize_t total = copy_len + ellipsis;
    char* truncated = mem_malloc_tp_n(mem, char, total + 1);
    if (truncated == NULL) {
        return source;
    }
    if (copy_len > 0) {
        memcpy(truncated, source, (size_t)copy_len);
    }
    if (ellipsis > 0) {
        memcpy(truncated + copy_len, "...", (size_t)ellipsis);
    }
    truncated[total] = '\0';
    if (allocated != NULL) {
        *allocated = truncated;
    }
    return truncated;
}

static void editor_append_completion(ic_env_t* env, editor_t* eb, ssize_t idx, ssize_t width,
                                     bool numbered, bool selected) {
    const char* help = NULL;
    const char* display = completions_get_display(env->completions, idx, &help);
    const char* source = completions_get_source(env->completions, idx);
    if (display == NULL)
        return;

    const char* arrow = (tty_is_utf8(env->tty) ? "\xE2\x86\x92" : ">");
    ssize_t width_remaining = width;

    if (numbered) {
        ssize_t shown = 1 + idx;
        ssize_t ndigits = 1;
        for (ssize_t t = shown; t >= 10; t /= 10) {
            ndigits++;
        }
        ssize_t prefix_width = 1 + ndigits + 1;
        width_remaining -= prefix_width;
        if (selected) {
            sbuf_append(eb->extra, "[ic-emphasis]");
        }
        sbuf_appendf(eb->extra, "%s%zd ", (selected ? arrow : " "), shown);
        if (selected) {
            sbuf_append(eb->extra, "[/ic-emphasis]");
        } else {
            sbuf_append(eb->extra, "[ic-info][/]");
        }
    } else {
        ssize_t prefix_width = 2;  // arrow + space (or two spaces when not selected)
        width_remaining -= prefix_width;
        if (selected) {
            sbuf_append(eb->extra, "[ic-emphasis]");
            sbuf_appendf(eb->extra, "%s ", arrow);
            sbuf_append(eb->extra, "[/ic-emphasis]");
        } else {
            sbuf_append(eb->extra, "  ");
        }
    }

    bool apply_width_constraint = (width_remaining > 0) && (numbered || !selected);
    if (apply_width_constraint) {
        sbuf_appendf(eb->extra, "[width=\"%zd;left; ;on\"]", width_remaining);
    }
    if (selected) {
        sbuf_append(eb->extra, "[ic-emphasis]");
    }
    char* single_line_alloc = NULL;
    const char* single_line_display =
        completion_single_line_view(env->mem, display, &single_line_alloc);
    sbuf_append(eb->extra, single_line_display);
    if (selected) {
        sbuf_append(eb->extra, "[/ic-emphasis]");
    }
    if (single_line_alloc != NULL) {
        mem_free(env->mem, single_line_alloc);
    }

    // Add source information if available
    const char* source_display = source;
    char* source_alloc = NULL;
    if (source != NULL) {
        ssize_t limit = (numbered ? IC_SMALL_MENU_SOURCE_LIMIT : IC_LARGE_MENU_SOURCE_LIMIT);
        bool allow_full_source = (!numbered && selected);
        source_display =
            completion_source_view(env->mem, source, limit, allow_full_source, &source_alloc);
    }
    if (source_display != NULL) {
        sbuf_append(eb->extra, " ");
        sbuf_append_tagged(eb->extra, "ic-source", "(");
        sbuf_append_tagged(eb->extra, "ic-source", source_display);
        sbuf_append_tagged(eb->extra, "ic-source", ")");
    }

    if (help != NULL) {
        sbuf_append(eb->extra, "  ");
        sbuf_append_tagged(eb->extra, "ic-info", help);
    }
    if (apply_width_constraint) {
        sbuf_append(eb->extra, "[/width]");
    }
    if (source_alloc != NULL) {
        mem_free(env->mem, source_alloc);
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

static ssize_t edit_completions_max_width(ic_env_t* env, ssize_t count, ssize_t source_limit) {
    ssize_t max_width = 0;
    for (ssize_t i = 0; i < count; i++) {
        const char* help = NULL;
        const char* source = completions_get_source(env->completions, i);
        const char* display = completions_get_display(env->completions, i, &help);
        char* display_alloc = NULL;
        const char* single_line_display =
            completion_single_line_view(env->mem, display, &display_alloc);
        ssize_t w = bbcode_column_width(env->bbcode, single_line_display);

        // Add space for source information if available
        if (source != NULL) {
            char* source_alloc = NULL;
            const char* limited_source =
                completion_source_view(env->mem, source, source_limit, false, &source_alloc);
            if (limited_source != NULL) {
                w += 3 + bbcode_column_width(env->bbcode, limited_source);
            }
            if (source_alloc != NULL) {
                mem_free(env->mem, source_alloc);
            }
        }

        if (help != NULL) {
            w += 2 + bbcode_column_width(env->bbcode, help);
        }
        if (w > max_width) {
            max_width = w;
        }
        if (display_alloc != NULL) {
            mem_free(env->mem, display_alloc);
        }
    }
    return max_width;
}

static void edit_completion_menu_update_hint(ic_env_t* env, editor_t* eb, bool allow_inline_hint) {
    if (env->no_hint)
        return;

    sbuf_clear(eb->hint);
    sbuf_clear(eb->hint_help);

    ssize_t hint_count = completions_count(env->completions);
    if (hint_count <= 0)
        return;

    const char* help = NULL;
    const char* hint = completions_get_hint(env->completions, 0, &help);
    if (hint == NULL || *hint == '\0')
        return;

    if (allow_inline_hint) {
        sbuf_replace(eb->hint, hint);
    }
    if (help != NULL) {
        editor_append_hint_help(eb, help);
    }
}

static bool edit_recompute_completion_list(ic_env_t* env, editor_t* eb, bool expanded_mode,
                                           ssize_t* count, bool* more_available, ssize_t* selected,
                                           ssize_t* scroll_offset, bool allow_inline_hint) {
    ssize_t limit = (expanded_mode ? IC_MAX_COMPLETIONS_TO_SHOW : IC_MAX_COMPLETIONS_TO_TRY);
    ssize_t new_count =
        completions_generate(env, env->completions, sbuf_string(eb->input), eb->pos, limit);
    bool new_more_available = (new_count >= limit);

    if (new_count <= 0) {
        completions_clear(env->completions);
        sbuf_clear(eb->hint);
        sbuf_clear(eb->hint_help);
        return false;
    }

    completions_sort(env->completions);
    *count = new_count;
    *more_available = new_more_available;

    if (*selected >= new_count) {
        *selected = (new_count > 0 ? new_count - 1 : -1);
    }
    if (env->complete_nopreview && *selected < 0 && new_count > 0) {
        *selected = 0;
    }

    if (scroll_offset != NULL) {
        *scroll_offset = 0;
    }

    edit_completion_menu_update_hint(env, eb, allow_inline_hint);

    return true;
}

static void edit_completion_menu(ic_env_t* env, editor_t* eb, bool more_available) {
    ssize_t count = completions_count(env->completions);
    if (count <= 0) {
        sbuf_clear(eb->extra);
        sbuf_clear(eb->hint);
        sbuf_clear(eb->hint_help);
        edit_refresh(env, eb);
        completions_clear(env->completions);
        return;
    }
    bool completion_applied = false;
    const bool hints_enabled = !env->no_hint;
    char* saved_input = NULL;
    char* saved_hint = NULL;
    char* saved_hint_help = NULL;
    ssize_t saved_pos = eb->pos;
    if (hints_enabled) {
        saved_input = sbuf_strdup(eb->input);
        if (sbuf_len(eb->hint) > 0) {
            saved_hint = sbuf_strdup(eb->hint);
        }
        if (sbuf_len(eb->hint_help) > 0) {
            saved_hint_help = sbuf_strdup(eb->hint_help);
        }
    }

    sbuf_clear(eb->hint);
    sbuf_clear(eb->hint_help);
    edit_completion_menu_update_hint(env, eb, false);
    ssize_t selected = (env->complete_nopreview ? 0 : -1);
    bool expanded_mode = false;
    ssize_t scroll_offset = 0;
    ssize_t last_rows_visible = 0;
    ssize_t last_max_scroll_offset = 0;
    ssize_t count_displayed = count;
    code_t c = 0;
    bool grid_layout_active = false;
    ssize_t grid_columns = 1;
    ssize_t grid_rows = 1;

again:
    sbuf_clear(eb->extra);
    last_rows_visible = 0;
    last_max_scroll_offset = 0;
    grid_layout_active = false;
    grid_columns = 1;
    grid_rows = 1;

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
        grid_rows = (count_displayed > 0 ? count_displayed : 1);
        ssize_t max_display_width =
            edit_completions_max_width(env, count_displayed, IC_SMALL_MENU_SOURCE_LIMIT);
        ssize_t max_col = (twidth > 2 ? twidth - 2 : max_display_width + 3);
        if (count_displayed > 3 && ((colwidth = 3 + max_display_width) * 3 + 2 * 2) < twidth) {
            if (colwidth > max_col)
                colwidth = max_col;
            percolumn = (count_displayed + 2) / 3;
            if (percolumn <= 0) {
                percolumn = 1;
            }
            grid_layout_active = true;
            grid_columns = 3;
            grid_rows = percolumn;
            for (ssize_t rw = 0; rw < percolumn; rw++) {
                if (rw > 0) {
                    sbuf_append(eb->extra, "\n");
                }
                editor_append_completion3(env, eb, colwidth, rw, percolumn + rw,
                                          (2 * percolumn) + rw, selected);
            }
        } else if (count_displayed > 4 && ((colwidth = 3 + max_display_width) * 2 + 2) < twidth) {
            percolumn = (count_displayed + 1) / 2;
            if (percolumn <= 0) {
                percolumn = 1;
            }
            grid_layout_active = true;
            grid_columns = 2;
            grid_rows = percolumn;
            for (ssize_t rw = 0; rw < percolumn; rw++) {
                if (rw > 0) {
                    sbuf_append(eb->extra, "\n");
                }
                editor_append_completion2(env, eb, colwidth, rw, percolumn + rw, selected);
            }
        } else {
            grid_layout_active = false;
            grid_columns = 1;
            grid_rows = (count_displayed > 0 ? count_displayed : 1);
            for (ssize_t i = 0; i < count_displayed; i++) {
                if (i > 0) {
                    sbuf_append(eb->extra, "\n");
                }
                editor_append_completion(env, eb, i, -1, true, (selected == i));
            }
        }
        if (count > count_displayed) {
            sbuf_append(eb->extra,
                        "\n[ic-info](press PgDn or ctrl-j to expand; ctrl-j again collapses)[/]");
        }
    } else {
        grid_layout_active = false;
        grid_columns = 1;
        grid_rows = (count_displayed > 0 ? count_displayed : 1);
        ssize_t max_display_width =
            edit_completions_max_width(env, count_displayed, IC_LARGE_MENU_SOURCE_LIMIT);
        colwidth = max_display_width + 6;  // extra space for prefix arrow and padding
        if (colwidth > twidth - 2) {
            colwidth = (twidth > 2 ? twidth - 2 : colwidth);
        }

        // force a single-column layout in expanded mode for readability
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
            editor_append_completion(env, eb, idx, colwidth, false, (selected == idx));
            visible_count++;
        }

        if (visible_count <= 0 && count_displayed > 0) {
            editor_append_completion(env, eb, 0, -1, false, (selected == 0));
            visible_count = 1;
        }

        if (show_instructions) {
            if (sbuf_len(eb->extra) > 0) {
                sbuf_append(eb->extra, "\n");
            }
            if (more_available) {
                sbuf_append(
                    eb->extra,
                    "[ic-info]Press PgDn to load more completions; ctrl-j collapses the list[/]");
            } else {
                sbuf_append(eb->extra,
                            "[ic-info]Use up/down or tab/shift-tab to move; Shift+Up/Down to page; "
                            "PgUp/PgDn to scroll[/]");
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
            snprintf(header, sizeof(header), "[ic-info]Showing %zd-%zd of %zd completions%s[/]\n",
                     visible_start, visible_end, count, hint_suffix);
        } else {
            snprintf(header, sizeof(header), "[ic-info]Showing %zd of %zd completions%s[/]\n",
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

    bool grid_mode = (!expanded_mode && grid_layout_active && grid_columns > 1 && grid_rows > 0);

    if (c >= '1' && c <= '9') {
        ssize_t i = (c - '1');
        ssize_t base = 0;
        ssize_t limit = count_displayed;
        if (expanded_mode) {
            base = scroll_offset;
            limit = (last_rows_visible > 0 ? last_rows_visible : count_displayed);
        }
        ssize_t idx = base + i;
        if (i < limit && idx < count_displayed) {
            selected = idx;
            c = KEY_ENTER;
        }
    }

    bool shift_pressed = ((KEY_MODS(c) & KEY_MOD_SHIFT) != 0);
    if (shift_pressed) {
        code_t base_key = KEY_NO_MODS(c);
        if (!expanded_mode && (base_key == KEY_DOWN || base_key == KEY_UP)) {
            if (count > count_displayed) {
                expanded_mode = true;
                scroll_offset = 0;
                goto again;
            }
        } else if (expanded_mode) {
            ssize_t page = (last_rows_visible > 0 ? last_rows_visible
                                                  : (count_displayed > 0 ? count_displayed : 1));
            if (page < 1) {
                page = 1;
            }
            if (base_key == KEY_DOWN) {
                if (scroll_offset < last_max_scroll_offset) {
                    scroll_offset += page;
                    if (scroll_offset > last_max_scroll_offset) {
                        scroll_offset = last_max_scroll_offset;
                    }
                    selected =
                        (scroll_offset < count_displayed ? scroll_offset : count_displayed - 1);
                    if (selected < 0) {
                        selected = 0;
                    }
                } else {
                    term_beep(env->term);
                }
                goto again;
            } else if (base_key == KEY_UP) {
                if (scroll_offset > 0) {
                    if (scroll_offset > page) {
                        scroll_offset -= page;
                    } else {
                        scroll_offset = 0;
                    }
                    selected = scroll_offset;
                    if (selected >= count_displayed) {
                        selected = count_displayed - 1;
                    }
                    if (selected < 0) {
                        selected = 0;
                    }
                } else {
                    term_beep(env->term);
                }
                goto again;
            }
        }
    }

    if ((c == KEY_RIGHT || c == KEY_LEFT) && grid_mode) {
        // Translate the linear selection index to grid coordinates for horizontal movement.
        if (count_displayed > 0) {
            if (selected < 0) {
                selected = (c == KEY_RIGHT ? 0 : count_displayed - 1);
            } else {
                ssize_t row = (grid_rows > 0 ? (selected % grid_rows) : 0);
                ssize_t col = (grid_rows > 0 ? (selected / grid_rows) : 0);
                ssize_t new_col = col;
                bool moved = false;
                for (ssize_t step = 0; step < grid_columns; step++) {
                    if (c == KEY_RIGHT) {
                        new_col = (new_col + 1) % grid_columns;
                    } else {
                        new_col = (new_col - 1 + grid_columns) % grid_columns;
                    }
                    ssize_t candidate = new_col * grid_rows + row;
                    if (candidate < count_displayed && candidate != selected) {
                        selected = candidate;
                        moved = true;
                        break;
                    }
                }
                if (!moved) {
                    term_beep(env->term);
                }
            }
        }
        goto again;
    }

    if (c == KEY_TAB && count_displayed == 1) {
        // With a single candidate, treat tab as immediate acceptance instead of cycling
        ssize_t accept_idx = selected;
        if (accept_idx < 0 || accept_idx >= count) {
            accept_idx = (count > 0 ? 0 : -1);
        }
        if (accept_idx >= 0) {
            bool applied_here = edit_complete(env, eb, accept_idx);
            if (applied_here) {
                completion_applied = true;
            }
            edit_refresh_hint(env, eb);
            if (applied_here && env->complete_autotab) {
                tty_code_pushback(env->tty, KEY_EVENT_AUTOTAB);
            }
        }
        c = 0;
        goto cleanup;
    } else if (c == KEY_DOWN || c == KEY_TAB) {
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
    } else if (selected >= 0 &&
               (c == KEY_ENTER || (!grid_mode && c == KEY_RIGHT) || c == KEY_END)) {
        assert(selected < count);
        c = 0;
        bool applied_here = edit_complete(env, eb, selected);
        if (applied_here) {
            completion_applied = true;
        }
        edit_refresh_hint(env, eb);
        if (applied_here && env->complete_autotab) {
            tty_code_pushback(env->tty, KEY_EVENT_AUTOTAB);
        }
    } else if (c == KEY_BACKSP) {
        edit_backspace(env, eb);
        if (!edit_recompute_completion_list(env, eb, expanded_mode, &count, &more_available,
                                            &selected, &scroll_offset, false)) {
            sbuf_clear(eb->extra);
            edit_refresh(env, eb);
            c = 0;
            goto cleanup;
        }
        goto again;
    } else if (c == KEY_DEL) {
        edit_delete_char(env, eb);
        if (!edit_recompute_completion_list(env, eb, expanded_mode, &count, &more_available,
                                            &selected, &scroll_offset, false)) {
            sbuf_clear(eb->extra);
            edit_refresh(env, eb);
            c = 0;
            goto cleanup;
        }
        goto again;
    } else if (!code_is_virt_key(c)) {
        bool inserted = false;
        char chr = 0;
        unicode_t uchr = 0;
        if (code_is_ascii_char(c, &chr)) {
            edit_insert_char(env, eb, chr);
            inserted = true;
        } else if (code_is_unicode(c, &uchr)) {
            edit_insert_unicode(env, eb, uchr);
            inserted = true;
        }
        if (inserted) {
            if (!edit_recompute_completion_list(env, eb, expanded_mode, &count, &more_available,
                                                &selected, &scroll_offset, false)) {
                sbuf_clear(eb->extra);
                edit_refresh(env, eb);
                c = 0;
                goto cleanup;
            }
            goto again;
        }
    } else if ((c == KEY_PAGEDOWN || c == KEY_LINEFEED) && count > 9) {
        bool triggered_by_ctrl_j = (c == KEY_LINEFEED);
        c = 0;
        if (!expanded_mode) {
            expanded_mode = true;
            scroll_offset = 0;
        } else if (triggered_by_ctrl_j) {
            expanded_mode = false;
            scroll_offset = 0;
            ssize_t collapsed_limit = (count > 9 ? 9 : count);
            if (collapsed_limit <= 0) {
                selected = -1;
            } else if (selected >= collapsed_limit) {
                selected = collapsed_limit - 1;
            }
        } else if (more_available) {
            ssize_t prev_count = count;
            count = completions_generate(env, env->completions, sbuf_string(eb->input), eb->pos,
                                         IC_MAX_COMPLETIONS_TO_SHOW);
            completions_sort(env->completions);
            more_available = (count >= IC_MAX_COMPLETIONS_TO_SHOW);
            if (selected >= count) {
                selected = (env->complete_nopreview ? 0 : -1);
            }
            if (count < prev_count && scroll_offset > 0 && scroll_offset >= count) {
                scroll_offset = (count > 0 ? count - 1 : 0);
            }
        } else if (expanded_mode && last_rows_visible > 0) {
            if (scroll_offset < last_max_scroll_offset) {
                scroll_offset += last_rows_visible;
                if (scroll_offset > last_max_scroll_offset) {
                    scroll_offset = last_max_scroll_offset;
                }
                if (scroll_offset < count_displayed) {
                    selected = scroll_offset;
                } else if (count_displayed > 0) {
                    selected = count_displayed - 1;
                }
            } else {
                term_beep(env->term);
            }
        }
        goto again;
    } else {
        edit_refresh(env, eb);
    }

cleanup:
    completions_clear(env->completions);
    if (!completion_applied && hints_enabled) {
        bool input_changed = true;
        if (saved_input != NULL) {
            const char* current_input = sbuf_string(eb->input);
            if (current_input != NULL) {
                if (strcmp(saved_input, current_input) == 0 && eb->pos == saved_pos) {
                    input_changed = false;
                }
            }
        }

        if (!input_changed) {
            sbuf_clear(eb->hint);
            if (saved_hint != NULL) {
                sbuf_replace(eb->hint, saved_hint);
            }
            sbuf_clear(eb->hint_help);
            if (saved_hint_help != NULL) {
                sbuf_replace(eb->hint_help, saved_hint_help);
            }
            edit_refresh(env, eb);
        } else {
            edit_refresh_hint(env, eb);
        }
    }

    if (saved_hint != NULL) {
        mem_free(eb->mem, saved_hint);
    }
    if (saved_hint_help != NULL) {
        mem_free(eb->mem, saved_hint_help);
    }
    if (saved_input != NULL) {
        mem_free(eb->mem, saved_input);
    }

    if (c != 0) {
        tty_code_pushback(env->tty, c);
    }
    return;
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

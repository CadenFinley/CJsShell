/*
  editline_completion.c

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

//-------------------------------------------------------------

// Completion menu: this file is included in editline.c
//-------------------------------------------------------------

#define IC_SMALL_MENU_SOURCE_LIMIT 35
#define IC_LARGE_MENU_SOURCE_LIMIT 70

static bool edit_completion_commit(editor_t* eb, ssize_t newpos) {
    if (newpos == IC_COMP_APPLY_FAIL) {
        editor_undo_restore(eb, false);
        return false;
    }
    if (newpos == IC_COMP_APPLY_NOOP) {
        editor_undo_forget(eb);
        return false;
    }
    eb->pos = newpos;
    return true;
}

// return true if anything changed
static bool edit_complete(ic_env_t* env, editor_t* eb, ssize_t idx) {
    editor_start_modify(eb);
    ssize_t newpos = completions_apply(env->completions, idx, eb->input, eb->pos);
    bool changed = edit_completion_commit(eb, newpos);

    if (changed) {
        edit_expand_abbreviation_if_needed(env, eb, true);
        edit_refresh(env, eb);
    } else if (newpos == IC_COMP_APPLY_NOOP && completions_count(env->completions) > 1) {
        edit_refresh(env, eb);
    }
    return changed;
}

static void edit_complete_longest_prefix(ic_env_t* env, editor_t* eb) {
    editor_start_modify(eb);
    ssize_t newpos = completions_apply_longest_prefix(env->completions, eb->input, eb->pos);
    if (!edit_completion_commit(eb, newpos)) {
        return;
    }
    edit_expand_abbreviation_if_needed(env, eb, true);
    edit_refresh(env, eb);
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
    const char* line_end = edit_menu_first_line_end(display);
    if (line_end == NULL || *line_end == '\0') {
        return display;
    }
    size_t prefix_len = (size_t)(line_end - display);
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
    const char* source_style = edit_menu_tag_style(selected);
    const char* help_style = (selected ? "ic-menu-selected-secondary" : "ic-info");

    if (selected) {
        sbuf_append(eb->extra, "[ic-menu-selected]");
    }

    if (numbered) {
        ssize_t shown = 1 + idx;
        ssize_t ndigits = 1;
        for (ssize_t t = shown; t >= 10; t /= 10) {
            ndigits++;
        }
        ssize_t prefix_width = 1 + ndigits + 1;
        width_remaining -= prefix_width;
        sbuf_appendf(eb->extra, "%s%zd ", (selected ? arrow : " "), shown);
    } else {
        ssize_t prefix_width = 2;
        width_remaining -= prefix_width;
        sbuf_appendf(eb->extra, "%s ", (selected ? arrow : " "));
    }

    bool display_has_line_break = edit_menu_contains_line_break(display);
    bool apply_width_constraint =
        (width_remaining > 0) && (numbered || !selected || display_has_line_break);
    if (apply_width_constraint) {
        sbuf_appendf(eb->extra, "[width=\"%zd;left; ;on\"]", width_remaining);
    }
    char* single_line_alloc = NULL;
    const char* single_line_display =
        completion_single_line_view(env->mem, display, &single_line_alloc);
    if (edit_menu_should_syntax_highlight_item(env, selected)) {
        if (!edit_menu_append_completion_syntax_highlighted_text(
                env, eb, eb->extra, env->completions, idx, single_line_display, -1, true, -1, 0,
                selected, false, NULL)) {
            edit_menu_append_syntax_highlighted_text(env, eb->extra, single_line_display, -1, true,
                                                     -1, 0, selected, false, NULL);
        }
    } else {
        sbuf_append(eb->extra, single_line_display);
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
        sbuf_append_tagged(eb->extra, source_style, "(");
        sbuf_append_tagged(eb->extra, source_style, source_display);
        sbuf_append_tagged(eb->extra, source_style, ")");
    }

    if (help != NULL) {
        sbuf_append(eb->extra, "  ");
        sbuf_append_tagged(eb->extra, help_style, help);
    }
    if (apply_width_constraint) {
        sbuf_append(eb->extra, "[/width]");
    }
    if (selected) {
        sbuf_append(eb->extra, "[/ic-menu-selected]");
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
    sbuf_clear(eb->hint);
    sbuf_clear(eb->hint_help);

    if (env->no_hint || edit_current_line_is_empty(eb))
        return;

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

static ssize_t edit_completion_available_rows_for_input(ic_env_t* env, editor_t* eb,
                                                        ssize_t input_rows) {
    ssize_t term_height = term_get_height(env->term);
    ssize_t available_rows = term_height - input_rows;
    if (eb->prompt_prefix_lines > 0) {
        available_rows -= eb->prompt_prefix_lines;
    }
    if (available_rows < 3) {
        available_rows = 3;
    }
    return available_rows;
}

static ssize_t edit_completion_preview_input_rows(ic_env_t* env, editor_t* eb, ssize_t selected) {
    ssize_t current_rows = edit_menu_input_rows(env, eb);
    if (env == NULL || eb == NULL || env->complete_nopreview || selected < 0 ||
        env->completions == NULL || eb->input == NULL) {
        return current_rows;
    }

    const char* input = sbuf_string(eb->input);
    if (input == NULL || eb->pos < 0) {
        return current_rows;
    }

    const char* replacement = NULL;
    ssize_t replacement_start = 0;
    ssize_t delete_after = 0;
    if (!completions_get_apply_range(env->completions, selected, input, eb->pos, &replacement,
                                     &replacement_start, &delete_after) ||
        replacement == NULL) {
        return current_rows;
    }

    const ssize_t input_len = ic_strlen(input);
    if (input_len < 0 || eb->pos > input_len) {
        return current_rows;
    }

    if (replacement_start < 0) {
        replacement_start = 0;
    }
    if (replacement_start > input_len) {
        replacement_start = input_len;
    }

    ssize_t suffix_start = eb->pos + delete_after;
    if (suffix_start < 0) {
        suffix_start = 0;
    }
    if (suffix_start > input_len) {
        suffix_start = input_len;
    }

    stringbuf_t* preview = sbuf_new(env->mem);
    if (preview == NULL) {
        return current_rows;
    }

    sbuf_append_n(preview, input, replacement_start);
    sbuf_append(preview, replacement);
    if (suffix_start < input_len) {
        sbuf_append_n(preview, input + suffix_start, input_len - suffix_start);
    }

    ssize_t promptw = 0;
    ssize_t cpromptw = 0;
    edit_get_prompt_width(env, eb, false, &promptw, &cpromptw);

    rowcol_t rc_dummy;
    memset(&rc_dummy, 0, sizeof(rc_dummy));
    ssize_t preview_rows =
        sbuf_get_rc_at_pos(preview, eb->termw, promptw, cpromptw, sbuf_len(preview), &rc_dummy);
    sbuf_free(preview);

    if (preview_rows <= 0) {
        preview_rows = 1;
    }
    return edit_visible_input_row_count(env, eb, preview_rows);
}

static ssize_t edit_completion_collapsed_max_rows(ic_env_t* env, editor_t* eb, ssize_t input_rows) {
    return edit_completion_available_rows_for_input(env, eb, input_rows);
}

static bool completion_menu_mouse_select(ic_env_t* env, editor_t* eb, bool expanded_mode,
                                         bool grid_mode, ssize_t grid_columns, ssize_t grid_rows,
                                         ssize_t colwidth, ssize_t scroll_offset,
                                         ssize_t count_displayed, ssize_t last_rows_visible,
                                         ssize_t* selected, bool* accept_selection) {
    if (env == NULL || eb == NULL || env->tty == NULL || selected == NULL ||
        accept_selection == NULL) {
        return false;
    }

    *accept_selection = false;

    tty_mouse_event_t mouse_event;
    if (!tty_get_last_mouse_event(env->tty, &mouse_event)) {
        return false;
    }

    if (mouse_event.action != TTY_MOUSE_ACTION_LEFT_PRESS &&
        mouse_event.action != TTY_MOUSE_ACTION_LEFT_RELEASE) {
        return false;
    }

    ssize_t target_row = 0;
    ssize_t target_col = 0;
    if (!edit_mouse_event_to_target_rowcol(env, eb, &mouse_event, &target_row, &target_col, NULL)) {
        return false;
    }

    const ssize_t input_rows = (eb->input_rows > 0 ? eb->input_rows : 1);
    const ssize_t items_first_row = input_rows + (expanded_mode ? 1 : 0);
    const ssize_t item_row = target_row - items_first_row;
    if (item_row < 0) {
        return false;
    }

    ssize_t idx = -1;
    if (expanded_mode) {
        ssize_t visible_rows = (last_rows_visible > 0 ? last_rows_visible : count_displayed);
        if (item_row >= visible_rows) {
            return false;
        }
        idx = scroll_offset + item_row;
    } else if (grid_mode) {
        if (grid_rows <= 0 || grid_columns <= 0 || colwidth <= 0 || item_row >= grid_rows) {
            return false;
        }

        const ssize_t slot_width = colwidth + 2;
        if (slot_width <= 0 || target_col < 0) {
            return false;
        }

        const ssize_t col = target_col / slot_width;
        const ssize_t col_offset = target_col % slot_width;
        if (col < 0 || col >= grid_columns || col_offset < 0 || col_offset >= colwidth) {
            return false;
        }

        idx = col * grid_rows + item_row;
    } else {
        if (item_row >= count_displayed) {
            return false;
        }
        idx = item_row;
    }

    if (idx < 0 || idx >= count_displayed) {
        return false;
    }

    *selected = idx;
    *accept_selection = (mouse_event.action == TTY_MOUSE_ACTION_LEFT_RELEASE);
    return true;
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
    bool menu_mouse_scroll_enabled = false;
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
    bool expanded_mode = env->complete_menu_start_expanded;
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

    bool want_mouse_scroll = expanded_mode;
    if (want_mouse_scroll && !menu_mouse_scroll_enabled) {
        menu_mouse_scroll_enabled = edit_enable_menu_mouse_scroll(env);
    } else if (!want_mouse_scroll && menu_mouse_scroll_enabled) {
        edit_disable_menu_mouse_scroll(env, true);
        menu_mouse_scroll_enabled = false;
    }

    ssize_t collapsed_max_rows = -1;
    const ssize_t collapsed_max_items = 12;
    const ssize_t rendered_input_rows = edit_completion_preview_input_rows(env, eb, selected);
    if (!expanded_mode) {
        collapsed_max_rows = edit_completion_collapsed_max_rows(env, eb, rendered_input_rows);
        if (collapsed_max_rows > 1 && count > collapsed_max_rows * 3) {
            collapsed_max_rows -= 1;
        }
        if (collapsed_max_rows < 1) {
            collapsed_max_rows = 1;
        }
    }

    count_displayed = (expanded_mode ? count : count);
    if (!expanded_mode && collapsed_max_rows > 0) {
        ssize_t max_items = collapsed_max_rows * 3;
        if (collapsed_max_items > 0 && max_items > collapsed_max_items) {
            max_items = collapsed_max_items;
        }
        if (count_displayed > max_items) {
            count_displayed = max_items;
        }
    }
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
            if (collapsed_max_rows > 0 && percolumn > collapsed_max_rows) {
                percolumn = collapsed_max_rows;
            }
            count_displayed = percolumn * 3;
            if (collapsed_max_items > 0 && count_displayed > collapsed_max_items) {
                count_displayed = collapsed_max_items;
            }
            if (count_displayed > count) {
                count_displayed = count;
            }
            if (count_displayed > 0) {
                percolumn = (count_displayed + 2) / 3;
                if (percolumn <= 0) {
                    percolumn = 1;
                }
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
            if (collapsed_max_rows > 0 && percolumn > collapsed_max_rows) {
                percolumn = collapsed_max_rows;
            }
            count_displayed = percolumn * 2;
            if (collapsed_max_items > 0 && count_displayed > collapsed_max_items) {
                count_displayed = collapsed_max_items;
            }
            if (count_displayed > count) {
                count_displayed = count;
            }
            if (count_displayed > 0) {
                percolumn = (count_displayed + 1) / 2;
                if (percolumn <= 0) {
                    percolumn = 1;
                }
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
            if (collapsed_max_rows > 0 && grid_rows > collapsed_max_rows) {
                grid_rows = collapsed_max_rows;
            }
            count_displayed = grid_rows;
            if (collapsed_max_items > 0 && count_displayed > collapsed_max_items) {
                count_displayed = collapsed_max_items;
                grid_rows = count_displayed;
            }
            for (ssize_t i = 0; i < count_displayed; i++) {
                if (i > 0) {
                    sbuf_append(eb->extra, "\n");
                }
                editor_append_completion(env, eb, i, -1, true, (selected == i));
            }
        }
        if (selected >= count_displayed) {
            selected = (count_displayed > 0 ? count_displayed - 1 : -1);
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

        ssize_t available_rows =
            edit_completion_available_rows_for_input(env, eb, rendered_input_rows);

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
                            "[ic-info]Use up/down, tab/shift-tab, or wheel to move; Shift+Up/Down "
                            "to page; PgUp/PgDn to scroll[/]");
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

        char header[384];
        const char* hint_suffix = "";
        if (more_available && max_scroll_offset > 0) {
            hint_suffix = " (more available; PgUp/PgDn or wheel to scroll)";
        } else if (more_available) {
            hint_suffix = " (more available)";
        } else if (max_scroll_offset > 0) {
            hint_suffix = " (PgUp/PgDn or wheel to scroll)";
        }

        const bool menu_mouse_click_enabled =
            (menu_mouse_scroll_enabled || eb->mouse_reporting_enabled);
        char mouse_suffix[EDIT_STATUS_HINT_BUFFER_LEN];
        mouse_suffix[0] = '\0';
        if (menu_mouse_click_enabled) {
            char mouse_status_text[EDIT_STATUS_HINT_BUFFER_LEN];
            const bool can_disable_mouse_here = (!expanded_mode && eb->mouse_reporting_enabled);
            edit_format_mouse_enabled_status_hint(env, can_disable_mouse_here, mouse_status_text,
                                                  sizeof(mouse_status_text));
            if (snprintf(mouse_suffix, sizeof(mouse_suffix), " (%s)", mouse_status_text) < 0) {
                mouse_suffix[0] = '\0';
            }
        }

        if (visible_start > 0 && visible_end >= visible_start) {
            snprintf(header, sizeof(header), "[ic-info]Showing %zd-%zd of %zd completions%s%s[/]\n",
                     visible_start, visible_end, count, hint_suffix, mouse_suffix);
        } else {
            snprintf(header, sizeof(header), "[ic-info]Showing %zd of %zd completions%s%s[/]\n",
                     (visible_count > 0 ? visible_count : count_displayed), count, hint_suffix,
                     mouse_suffix);
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
    code_t key_no_mods = KEY_NO_MODS(c);

    if (key_no_mods == KEY_EVENT_MOUSE_OTHER) {
        const bool click_selection_enabled =
            (menu_mouse_scroll_enabled || eb->mouse_reporting_enabled);
        if (click_selection_enabled) {
            bool accept_selection = false;
            if (completion_menu_mouse_select(env, eb, expanded_mode, grid_mode, grid_columns,
                                             grid_rows, colwidth, scroll_offset, count_displayed,
                                             last_rows_visible, &selected, &accept_selection)) {
                if (accept_selection && edit_completion_click_accept_enabled(env)) {
                    c = KEY_ENTER;
                    key_no_mods = KEY_ENTER;
                } else {
                    goto again;
                }
            } else {
                c = 0;
                goto again;
            }
        } else {
            c = 0;
            goto again;
        }
    }

    if (!expanded_mode &&
        (key_no_mods == KEY_EVENT_MOUSE_WHEEL_UP || key_no_mods == KEY_EVENT_MOUSE_WHEEL_DOWN)) {
        c = 0;
        goto again;
    }

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
        if (!expanded_mode && (key_no_mods == KEY_DOWN || key_no_mods == KEY_UP)) {
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
            if (key_no_mods == KEY_DOWN) {
                (void)edit_menu_page_down(env, count_displayed, page, last_max_scroll_offset,
                                          &scroll_offset, &selected);
                goto again;
            } else if (key_no_mods == KEY_UP) {
                (void)edit_menu_page_up(env, count_displayed, page, &scroll_offset, &selected);
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

    if (menu_mouse_scroll_enabled && expanded_mode &&
        (key_no_mods == KEY_EVENT_MOUSE_WHEEL_UP || key_no_mods == KEY_EVENT_MOUSE_WHEEL_DOWN)) {
        if (count_displayed > 0) {
            if (key_no_mods == KEY_EVENT_MOUSE_WHEEL_DOWN) {
                if (selected < 0) {
                    selected = 0;
                } else if (selected < count_displayed - 1) {
                    selected++;
                } else {
                    term_beep(env->term);
                }
            } else {
                if (selected < 0) {
                    selected = 0;
                } else if (selected > 0) {
                    selected--;
                } else {
                    term_beep(env->term);
                }
            }
        } else {
            term_beep(env->term);
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
    } else {
        if (edit_key_is_mouse_toggle_binding(env, c)) {
            if (edit_mouse_mode_supports_editing_capture(eb->mouse_reporting_mode)) {
                edit_disable_menu_mouse_scroll(env, menu_mouse_scroll_enabled);
                menu_mouse_scroll_enabled = false;
                edit_toggle_mouse_reporting(env, eb);
                if (expanded_mode) {
                    menu_mouse_scroll_enabled = edit_enable_menu_mouse_scroll(env);
                }
            }
            c = 0;
            goto again;
        }
    }

    if (c == KEY_F1) {
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
    } else if (!code_is_virt_key(c) || c == ' ') {
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
    } else if ((c == KEY_PAGEDOWN || c == KEY_LINEFEED) &&
               (expanded_mode || more_available || count > 9)) {
        bool triggered_by_ctrl_j = (c == KEY_LINEFEED);
        c = 0;
        if (!expanded_mode) {
            expanded_mode = true;
            scroll_offset = 0;
        } else if (triggered_by_ctrl_j) {
            expanded_mode = false;
            scroll_offset = 0;
            ssize_t collapsed_limit = count;
            if (collapsed_max_items > 0 && collapsed_limit > collapsed_max_items) {
                collapsed_limit = collapsed_max_items;
            }
            ssize_t max_rows = edit_completion_collapsed_max_rows(
                env, eb, edit_completion_preview_input_rows(env, eb, selected));
            if (max_rows > 1 && count > max_rows * 3) {
                max_rows -= 1;
            }
            if (max_rows > 0) {
                ssize_t max_items = max_rows * 3;
                if (collapsed_max_items > 0 && max_items > collapsed_max_items) {
                    max_items = collapsed_max_items;
                }
                if (collapsed_limit > max_items) {
                    collapsed_limit = max_items;
                }
            }
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
            (void)edit_menu_page_down(env, count_displayed, last_rows_visible,
                                      last_max_scroll_offset, &scroll_offset, &selected);
        }
        goto again;
    } else {
        edit_refresh(env, eb);
    }

cleanup:
    edit_disable_menu_mouse_scroll(env, menu_mouse_scroll_enabled);
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
    const char* first_source = (count > 0 ? completions_get_source(env->completions, 0) : NULL);
    if (first_source != NULL && strcmp(first_source, "spell") == 0) {
        bool current_word_spell = edit_completion_is_current_word_spell(env, eb, 0, NULL, NULL);
        if (!autotab && current_word_spell) {
            if (!edit_complete(env, eb, 0)) {
                term_beep(env->term);
            }
        } else if (!autotab && !current_word_spell) {
            term_beep(env->term);
        }
        completions_clear(env->completions);
        return;
    }
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

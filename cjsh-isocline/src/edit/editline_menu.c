/*
  editline_menu.c

  Shared helpers for editline menus. This file is included in editline.c.

  MIT License
*/

typedef struct edit_menu_session_s {
    const char* prompt_text;
    bool prompt_replacement;
    bool force_prompt_visibility;
    ssize_t line_number_column_width;
    bool old_hint;
    bool old_highlight;
    bool mouse_scroll_enabled;
} edit_menu_session_t;

typedef struct edit_menu_window_s {
    ssize_t display_count;
    ssize_t max_scroll;
    ssize_t scroll_offset;
} edit_menu_window_t;

static const char* edit_menu_tag_style(bool selected) {
    return selected ? "ic-menu-selected-secondary" : "ic-diminish";
}

static void edit_menu_append_tag_text(stringbuf_t* sb, bool selected, const char* text) {
    if (sb == NULL || text == NULL || text[0] == '\0') {
        return;
    }
    sbuf_appendf(sb, "[%s]", edit_menu_tag_style(selected));
    sbuf_append(sb, text);
    sbuf_append(sb, "[/]");
}

static bool edit_menu_should_syntax_highlight_item_ex(const ic_env_t* env, bool selected,
                                                      bool syntax_highlighting_enabled) {
    if (env == NULL || !syntax_highlighting_enabled || env->highlighter == NULL) {
        return false;
    }
    switch (env->menu_highlight_mode) {
        case IC_MENU_HIGHLIGHT_ALL:
            return true;
        case IC_MENU_HIGHLIGHT_SINGLE:
            return selected;
        case IC_MENU_HIGHLIGHT_REVERSE:
            return !selected;
        case IC_MENU_HIGHLIGHT_NONE:
        default:
            return false;
    }
}

static bool edit_menu_should_syntax_highlight_item(const ic_env_t* env, bool selected) {
    return edit_menu_should_syntax_highlight_item_ex(env, selected,
                                                     env != NULL && !env->no_highlight);
}

static bool edit_menu_color_is_rgb(ic_color_t color) {
    return color >= IC_RGB(0);
}

static int edit_menu_ansi_color_index(ic_color_t color) {
    if (color >= IC_ANSI_BLACK && color <= IC_ANSI_SILVER) {
        return (int)(color - IC_ANSI_BLACK);
    }
    if (color >= IC_ANSI_GRAY && color <= IC_ANSI_WHITE) {
        return 8 + (int)(color - IC_ANSI_GRAY);
    }
    if (color == IC_ANSI_DEFAULT) {
        return 256;
    }
    return -1;
}

static void edit_menu_append_tag_separator(stringbuf_t* sb, bool* first) {
    if (!*first) {
        sbuf_append_char(sb, ' ');
    }
    *first = false;
}

static bool edit_menu_append_color_property(stringbuf_t* sb, bool* first, const char* rgb_name,
                                            const char* ansi_name, ic_color_t color) {
    if (color == IC_COLOR_NONE) {
        return false;
    }

    int ansi_index = edit_menu_ansi_color_index(color);
    bool is_rgb = edit_menu_color_is_rgb(color);
    if (ansi_index < 0 && !is_rgb) {
        return false;
    }

    edit_menu_append_tag_separator(sb, first);
    if (ansi_index >= 0) {
        sbuf_appendf(sb, "%s=%d", ansi_name, ansi_index);
    } else {
        sbuf_appendf(sb, "%s=#%06x", rgb_name, (unsigned int)(color & 0xFFFFFFu));
    }
    return true;
}

static bool edit_menu_append_attr_open(stringbuf_t* sb, attr_t attr) {
    if (sb == NULL || attr_is_none(attr)) {
        return false;
    }

    bool first = true;
    ssize_t tag_start = sbuf_len(sb);
    sbuf_append_char(sb, '[');
    if (attr.x.bold == IC_ON) {
        edit_menu_append_tag_separator(sb, &first);
        sbuf_append(sb, "b");
    }
    if (attr.x.italic == IC_ON) {
        edit_menu_append_tag_separator(sb, &first);
        sbuf_append(sb, "i");
    }
    if (attr.x.underline == IC_ON) {
        edit_menu_append_tag_separator(sb, &first);
        sbuf_append(sb, "u");
    }
    if (attr.x.reverse == IC_ON) {
        edit_menu_append_tag_separator(sb, &first);
        sbuf_append(sb, "r");
    }
    (void)edit_menu_append_color_property(sb, &first, "color", "ansi-color", attr.x.color);
    (void)edit_menu_append_color_property(sb, &first, "bgcolor", "ansi-bgcolor", attr.x.bgcolor);
    (void)edit_menu_append_color_property(sb, &first, "underline-color",
                                          "ansi-underline-color", attr.x.underline_color);

    if (first) {
        sbuf_delete_from(sb, tag_start);
        return false;
    }
    sbuf_append_char(sb, ']');
    return true;
}

static void edit_menu_append_escaped_n(stringbuf_t* sb, const char* text, ssize_t len) {
    if (sb == NULL || text == NULL || len <= 0) {
        return;
    }

    ssize_t segment_start = 0;
    for (ssize_t i = 0; i < len; ++i) {
        if (text[i] != '[' && text[i] != '\\') {
            continue;
        }
        if (i > segment_start) {
            sbuf_append_n(sb, text + segment_start, i - segment_start);
        }
        sbuf_append_char(sb, '\\');
        sbuf_append_char(sb, text[i]);
        segment_start = i + 1;
    }
    if (segment_start < len) {
        sbuf_append_n(sb, text + segment_start, len - segment_start);
    }
}

static char* edit_menu_plain_text(alloc_t* mem, bbcode_t* bbcode, const char* text, ssize_t len,
                                  bool parse_bbcode) {
    if (mem == NULL || text == NULL) {
        return NULL;
    }
    if (len < 0) {
        len = ic_strlen(text);
    }
    if (!parse_bbcode) {
        return mem_strndup(mem, text, len);
    }

    char* text_copy = mem_strndup(mem, text, len);
    if (text_copy == NULL) {
        return NULL;
    }
    stringbuf_t* plain = sbuf_new(mem);
    attrbuf_t* ignored_attrs = attrbuf_new(mem);
    char* result = NULL;
    if (plain != NULL && ignored_attrs != NULL) {
        bbcode_append(bbcode, text_copy, plain, ignored_attrs);
        result = mem_strdup(mem, sbuf_string(plain));
    }
    attrbuf_free(ignored_attrs);
    sbuf_free(plain);
    mem_free(mem, text_copy);
    return result;
}

static bool edit_menu_is_line_break_at(const char* text, ssize_t len, ssize_t pos,
                                       ssize_t* advance) {
    if (advance != NULL) {
        *advance = 0;
    }
    if (text == NULL || pos < 0 || pos >= len) {
        return false;
    }
    if (text[pos] == '\r') {
        if (advance != NULL) {
            *advance = (pos + 1 < len && text[pos + 1] == '\n') ? 2 : 1;
        }
        return true;
    }
    if (text[pos] == '\n') {
        if (advance != NULL) {
            *advance = 1;
        }
        return true;
    }
    return false;
}

static void edit_menu_append_syntax_highlighted_plain(ic_env_t* env, stringbuf_t* sb,
                                                      const char* text, ssize_t len,
                                                      ssize_t match_pos, ssize_t match_len,
                                                      bool selected, bool highlight_match,
                                                      const char* newline_indent) {
    if (env == NULL || sb == NULL || text == NULL || len <= 0) {
        return;
    }

    attrbuf_t* attrs = attrbuf_new(env->mem);
    if (attrs == NULL) {
        edit_menu_append_escaped_n(sb, text, len);
        return;
    }
    highlight(env->mem, env->bbcode, text, attrs, env->highlighter, env->highlighter_arg);
    const attr_t* attr_data = attrbuf_attrs(attrs, len);
    if (attr_data == NULL) {
        attrbuf_free(attrs);
        edit_menu_append_escaped_n(sb, text, len);
        return;
    }

    ssize_t underline_start = -1;
    ssize_t underline_end = -1;
    if (highlight_match && match_len > 0 && match_pos >= 0 && match_pos < len) {
        underline_start = match_pos;
        underline_end = match_pos + match_len;
        if (underline_end > len) {
            underline_end = len;
        }
        if (underline_end <= underline_start) {
            underline_start = -1;
            underline_end = -1;
        }
    }

    bool underline_open = false;
    ssize_t pos = 0;
    while (pos < len) {
        ssize_t line_break_advance = 0;
        if (edit_menu_is_line_break_at(text, len, pos, &line_break_advance)) {
            if (underline_open) {
                sbuf_append(sb, "[/u]");
                underline_open = false;
            }
            sbuf_append_char(sb, '\n');
            if (newline_indent != NULL) {
                sbuf_append(sb, newline_indent);
            }
            pos += line_break_advance;
            continue;
        }

        bool should_underline =
            (underline_start >= 0 && pos >= underline_start && pos < underline_end);
        if (should_underline != underline_open) {
            if (underline_open) {
                sbuf_append(sb, "[/u]");
            } else if (selected) {
                sbuf_append(sb, "[u]");
            } else {
                sbuf_append(sb, "[u ic-emphasis]");
            }
            underline_open = should_underline;
        }

        attr_t attr = attr_data[pos];
        ssize_t run_end = pos + 1;
        while (run_end < len) {
            if (edit_menu_is_line_break_at(text, len, run_end, NULL)) {
                break;
            }
            bool run_underline =
                (underline_start >= 0 && run_end >= underline_start && run_end < underline_end);
            if (run_underline != should_underline || !attr_is_eq(attr, attr_data[run_end])) {
                break;
            }
            run_end++;
        }

        bool attr_open = edit_menu_append_attr_open(sb, attr);
        edit_menu_append_escaped_n(sb, text + pos, run_end - pos);
        if (attr_open) {
            sbuf_append(sb, "[/]");
        }
        pos = run_end;
    }

    if (underline_open) {
        sbuf_append(sb, "[/u]");
    }
    attrbuf_free(attrs);
}

static void edit_menu_append_syntax_highlighted_text(ic_env_t* env, stringbuf_t* sb,
                                                     const char* text, ssize_t len,
                                                     bool parse_bbcode, ssize_t match_pos,
                                                     ssize_t match_len, bool selected,
                                                     bool highlight_match,
                                                     const char* newline_indent) {
    if (env == NULL || sb == NULL || text == NULL) {
        return;
    }
    char* plain = edit_menu_plain_text(env->mem, env->bbcode, text, len, parse_bbcode);
    if (plain == NULL) {
        return;
    }
    edit_menu_append_syntax_highlighted_plain(env, sb, plain, ic_strlen(plain), match_pos,
                                              match_len, selected, highlight_match,
                                              newline_indent);
    mem_free(env->mem, plain);
}

static edit_menu_session_t edit_menu_begin(ic_env_t* env, editor_t* eb, const char* prompt_text,
                                           bool enable_mouse_scroll) {
    edit_menu_session_t session = {0};
    if (eb == NULL) {
        return session;
    }

    editor_undo_capture(eb);
    eb->disable_undo = true;
    session.old_hint = ic_enable_hint(false);
    session.old_highlight = ic_enable_highlight(true);
    ic_enable_highlight(session.old_highlight);
    session.prompt_text = eb->prompt_text;
    session.prompt_replacement = eb->replace_prompt_line_with_number;
    session.force_prompt_visibility = eb->force_prompt_text_visible;
    session.line_number_column_width = eb->line_number_column_width;
    eb->force_prompt_text_visible = true;
    eb->replace_prompt_line_with_number = false;
    eb->prompt_text = prompt_text;
    session.mouse_scroll_enabled =
        (enable_mouse_scroll ? edit_enable_menu_mouse_scroll(env) : false);
    return session;
}

static void edit_menu_finish(ic_env_t* env, editor_t* eb, edit_menu_session_t* session,
                             bool restore_undo, bool refresh) {
    if (eb == NULL || session == NULL) {
        return;
    }

    sbuf_clear(eb->extra);
    eb->disable_undo = false;
    if (restore_undo) {
        editor_undo_restore(eb, false);
    }
    eb->prompt_text = session->prompt_text;
    eb->replace_prompt_line_with_number = session->prompt_replacement;
    eb->force_prompt_text_visible = session->force_prompt_visibility;
    eb->line_number_column_width = session->line_number_column_width;
    ic_enable_hint(session->old_hint);
    ic_enable_highlight(session->old_highlight);
    edit_disable_menu_mouse_scroll(env, session->mouse_scroll_enabled);
    session->mouse_scroll_enabled = false;
    if (refresh) {
        edit_refresh(env, eb);
    }
}

static ssize_t edit_menu_input_rows(ic_env_t* env, editor_t* eb) {
    if (env == NULL || eb == NULL) {
        return 1;
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
    return input_rows;
}

static ssize_t edit_menu_available_lines(ic_env_t* env, editor_t* eb, ssize_t reserved_rows,
                                         ssize_t min_lines) {
    if (env == NULL || eb == NULL) {
        return min_lines;
    }

    ssize_t available_lines = term_get_height(env->term) - reserved_rows;
    if (eb->prompt_prefix_lines > 0) {
        available_lines -= eb->prompt_prefix_lines;
    }
    if (available_lines < min_lines) {
        available_lines = min_lines;
    }
    return available_lines;
}

static edit_menu_window_t edit_menu_window_for(ssize_t item_count, ssize_t requested_rows,
                                               ssize_t selected_idx, ssize_t scroll_offset) {
    edit_menu_window_t window = {0};
    if (requested_rows < 1) {
        requested_rows = 1;
    }

    window.display_count = (item_count > requested_rows ? requested_rows : item_count);
    if (window.display_count < 1) {
        window.display_count = 1;
    }

    window.max_scroll =
        (item_count > window.display_count ? (item_count - window.display_count) : 0);
    window.scroll_offset = scroll_offset;
    if (window.scroll_offset > window.max_scroll) {
        window.scroll_offset = window.max_scroll;
    }
    if (window.scroll_offset < 0) {
        window.scroll_offset = 0;
    }

    if (selected_idx < window.scroll_offset) {
        window.scroll_offset = selected_idx;
    } else if (selected_idx >= window.scroll_offset + window.display_count) {
        window.scroll_offset = selected_idx - window.display_count + 1;
    }

    if (window.scroll_offset < 0) {
        window.scroll_offset = 0;
    }
    if (window.scroll_offset > window.max_scroll) {
        window.scroll_offset = window.max_scroll;
    }
    return window;
}

static bool edit_menu_page_down(ic_env_t* env, ssize_t item_count, ssize_t page, ssize_t max_scroll,
                                ssize_t* scroll_offset, ssize_t* selected_idx) {
    if (scroll_offset == NULL || selected_idx == NULL || item_count <= 0 || page <= 0) {
        term_beep(env->term);
        return false;
    }
    if (*scroll_offset >= max_scroll) {
        term_beep(env->term);
        return false;
    }

    *scroll_offset += page;
    if (*scroll_offset > max_scroll) {
        *scroll_offset = max_scroll;
    }
    *selected_idx = *scroll_offset;
    if (*selected_idx >= item_count) {
        *selected_idx = item_count - 1;
    }
    if (*selected_idx < 0) {
        *selected_idx = 0;
    }
    return true;
}

static bool edit_menu_page_up(ic_env_t* env, ssize_t item_count, ssize_t page,
                              ssize_t* scroll_offset, ssize_t* selected_idx) {
    if (scroll_offset == NULL || selected_idx == NULL || item_count <= 0 || page <= 0) {
        term_beep(env->term);
        return false;
    }
    if (*scroll_offset <= 0) {
        term_beep(env->term);
        return false;
    }

    if (*scroll_offset > page) {
        *scroll_offset -= page;
    } else {
        *scroll_offset = 0;
    }
    *selected_idx = *scroll_offset;
    if (*selected_idx >= item_count) {
        *selected_idx = item_count - 1;
    }
    if (*selected_idx < 0) {
        *selected_idx = 0;
    }
    return true;
}

static bool edit_menu_move_selection(ic_env_t* env, ssize_t item_count, ssize_t delta,
                                     ssize_t* selected_idx) {
    if (selected_idx == NULL || item_count <= 0) {
        term_beep(env->term);
        return false;
    }

    ssize_t next = *selected_idx + delta;
    if (next < 0 || next >= item_count) {
        term_beep(env->term);
        return false;
    }
    *selected_idx = next;
    return true;
}

static const char* edit_menu_first_line_end(const char* str) {
    if (str == NULL) {
        return NULL;
    }
    const char* p = str;
    while (*p && *p != '\n' && *p != '\r') {
        p++;
    }
    return p;
}

static bool edit_menu_contains_line_break(const char* str) {
    if (str == NULL) {
        return false;
    }
    return (strchr(str, '\n') != NULL || strchr(str, '\r') != NULL);
}

static ssize_t edit_menu_line_count(const char* str) {
    if (str == NULL || *str == '\0') {
        return 1;
    }

    ssize_t lines = 1;
    for (const char* p = str; *p != '\0'; ++p) {
        if (*p == '\n') {
            lines++;
        } else if (*p == '\r') {
            lines++;
            if (p[1] == '\n') {
                ++p;
            }
        }
    }
    return lines;
}

static void edit_menu_append_multiline_preview(ic_env_t* env, editor_t* eb, const char* display,
                                               bool syntax_highlight, bool parse_bbcode) {
    if (env == NULL || eb == NULL || display == NULL) {
        return;
    }

    const char* arrow = (tty_is_utf8(env->tty) ? "\xE2\x86\x92" : ">");
    if (syntax_highlight) {
        sbuf_append(eb->extra, "[ic-menu-selected][!pre]");
        sbuf_appendf(eb->extra, "%s ", arrow);
        sbuf_append(eb->extra, "[/pre]");
        edit_menu_append_syntax_highlighted_text(env, eb->extra, display, -1, parse_bbcode, -1, 0,
                                                 true, false, "  ");
        sbuf_append(eb->extra, "[/ic-menu-selected]");
        return;
    }

    sbuf_append(eb->extra, "[ic-menu-selected][!pre]");
    sbuf_appendf(eb->extra, "%s ", arrow);

    const char* segment = display;
    for (const char* p = display; *p != '\0'; ++p) {
        if (*p != '\n' && *p != '\r') {
            continue;
        }

        if (p > segment) {
            sbuf_append_n(eb->extra, segment, p - segment);
        }

        sbuf_append(eb->extra, "\n  ");
        if (*p == '\r' && p[1] == '\n') {
            ++p;
        }
        segment = p + 1;
    }

    if (*segment != '\0') {
        sbuf_append(eb->extra, segment);
    }

    sbuf_append(eb->extra, "[/pre][/ic-menu-selected]");
}

static ssize_t edit_menu_visible_prefix(const char* s, ssize_t len, ssize_t max_columns,
                                        ssize_t* width_out) {
    if (s == NULL || len <= 0 || max_columns <= 0) {
        if (width_out != NULL) {
            *width_out = 0;
        }
        return 0;
    }

    ssize_t pos = 0;
    ssize_t width = 0;
    while (pos < len) {
        ssize_t cw = 0;
        ssize_t next = str_next_ofs(s, len, pos, &cw);
        if (next <= 0) {
            break;
        }

        if (cw <= 0) {
            pos += next;
            continue;
        }

        if (width + cw > max_columns) {
            break;
        }

        width += cw;
        pos += next;
    }

    if (width_out != NULL) {
        *width_out = width;
    }
    return pos;
}

static void edit_menu_append_highlighted_prefix(stringbuf_t* sb, const char* display,
                                                ssize_t visible_len, ssize_t entry_len,
                                                ssize_t match_pos, ssize_t match_len, bool selected,
                                                bool highlight, ic_env_t* env,
                                                bool syntax_highlight) {
    if (sb == NULL || display == NULL || visible_len <= 0) {
        return;
    }

    if (syntax_highlight && env != NULL) {
        edit_menu_append_syntax_highlighted_text(env, sb, display, visible_len, false, match_pos,
                                                 match_len, selected, highlight, NULL);
        return;
    }

    if (highlight && match_len > 0 && match_pos >= 0) {
        if (match_pos < 0) {
            match_pos = 0;
        }
        if (match_pos < visible_len) {
            if (match_pos > 0) {
                ssize_t prefix_len = (match_pos <= visible_len ? match_pos : visible_len);
                sbuf_append_n(sb, display, prefix_len);
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
                if (selected) {
                    sbuf_append(sb, "[/pre][u][!pre]");
                } else {
                    sbuf_append(sb, "[/pre][u ic-emphasis][!pre]");
                }
                sbuf_append_n(sb, display + match_pos, match_len);
                sbuf_append(sb, "[/pre][/u][!pre]");
            }

            ssize_t suffix_start = match_pos + match_len;
            if (suffix_start < visible_len) {
                sbuf_append_n(sb, display + suffix_start, visible_len - suffix_start);
            }
            return;
        }
    }

    sbuf_append_n(sb, display, visible_len);
}

static void edit_menu_append_scroll_hint(stringbuf_t* sb, ssize_t item_count, ssize_t display_count,
                                         ssize_t scroll_offset) {
    if (sb == NULL || item_count <= display_count) {
        return;
    }

    ssize_t hidden_above = scroll_offset;
    ssize_t hidden_below = item_count - (scroll_offset + display_count);
    if (hidden_above > 0 && hidden_below > 0) {
        sbuf_appendf(sb, "[ic-info]  (%zd above, %zd below)[/]\n", hidden_above, hidden_below);
    } else if (hidden_above > 0) {
        sbuf_appendf(sb, "[ic-info]  (%zd more above)[/]\n", hidden_above);
    } else if (hidden_below > 0) {
        sbuf_appendf(sb, "[ic-info]  (%zd more below)[/]\n", hidden_below);
    }
}

static bool edit_menu_mouse_select_vertical(ic_env_t* env, editor_t* eb, ssize_t item_count,
                                            ssize_t scroll_offset, ssize_t display_count,
                                            ssize_t status_rows, ssize_t* selected_idx,
                                            bool* accept_selection) {
    if (env == NULL || eb == NULL || env->tty == NULL || selected_idx == NULL ||
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

    if (item_count <= 0 || display_count <= 0) {
        return false;
    }

    ssize_t target_row = 0;
    ssize_t target_col = 0;
    if (!edit_mouse_event_to_target_rowcol(env, eb, &mouse_event, &target_row, &target_col, NULL)) {
        return false;
    }
    ic_unused(target_col);

    const ssize_t input_rows = edit_menu_input_rows(env, eb);
    const ssize_t items_first_row = input_rows + status_rows;
    const ssize_t item_row = target_row - items_first_row;
    if (item_row < 0 || item_row >= display_count) {
        return false;
    }

    const ssize_t idx = scroll_offset + item_row;
    if (idx < 0 || idx >= item_count) {
        return false;
    }

    *selected_idx = idx;
    *accept_selection = (mouse_event.action == TTY_MOUSE_ACTION_LEFT_RELEASE);
    return true;
}

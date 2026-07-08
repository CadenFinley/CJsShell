/*
  editline_history.c

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

static bool history_search_extract_preview_key(const char* query, char* key_buf,
                                               size_t key_buf_size) {
    if (key_buf == NULL || key_buf_size == 0) {
        return false;
    }
    key_buf[0] = '\0';
    if (query == NULL || query[0] == '\0') {
        return false;
    }

    bool found = false;
    const char* cursor = query;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (*cursor == '\0')
            break;

        const char* token_start = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            cursor++;
        }

        size_t token_len = (size_t)(cursor - token_start);
        if (token_len < 3)
            continue;

        for (size_t i = 1; i + 1 < token_len; ++i) {
            if (token_start[i] == ':' && token_start[i + 1] == ':' && i + 2 == token_len) {
                size_t key_len = i;
                if (key_len >= key_buf_size) {
                    key_len = key_buf_size - 1;
                }
                ic_memcpy(key_buf, token_start, (ssize_t)key_len);
                key_buf[key_len] = '\0';
                found = (key_len > 0);
            }
        }
    }

    return found;
}

static bool history_search_has_valid_metadata_tag(const char* query) {
    if (query == NULL || query[0] == '\0') {
        return false;
    }

    const char* cursor = query;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (*cursor == '\0')
            break;

        const char* token_start = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            cursor++;
        }

        size_t token_len = (size_t)(cursor - token_start);
        if (token_len < 3)
            continue;

        for (size_t i = 1; i + 1 < token_len; ++i) {
            if (token_start[i] == ':' && token_start[i + 1] == ':') {
                bool valid_key = true;
                for (size_t j = 0; j < i; ++j) {
                    char c = token_start[j];
                    if (c == ' ' || c == '\t' || c == '=') {
                        valid_key = false;
                        break;
                    }
                }
                if (valid_key) {
                    return true;
                }
            }
        }
    }

    return false;
}

static const char* k_history_search_timestamp_key = "timestamp";
static const char* k_history_search_exit_code_key = "code";
static const char* k_history_search_exit_code_legacy_key = "exit_code";

static const char* history_search_entry_exit_code(const history_entry_t* entry) {
    if (entry == NULL) {
        return NULL;
    }

    const char* exit_code = history_entry_get_metadata(entry, k_history_search_exit_code_key);
    if (exit_code == NULL || exit_code[0] == '\0') {
        exit_code = history_entry_get_metadata(entry, k_history_search_exit_code_legacy_key);
    }

    if (exit_code == NULL || exit_code[0] == '\0') {
        return NULL;
    }
    return exit_code;
}

static bool history_search_format_relative_time(time_t timestamp, char* formatted_buf,
                                                size_t formatted_buf_size) {
    if (formatted_buf == NULL || formatted_buf_size == 0) {
        return false;
    }

    time_t now = time(NULL);
    if (now == (time_t)-1) {
        return false;
    }

    long long now_ll = (long long)now;
    long long ts_ll = (long long)timestamp;
    bool in_future = (ts_ll > now_ll);
    unsigned long long delta_seconds =
        (unsigned long long)(in_future ? (ts_ll - now_ll) : (now_ll - ts_ll));

    if (delta_seconds < 5ULL) {
        int n = snprintf(formatted_buf, formatted_buf_size, "%s",
                         in_future ? "in a moment" : "just now");
        return (n > 0 && (size_t)n < formatted_buf_size);
    }

    unsigned long long amount = delta_seconds;
    const char* unit = "s";

    if (delta_seconds >= 31557600ULL) {
        amount = delta_seconds / 31557600ULL;
        unit = "y";
    } else if (delta_seconds >= 2629800ULL) {
        amount = delta_seconds / 2629800ULL;
        unit = "mo";
    } else if (delta_seconds >= 604800ULL) {
        amount = delta_seconds / 604800ULL;
        unit = "w";
    } else if (delta_seconds >= 86400ULL) {
        amount = delta_seconds / 86400ULL;
        unit = "d";
    } else if (delta_seconds >= 3600ULL) {
        amount = delta_seconds / 3600ULL;
        unit = "h";
    } else if (delta_seconds >= 60ULL) {
        amount = delta_seconds / 60ULL;
        unit = "m";
    }

    int n = snprintf(formatted_buf, formatted_buf_size, in_future ? "in %llu%s" : "%llu%s ago",
                     amount, unit);
    return (n > 0 && (size_t)n < formatted_buf_size);
}

static const char* history_search_pretty_metadata_value(const char* key, const char* value,
                                                        char* formatted_buf,
                                                        size_t formatted_buf_size) {
    if (value == NULL || value[0] == '\0') {
        return "-";
    }

    if (key == NULL || ic_stricmp(key, k_history_search_timestamp_key) != 0) {
        return value;
    }

    char* end = NULL;
    long long epoch = strtoll(value, &end, 10);
    if (end == value || end == NULL || *end != '\0' || epoch < 0) {
        return value;
    }

    // Only treat values in a plausible unix-seconds range as timestamps.
    if (epoch < 946684800LL || epoch > 4102444800LL) {
        return value;
    }

    time_t ts = (time_t)epoch;
    if ((long long)ts != epoch) {
        return value;
    }

    if (!history_search_format_relative_time(ts, formatted_buf, formatted_buf_size)) {
        return value;
    }

    return formatted_buf;
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

static bool history_is_word_separator(char ch) {
    switch (ch) {
        case '|':
        case '&':
        case ';':
        case '(':
        case ')':
        case '<':
        case '>':
            return true;
        default:
            return (isspace((unsigned char)ch) != 0);
    }
}

static bool history_find_last_word_bounds(const char* command, ssize_t* start_out,
                                          ssize_t* end_out) {
    if (command == NULL || start_out == NULL || end_out == NULL) {
        return false;
    }

    const ssize_t len = ic_strlen(command);
    ssize_t pos = 0;
    ssize_t last_start = -1;
    ssize_t last_end = -1;

    // Track raw token ranges so quoted history arguments are reinserted verbatim.
    while (pos < len) {
        while (pos < len && history_is_word_separator(command[pos])) {
            pos++;
        }
        if (pos >= len) {
            break;
        }

        ssize_t token_start = pos;
        char quote = 0;
        while (pos < len) {
            char ch = command[pos];
            if (quote != 0) {
                if (quote == '"' && ch == '\\' && pos + 1 < len) {
                    pos += 2;
                    continue;
                }
                pos++;
                if (ch == quote) {
                    quote = 0;
                }
                continue;
            }
            if (ch == '\\') {
                pos += (pos + 1 < len ? 2 : 1);
                continue;
            }
            if (ch == '\'' || ch == '"') {
                quote = ch;
                pos++;
                continue;
            }
            if (history_is_word_separator(ch)) {
                break;
            }
            pos++;
        }

        if (pos > token_start) {
            last_start = token_start;
            last_end = pos;
        }
    }

    if (last_start < 0 || last_end <= last_start) {
        return false;
    }

    *start_out = last_start;
    *end_out = last_end;
    return true;
}

static char* history_dup_last_word(alloc_t* mem, const char* command) {
    ssize_t start = 0;
    ssize_t end = 0;
    if (!history_find_last_word_bounds(command, &start, &end)) {
        return NULL;
    }
    return mem_strndup(mem, command + start, end - start);
}

static void edit_yank_last_arg(ic_env_t* env, editor_t* eb) {
    if (env == NULL || eb == NULL || env->history == NULL || eb->input == NULL) {
        return;
    }

    history_snapshot_t snap = {0};
    if (!history_snapshot_load(env->history, &snap, false)) {
        term_beep(env->term);
        return;
    }

    ssize_t start_history_idx = 1;
    if (eb->last_arg_yank_active) {
        start_history_idx = eb->last_arg_yank_history_idx + 1;
    } else if (!eb->modified && eb->history_idx > 0) {
        start_history_idx = eb->history_idx + 1;
    }

    char* last_word = NULL;
    ssize_t found_history_idx = -1;
    const ssize_t total_history = history_snapshot_count(&snap);
    for (ssize_t idx = start_history_idx; idx < total_history; ++idx) {
        const history_entry_t* entry = history_snapshot_get(&snap, idx);
        if (entry == NULL || entry->command == NULL || entry->command[0] == '\0') {
            continue;
        }
        last_word = history_dup_last_word(env->mem, entry->command);
        if (last_word != NULL && last_word[0] != '\0') {
            found_history_idx = idx;
            break;
        }
        mem_free(env->mem, last_word);
        last_word = NULL;
    }
    history_snapshot_free(env->history, &snap);

    if (last_word == NULL || found_history_idx < 0) {
        mem_free(env->mem, last_word);
        term_beep(env->term);
        return;
    }

    ssize_t replace_start = eb->pos;
    ssize_t replace_end = eb->pos;
    if (eb->last_arg_yank_active) {
        replace_start = eb->last_arg_yank_start;
        replace_end = eb->last_arg_yank_end;
    }

    ssize_t input_len = sbuf_len(eb->input);
    if (replace_start < 0) {
        replace_start = 0;
    }
    if (replace_start > input_len) {
        replace_start = input_len;
    }
    if (replace_end < replace_start) {
        replace_end = replace_start;
    }
    if (replace_end > input_len) {
        replace_end = input_len;
    }

    editor_start_modify_preserve_last_arg(eb);
    const ssize_t replaced_len = replace_end - replace_start;
    ssize_t inserted_end = sbuf_insert_at(eb->input, last_word, replace_start);
    if (last_word[0] != '\0' && inserted_end == replace_start) {
        mem_free(env->mem, last_word);
        term_beep(env->term);
        return;
    }
    if (replaced_len > 0) {
        sbuf_delete_from_to(eb->input, inserted_end, inserted_end + replaced_len);
    }

    eb->pos = inserted_end;
    eb->last_arg_yank_active = true;
    eb->last_arg_yank_history_idx = found_history_idx;
    eb->last_arg_yank_start = replace_start;
    eb->last_arg_yank_end = inserted_end;

    mem_free(env->mem, last_word);
    edit_refresh_hint(env, eb);
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

static ssize_t history_menu_input_rows(ic_env_t* env, editor_t* eb) {
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

static bool history_menu_mouse_select(ic_env_t* env, editor_t* eb, ssize_t match_count,
                                      ssize_t scroll_offset, ssize_t display_count,
                                      ssize_t* selected_idx, bool* accept_selection) {
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

    if (match_count <= 0 || display_count <= 0) {
        return false;
    }

    ssize_t target_row = 0;
    ssize_t target_col = 0;
    if (!edit_mouse_event_to_target_rowcol(env, eb, &mouse_event, &target_row, &target_col, NULL)) {
        return false;
    }
    ic_unused(target_col);

    const ssize_t input_rows = history_menu_input_rows(env, eb);
    const ssize_t items_first_row = input_rows + 1;  // one status row before results
    const ssize_t item_row = target_row - items_first_row;
    if (item_row < 0 || item_row >= display_count) {
        return false;
    }

    const ssize_t idx = scroll_offset + item_row;
    if (idx < 0 || idx >= match_count) {
        return false;
    }

    *selected_idx = idx;
    *accept_selection = (mouse_event.action == TTY_MOUSE_ACTION_LEFT_RELEASE);
    return true;
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
    bool old_highlight = ic_enable_highlight(true);
    ic_enable_highlight(old_highlight);
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
        ic_enable_highlight(old_highlight);
        return;
    }

    ssize_t match_count = 0;
    ssize_t selected_idx = 0;
    ssize_t scroll_offset = 0;
    ssize_t last_display_count = 0;
    ssize_t last_max_scroll = 0;
    bool session_case_sensitive = ic_history_fuzzy_search_is_case_sensitive();

    if (initial != NULL) {
        sbuf_replace(eb->input, initial);
        eb->pos = ic_strlen(initial);
    } else {
        sbuf_clear(eb->input);
        eb->pos = 0;
    }

    bool menu_mouse_scroll_enabled = edit_enable_menu_mouse_scroll(env);

again:;

    last_display_count = 0;
    last_max_scroll = 0;

    bool showing_all_due_to_no_matches = false;
    bool metadata_filter_applied = false;
    char metadata_preview_key[64];
    metadata_preview_key[0] = '\0';
    const char* metadata_suffix_key = k_history_search_timestamp_key;
    bool metadata_suffix_use_default_tag = true;

    {
        const char* query = sbuf_string(eb->input);
        bool suppress_highlight = history_search_has_valid_metadata_tag(query);
        (void)ic_enable_highlight(suppress_highlight ? false : old_highlight);
        (void)history_search_extract_preview_key(query, metadata_preview_key,
                                                 sizeof(metadata_preview_key));

        if (metadata_preview_key[0] != '\0') {
            metadata_suffix_key = metadata_preview_key;
            metadata_suffix_use_default_tag = false;
        } else {
            metadata_suffix_key = k_history_search_timestamp_key;
            metadata_suffix_use_default_tag = true;
        }

        history_fuzzy_search_with_case(env->history, query ? query : "", matches, MAX_FUZZY_RESULTS,
                                       &match_count, &metadata_filter_applied,
                                       session_case_sensitive);

        if (match_count == 0 && query != NULL && query[0] != '\0' && !metadata_filter_applied) {
            history_fuzzy_search_with_case(env->history, "", matches, MAX_FUZZY_RESULTS,
                                           &match_count, NULL, session_case_sensitive);
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
    const char* mouse_suffix = (menu_mouse_scroll_enabled ? " | Mouse clicking is enabled" : "");

    if (match_count > 0) {
        const char* query = sbuf_string(eb->input);
        bool is_filtered = (query != NULL && query[0] != '\0');
        ssize_t total_history = history_snapshot_count(&snap);

        if (showing_all_due_to_no_matches) {
            sbuf_appendf(eb->extra,
                         "[ic-info]No matches - showing all history (%zd entr%s) - case %s%s[/]\n",
                         total_history, total_history == 1 ? "y" : "ies",
                         session_case_sensitive ? "sensitive" : "insensitive", mouse_suffix);
        } else if (is_filtered) {
            if (metadata_filter_applied) {
                sbuf_appendf(eb->extra,
                             "[ic-info]%zd match%s found (metadata filter) - case %s%s[/]\n",
                             match_count, match_count == 1 ? "" : "es",
                             session_case_sensitive ? "sensitive" : "insensitive", mouse_suffix);
            } else {
                sbuf_appendf(eb->extra, "[ic-info]%zd match%s found - case %s%s[/]\n", match_count,
                             match_count == 1 ? "" : "es",
                             session_case_sensitive ? "sensitive" : "insensitive", mouse_suffix);
            }
        } else {
            sbuf_appendf(eb->extra, "[ic-info]History (%zd entr%s) - case %s%s[/]\n", total_history,
                         total_history == 1 ? "y" : "ies",
                         session_case_sensitive ? "sensitive" : "insensitive", mouse_suffix);
        }

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

            char metadata_suffix[160];
            metadata_suffix[0] = '\0';
            ssize_t metadata_reserved_columns = 0;
            if (metadata_suffix_use_default_tag) {
                const char* exit_code = history_search_entry_exit_code(entry);
                char formatted_meta_value[64];
                const char* shown_value = history_search_pretty_metadata_value(
                    k_history_search_timestamp_key,
                    history_entry_get_metadata(entry, k_history_search_timestamp_key),
                    formatted_meta_value, sizeof(formatted_meta_value));

                if (exit_code != NULL) {
                    int suffix_written = snprintf(metadata_suffix, sizeof(metadata_suffix),
                                                  " [%s | %s]", shown_value, exit_code);
                    if (suffix_written > 0) {
                        if (suffix_written >= (int)sizeof(metadata_suffix)) {
                            suffix_written = (int)sizeof(metadata_suffix) - 1;
                            metadata_suffix[suffix_written] = '\0';
                        }
                        metadata_reserved_columns = suffix_written;
                    }
                }
            } else if (metadata_suffix_key != NULL && metadata_suffix_key[0] != '\0') {
                const char* meta_value = history_entry_get_metadata(entry, metadata_suffix_key);
                char formatted_meta_value[64];
                const char* shown_value = history_search_pretty_metadata_value(
                    metadata_suffix_key, meta_value, formatted_meta_value,
                    sizeof(formatted_meta_value));
                int suffix_written =
                    snprintf(metadata_suffix, sizeof(metadata_suffix), " [%s]", shown_value);
                if (suffix_written > 0) {
                    if (suffix_written >= (int)sizeof(metadata_suffix)) {
                        suffix_written = (int)sizeof(metadata_suffix) - 1;
                        metadata_suffix[suffix_written] = '\0';
                    }
                    metadata_reserved_columns = suffix_written;
                }
            }

            const char* display = entry->command;
            const char* line_end = get_first_line_end(display);
            ssize_t entry_len = line_end ? (line_end - display) : (ssize_t)strlen(display);
            bool is_multiline = (line_end && (*line_end == '\n' || *line_end == '\r'));

            ssize_t marker_columns = 4;
            ssize_t max_columns = term_width - marker_columns - metadata_reserved_columns;
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

            bool is_selected = (match_idx == selected_idx);
            if (is_selected) {
                sbuf_append(eb->extra, "[ic-menu-selected]");
            }
            const char* arrow = (tty_is_utf8(env->tty) ? "\xE2\x86\x92" : ">");
            sbuf_appendf(eb->extra, "[!pre]%s ", (is_selected ? arrow : " "));

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
                        if (is_selected) {
                            sbuf_append(eb->extra, "[/pre][u][!pre]");
                        } else {
                            sbuf_append(eb->extra, "[/pre][u ic-emphasis][!pre]");
                        }
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

            if (metadata_suffix[0] != '\0') {
                if (is_selected) {
                    sbuf_appendf(eb->extra, "[ic-menu-selected-secondary]%s[/]", metadata_suffix);
                } else {
                    sbuf_appendf(eb->extra, "[ic-diminish]%s[/]", metadata_suffix);
                }
            }

            if (is_selected) {
                sbuf_append(eb->extra, "[/ic-menu-selected]");
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
        if (metadata_filter_applied) {
            sbuf_appendf(eb->extra,
                         "[ic-info]No history entries matched metadata filters - case %s%s[/]\n",
                         session_case_sensitive ? "sensitive" : "insensitive", mouse_suffix);
        } else {
            sbuf_appendf(eb->extra, "[ic-info]No matches found - case %s%s[/]\n",
                         session_case_sensitive ? "sensitive" : "insensitive", mouse_suffix);
        }
    }

    if (!env->no_help) {
        sbuf_append(eb->extra,
                    "[ic-diminish](↑↓/wheel:navigate shift+↑/↓:page enter:run tab:edit alt+c:case "
                    "esc:cancel)[/]");
    }

    edit_refresh(env, eb);

    code_t c = tty_read(env->tty);
    if (tty_term_resize_event(env->tty)) {
        edit_resize(env, eb);
    }
    sbuf_clear(eb->extra);

    code_t key_no_mods = KEY_NO_MODS(c);
    if (menu_mouse_scroll_enabled && key_no_mods == KEY_EVENT_MOUSE_OTHER) {
        bool accept_selection = false;
        if (history_menu_mouse_select(env, eb, match_count, scroll_offset, last_display_count,
                                      &selected_idx, &accept_selection)) {
            if (accept_selection) {
                c = KEY_TAB;
                key_no_mods = KEY_TAB;
            } else {
                goto again;
            }
        } else {
            goto again;
        }
    }

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
        ic_enable_highlight(old_highlight);
        edit_disable_menu_mouse_scroll(env, menu_mouse_scroll_enabled);
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
        ic_enable_highlight(old_highlight);
        edit_disable_menu_mouse_scroll(env, menu_mouse_scroll_enabled);
        edit_refresh(env, eb);

        eb->request_submit = true;
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
        ic_enable_highlight(old_highlight);
        edit_disable_menu_mouse_scroll(env, menu_mouse_scroll_enabled);
        edit_refresh(env, eb);
        return;
    }

    if ((KEY_MODS(c) & KEY_MOD_SHIFT) && key_no_mods == KEY_DOWN) {
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
    } else if ((KEY_MODS(c) & KEY_MOD_SHIFT) && key_no_mods == KEY_UP) {
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
    } else if ((KEY_MODS(c) & KEY_MOD_ALT) && (key_no_mods == 'c' || key_no_mods == 'C')) {
        session_case_sensitive = !session_case_sensitive;
        goto again;
    } else if (key_no_mods == KEY_UP || c == KEY_CTRL_P ||
               (menu_mouse_scroll_enabled && key_no_mods == KEY_EVENT_MOUSE_WHEEL_UP)) {
        if (selected_idx > 0) {
            selected_idx--;
        } else {
            term_beep(env->term);
        }
        goto again;
    } else if (key_no_mods == KEY_DOWN || c == KEY_CTRL_N ||
               (menu_mouse_scroll_enabled && key_no_mods == KEY_EVENT_MOUSE_WHEEL_DOWN)) {
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

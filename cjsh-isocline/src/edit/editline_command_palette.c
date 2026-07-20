/*
  editline_command_palette.c

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
// Command palette: this file is included in editline.c
//-------------------------------------------------------------

#define MAX_COMMAND_PALETTE_RESULTS 128

typedef struct command_palette_action_entry_s {
    ic_key_action_t action;
    const char* name;
    const char* description;
    const char* keywords;
} command_palette_action_entry_t;

typedef struct command_palette_match_s {
    bool is_custom;
    ssize_t item_idx;
    int score;
    ssize_t match_pos;
    ssize_t match_len;
} command_palette_match_t;

static const command_palette_action_entry_t command_palette_actions[] = {
    {IC_KEY_ACTION_COMPLETE, "Complete Input", "show completion suggestions at the cursor",
     "autocomplete complete suggestion finish word"},
    {IC_KEY_ACTION_HISTORY_SEARCH, "Search History", "open fuzzy history search",
     "history reverse search find previous command"},
    {IC_KEY_ACTION_HISTORY_PREV, "Previous History Entry", "load the previous command from history",
     "history back older previous"},
    {IC_KEY_ACTION_HISTORY_NEXT, "Next History Entry", "load the next command from history",
     "history forward newer next"},
    {IC_KEY_ACTION_UNDO, "Undo", "undo the latest edit", "undo revert back"},
    {IC_KEY_ACTION_REDO, "Redo", "redo the latest undone edit", "redo repeat forward"},
    {IC_KEY_ACTION_CLEAR_SCREEN, "Clear Screen", "clear the terminal screen",
     "clear cls wipe redraw"},
    {IC_KEY_ACTION_SHOW_HELP, "Show Help", "open interactive editing help",
     "help manual keybindings shortcuts"},
    {IC_KEY_ACTION_INSERT_NEWLINE, "Insert Newline", "insert a newline in multiline mode",
     "newline line break multi line"},
    {IC_KEY_ACTION_CURSOR_LEFT, "Cursor Left", "move cursor one character left",
     "move left previous character"},
    {IC_KEY_ACTION_CURSOR_RIGHT_OR_COMPLETE, "Cursor Right", "move cursor right or complete at end",
     "move right next character smart"},
    {IC_KEY_ACTION_CURSOR_UP, "Cursor Up", "move cursor one row up", "move up row previous line"},
    {IC_KEY_ACTION_CURSOR_DOWN, "Cursor Down", "move cursor one row down",
     "move down row next line"},
    {IC_KEY_ACTION_CURSOR_LINE_START, "Line Start", "move cursor to start of line",
     "home beginning line start"},
    {IC_KEY_ACTION_CURSOR_LINE_END, "Line End", "move cursor to end of line",
     "end line finish line"},
    {IC_KEY_ACTION_CURSOR_WORD_PREV, "Previous Word", "move cursor to previous word",
     "word left backward previous token"},
    {IC_KEY_ACTION_CURSOR_WORD_NEXT_OR_COMPLETE, "Next Word",
     "move cursor to next word or complete at end", "word right forward next token"},
    {IC_KEY_ACTION_CURSOR_INPUT_START, "Input Start", "move cursor to start of input",
     "input beginning top first"},
    {IC_KEY_ACTION_CURSOR_INPUT_END, "Input End", "move cursor to end of input",
     "input finish bottom last"},
    {IC_KEY_ACTION_CURSOR_MATCH_BRACE, "Match Brace", "jump to matching brace",
     "brace bracket pair matching jump"},
    {IC_KEY_ACTION_DELETE_BACKWARD, "Delete Backward", "delete the character before the cursor",
     "backspace delete previous character"},
    {IC_KEY_ACTION_DELETE_FORWARD, "Delete Forward", "delete the character at the cursor",
     "delete remove next character"},
    {IC_KEY_ACTION_DELETE_WORD_END, "Delete To Word End",
     "delete from cursor to the end of the word", "kill word forward delete"},
    {IC_KEY_ACTION_DELETE_WORD_START_WS, "Delete To Whitespace",
     "delete backwards to previous whitespace", "delete whitespace backward"},
    {IC_KEY_ACTION_DELETE_WORD_START, "Delete To Word Start", "delete backwards to start of word",
     "kill word backward delete"},
    {IC_KEY_ACTION_DELETE_LINE_START, "Delete To Line Start", "delete from cursor to line start",
     "delete kill beginning line"},
    {IC_KEY_ACTION_DELETE_LINE_END, "Delete To Line End", "delete from cursor to line end",
     "delete kill end line"},
    {IC_KEY_ACTION_TRANSPOSE_CHARS, "Transpose Characters",
     "swap character with previous character", "swap transpose characters"},
    {IC_KEY_ACTION_YANK_LAST_ARG, "Insert Last Argument",
     "insert the last argument from previous history entries",
     "yank last argument previous command"},
    {IC_KEY_ACTION_TOGGLE_MOUSE_REPORTING, "Toggle Mouse Reporting",
     "toggle mouse clicking support for this prompt", "mouse pointer click reporting toggle"},
};

static ssize_t command_palette_action_count(void) {
    return (ssize_t)(sizeof(command_palette_actions) / sizeof(command_palette_actions[0]));
}

static ssize_t command_palette_custom_count(const ic_env_t* env) {
    if (env == NULL || env->command_palette_entries == NULL ||
        env->command_palette_entry_count <= 0) {
        return 0;
    }
    return env->command_palette_entry_count;
}

static bool command_palette_char_equals(char left, char right, bool case_sensitive) {
    if (case_sensitive) {
        return (left == right);
    }
    return (ic_tolower(left) == ic_tolower(right));
}

static bool command_palette_find_substring(const char* haystack, const char* needle,
                                           bool case_sensitive, ssize_t* pos_out,
                                           ssize_t* len_out) {
    if (pos_out != NULL) {
        *pos_out = -1;
    }
    if (len_out != NULL) {
        *len_out = 0;
    }
    if (haystack == NULL || needle == NULL || needle[0] == '\0') {
        return false;
    }

    ssize_t hlen = ic_strlen(haystack);
    ssize_t nlen = ic_strlen(needle);
    if (hlen <= 0 || nlen <= 0 || nlen > hlen) {
        return false;
    }

    for (ssize_t i = 0; i + nlen <= hlen; ++i) {
        bool matched = true;
        for (ssize_t j = 0; j < nlen; ++j) {
            if (!command_palette_char_equals(haystack[i + j], needle[j], case_sensitive)) {
                matched = false;
                break;
            }
        }
        if (matched) {
            if (pos_out != NULL) {
                *pos_out = i;
            }
            if (len_out != NULL) {
                *len_out = nlen;
            }
            return true;
        }
    }
    return false;
}

static int command_palette_fuzzy_match_score(const char* entry, const char* query,
                                             ssize_t* match_pos, ssize_t* match_len,
                                             bool case_sensitive) {
    if (entry == NULL || query == NULL || query[0] == '\0') {
        return -1;
    }

    const char* e = entry;
    const char* q = query;
    ssize_t first_match = -1;
    ssize_t last_match = -1;
    ssize_t consecutive = 0;
    ssize_t max_consecutive = 0;
    int score = 0;
    bool in_match = false;

    while (*e && *q) {
        char e_compare = case_sensitive ? *e : ic_tolower(*e);
        char q_compare = case_sensitive ? *q : ic_tolower(*q);

        if (e_compare == q_compare) {
            if (first_match == -1) {
                first_match = e - entry;
            }
            last_match = e - entry;

            if (in_match) {
                consecutive++;
                score += 5;
            } else {
                consecutive = 1;
                in_match = true;
            }

            if (e == entry || *(e - 1) == ' ' || *(e - 1) == '/' || *(e - 1) == '-' ||
                *(e - 1) == '_') {
                score += 10;
            }

            if (case_sensitive) {
                if (*e == *q) {
                    score += 2;
                }
            } else {
                score += 2;
            }

            q++;
            score += 1;
        } else {
            if (consecutive > max_consecutive) {
                max_consecutive = consecutive;
            }
            consecutive = 0;
            in_match = false;
        }
        e++;
    }

    if (consecutive > max_consecutive) {
        max_consecutive = consecutive;
    }

    if (*q != '\0') {
        return -1;
    }

    score += max_consecutive * 3;

    if (first_match >= 0 && last_match >= 0) {
        ssize_t span = last_match - first_match + 1;
        score -= (int)(span / 2);
    }

    score -= (int)(strlen(entry) / 10);

    if (match_pos != NULL) {
        *match_pos = first_match;
    }
    if (match_len != NULL) {
        *match_len = (first_match >= 0 && last_match >= 0) ? (last_match - first_match + 1) : 0;
    }

    return score;
}

static int command_palette_token_bonus(const command_palette_action_entry_t* entry,
                                       const char* query, bool case_sensitive) {
    if (entry == NULL || query == NULL || query[0] == '\0') {
        return 0;
    }

    int bonus = 0;
    const char* cursor = query;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }

        const char* token_start = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            cursor++;
        }

        size_t token_len = (size_t)(cursor - token_start);
        while (token_len > 0 && !isalnum((unsigned char)token_start[token_len - 1]) &&
               token_start[token_len - 1] != '_' && token_start[token_len - 1] != '-') {
            token_len--;
        }
        while (token_len > 0 && !isalnum((unsigned char)token_start[0]) && token_start[0] != '_' &&
               token_start[0] != '-') {
            token_start++;
            token_len--;
        }

        if (token_len < 2 || token_len >= 64) {
            continue;
        }

        char token[64];
        ic_memcpy(token, token_start, (ssize_t)token_len);
        token[token_len] = '\0';

        if (command_palette_find_substring(entry->name, token, case_sensitive, NULL, NULL)) {
            bonus += 26;
        } else if (command_palette_find_substring(entry->keywords, token, case_sensitive, NULL,
                                                  NULL)) {
            bonus += 18;
        } else if (command_palette_find_substring(entry->description, token, case_sensitive, NULL,
                                                  NULL)) {
            bonus += 10;
        }
    }

    return bonus;
}

static bool command_palette_score_action(const command_palette_action_entry_t* entry,
                                         const char* query, bool case_sensitive, int* score_out,
                                         ssize_t* match_pos_out, ssize_t* match_len_out) {
    if (entry == NULL || score_out == NULL || match_pos_out == NULL || match_len_out == NULL) {
        return false;
    }

    *score_out = 0;
    *match_pos_out = -1;
    *match_len_out = 0;

    if (query == NULL || query[0] == '\0') {
        return true;
    }

    ssize_t name_match_pos = -1;
    ssize_t name_match_len = 0;
    int name_score = command_palette_fuzzy_match_score(entry->name, query, &name_match_pos,
                                                       &name_match_len, case_sensitive);

    ssize_t description_match_pos = -1;
    ssize_t description_match_len = 0;
    int description_score = command_palette_fuzzy_match_score(
        entry->description, query, &description_match_pos, &description_match_len, case_sensitive);

    ssize_t keywords_match_pos = -1;
    ssize_t keywords_match_len = 0;
    int keywords_score = command_palette_fuzzy_match_score(
        entry->keywords, query, &keywords_match_pos, &keywords_match_len, case_sensitive);

    if (name_score < 0 && description_score < 0 && keywords_score < 0) {
        return false;
    }

    int best_score = -1;
    ssize_t best_match_pos = -1;
    ssize_t best_match_len = 0;

    if (name_score >= 0) {
        best_score = name_score + 90;
        best_match_pos = name_match_pos;
        best_match_len = name_match_len;
    }
    if (keywords_score >= 0) {
        int weighted = keywords_score + 55;
        if (weighted > best_score) {
            best_score = weighted;
            best_match_pos = -1;
            best_match_len = 0;
        }
    }
    if (description_score >= 0) {
        int weighted = description_score + 35;
        if (weighted > best_score) {
            best_score = weighted;
            best_match_pos = -1;
            best_match_len = 0;
        }
    }

    ssize_t exact_name_pos = -1;
    ssize_t exact_name_len = 0;
    if (command_palette_find_substring(entry->name, query, case_sensitive, &exact_name_pos,
                                       &exact_name_len)) {
        best_score += 45;
        best_match_pos = exact_name_pos;
        best_match_len = exact_name_len;
    }

    best_score += command_palette_token_bonus(entry, query, case_sensitive);

    if (best_match_pos < 0 || best_match_len <= 0) {
        const char* cursor = query;
        while (*cursor != '\0') {
            while (*cursor == ' ' || *cursor == '\t') {
                cursor++;
            }
            if (*cursor == '\0') {
                break;
            }

            const char* token_start = cursor;
            while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
                cursor++;
            }

            size_t token_len = (size_t)(cursor - token_start);
            if (token_len < 2 || token_len >= 64) {
                continue;
            }

            char token[64];
            ic_memcpy(token, token_start, (ssize_t)token_len);
            token[token_len] = '\0';

            if (command_palette_find_substring(entry->name, token, case_sensitive, &best_match_pos,
                                               &best_match_len)) {
                break;
            }
        }
    }

    *score_out = best_score;
    *match_pos_out = best_match_pos;
    *match_len_out = best_match_len;
    return true;
}

static int command_palette_compare_matches(const void* left, const void* right) {
    const command_palette_match_t* a = (const command_palette_match_t*)left;
    const command_palette_match_t* b = (const command_palette_match_t*)right;

    if (a == NULL || b == NULL) {
        return 0;
    }

    if (b->score != a->score) {
        return (b->score - a->score);
    }

    if (a->is_custom != b->is_custom) {
        return (a->is_custom ? 1 : -1);
    }

    if (a->item_idx < b->item_idx) {
        return -1;
    }
    if (a->item_idx > b->item_idx) {
        return 1;
    }
    return 0;
}

static ssize_t command_palette_search_actions(ic_env_t* env, const char* query, bool case_sensitive,
                                              command_palette_match_t* matches,
                                              ssize_t max_matches) {
    if (matches == NULL || max_matches <= 0) {
        return 0;
    }

    ssize_t count = 0;
    ssize_t total_actions = command_palette_action_count();
    for (ssize_t i = 0; i < total_actions && count < max_matches; ++i) {
        const command_palette_action_entry_t* entry = &command_palette_actions[i];
        int score = 0;
        ssize_t match_pos = -1;
        ssize_t match_len = 0;

        if (!command_palette_score_action(entry, query, case_sensitive, &score, &match_pos,
                                          &match_len)) {
            continue;
        }

        if (query == NULL || query[0] == '\0') {
            score = (int)(1000 - i);
        }

        matches[count].is_custom = false;
        matches[count].item_idx = i;
        matches[count].score = score;
        matches[count].match_pos = match_pos;
        matches[count].match_len = match_len;
        count++;
    }

    ssize_t total_custom_entries = command_palette_custom_count(env);
    for (ssize_t i = 0; i < total_custom_entries && count < max_matches; ++i) {
        const ic_command_palette_entry_internal_t* custom_entry = &env->command_palette_entries[i];
        command_palette_action_entry_t score_entry = {
            IC_KEY_ACTION_NONE,
            custom_entry->name,
            custom_entry->description,
            custom_entry->keywords,
        };

        int score = 0;
        ssize_t match_pos = -1;
        ssize_t match_len = 0;

        if (!command_palette_score_action(&score_entry, query, case_sensitive, &score, &match_pos,
                                          &match_len)) {
            continue;
        }

        if (query == NULL || query[0] == '\0') {
            score = (int)(500 - i);
        }

        matches[count].is_custom = true;
        matches[count].item_idx = i;
        matches[count].score = score;
        matches[count].match_pos = match_pos;
        matches[count].match_len = match_len;
        count++;
    }

    if (count > 1) {
        qsort(matches, (size_t)count, sizeof(matches[0]), command_palette_compare_matches);
    }
    return count;
}

static void edit_command_palette(ic_env_t* env, editor_t* eb) {
    if (env == NULL || eb == NULL) {
        return;
    }

    if (command_palette_action_count() + command_palette_custom_count(env) <= 0) {
        term_beep(env->term);
        return;
    }

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
    eb->prompt_text = "command palette: ";

    command_palette_match_t* matches = (command_palette_match_t*)mem_zalloc_tp_n(
        env->mem, command_palette_match_t, MAX_COMMAND_PALETTE_RESULTS);
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

    sbuf_clear(eb->input);
    eb->pos = 0;

    ssize_t match_count = 0;
    ssize_t selected_idx = 0;
    ssize_t scroll_offset = 0;
    ssize_t last_display_count = 0;
    ssize_t last_max_scroll = 0;
    bool session_case_sensitive = false;

    bool menu_mouse_scroll_enabled = edit_enable_menu_mouse_scroll(env);

again:;

    last_display_count = 0;
    last_max_scroll = 0;

    bool showing_all_due_to_no_matches = false;

    {
        const char* query = sbuf_string(eb->input);
        match_count = command_palette_search_actions(
            env, query ? query : "", session_case_sensitive, matches, MAX_COMMAND_PALETTE_RESULTS);

        if (match_count == 0 && query != NULL && query[0] != '\0') {
            match_count = command_palette_search_actions(env, "", session_case_sensitive, matches,
                                                         MAX_COMMAND_PALETTE_RESULTS);
            showing_all_due_to_no_matches = true;
        }
    }

    if (selected_idx >= match_count) {
        selected_idx = (match_count > 0 ? match_count - 1 : 0);
    }
    if (selected_idx < 0) {
        selected_idx = 0;
    }

    sbuf_clear(eb->extra);
    const char* mouse_suffix = (menu_mouse_scroll_enabled ? " | Mouse clicking is enabled" : "");

    if (match_count > 0) {
        const char* query = sbuf_string(eb->input);
        bool is_filtered = (query != NULL && query[0] != '\0');
        ssize_t total_actions = command_palette_action_count() + command_palette_custom_count(env);

        if (showing_all_due_to_no_matches) {
            sbuf_appendf(
                eb->extra,
                "[ic-info]No matches - showing all actions (%zd action%s) - case %s%s[/]\n",
                total_actions, total_actions == 1 ? "" : "s",
                session_case_sensitive ? "sensitive" : "insensitive", mouse_suffix);
        } else if (is_filtered) {
            sbuf_appendf(eb->extra, "[ic-info]%zd action%s found - case %s%s[/]\n", match_count,
                         match_count == 1 ? "" : "s",
                         session_case_sensitive ? "sensitive" : "insensitive", mouse_suffix);
        } else {
            sbuf_appendf(eb->extra, "[ic-info]Actions (%zd total) - case %s%s[/]\n", total_actions,
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

        ssize_t display_count = (match_count > available_lines ? available_lines : match_count);
        if (display_count < 1) {
            display_count = 1;
        }

        ssize_t max_scroll = (match_count > display_count ? (match_count - display_count) : 0);
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

        for (ssize_t i = 0; i < display_count; ++i) {
            ssize_t match_idx = scroll_offset + i;
            if (match_idx >= match_count) {
                break;
            }

            const command_palette_match_t* match = &matches[match_idx];
            const char* entry_name = NULL;
            const char* entry_description = NULL;
            ic_key_action_t entry_action = IC_KEY_ACTION_NONE;
            bool entry_is_custom = match->is_custom;

            if (entry_is_custom) {
                if (match->item_idx < 0 || match->item_idx >= command_palette_custom_count(env)) {
                    continue;
                }
                const ic_command_palette_entry_internal_t* custom_entry =
                    &env->command_palette_entries[match->item_idx];
                entry_name = custom_entry->name;
                entry_description = custom_entry->description;
            } else {
                if (match->item_idx < 0 || match->item_idx >= command_palette_action_count()) {
                    continue;
                }
                const command_palette_action_entry_t* entry =
                    &command_palette_actions[match->item_idx];
                entry_name = entry->name;
                entry_description = entry->description;
                entry_action = entry->action;
            }

            if (entry_name == NULL || entry_name[0] == '\0') {
                continue;
            }
            if (entry_description == NULL) {
                entry_description = "";
            }

            char linebuf[512];
            int written = -1;
            if (entry_is_custom) {
                if (entry_description[0] == '\0') {
                    written = snprintf(linebuf, sizeof(linebuf), "%s (custom)", entry_name);
                } else if (strstr(entry_description, "(custom)") != NULL) {
                    written = snprintf(linebuf, sizeof(linebuf), "%s - %s", entry_name,
                                       entry_description);
                } else {
                    written = snprintf(linebuf, sizeof(linebuf), "%s - %s (custom)", entry_name,
                                       entry_description);
                }
            } else {
                char binding_keys[64];
                format_binding_keys(env, entry_action, NULL, binding_keys, sizeof(binding_keys),
                                    true);
                if (entry_description[0] == '\0') {
                    written =
                        snprintf(linebuf, sizeof(linebuf), "%s [%s]", entry_name, binding_keys);
                } else {
                    written = snprintf(linebuf, sizeof(linebuf), "%s - %s [%s]", entry_name,
                                       entry_description, binding_keys);
                }
            }
            if (written < 0) {
                continue;
            }
            if (written >= (int)sizeof(linebuf)) {
                linebuf[sizeof(linebuf) - 1] = '\0';
            }

            const char* display = linebuf;
            const char* line_end = get_first_line_end(display);
            ssize_t entry_len = line_end ? (line_end - display) : (ssize_t)strlen(display);
            bool is_multiline = (line_end && (*line_end == '\n' || *line_end == '\r'));

            ssize_t marker_columns = 4;
            ssize_t max_columns = term_width - marker_columns;
            if (max_columns < 4) {
                max_columns = 4;
            }

            ssize_t visible_width = 0;
            ssize_t visible_len =
                history_visible_prefix(display, entry_len, max_columns, &visible_width);
            bool truncated = (visible_len < entry_len);
            bool append_ellipsis = (is_multiline || truncated);

            if (append_ellipsis && max_columns > 3) {
                if (visible_width + 3 > max_columns) {
                    ssize_t adjusted_columns = max_columns - 3;
                    if (adjusted_columns < 1) {
                        adjusted_columns = 1;
                    }
                    visible_len = history_visible_prefix(display, entry_len, adjusted_columns,
                                                         &visible_width);
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

            if (is_filtered && !showing_all_due_to_no_matches && match->match_len > 0 &&
                match->match_pos >= 0) {
                ssize_t match_pos = match->match_pos;
                ssize_t match_len = match->match_len;
                if (match_pos < 0) {
                    match_pos = 0;
                }
                if (match_pos < visible_len) {
                    if (match_pos > 0) {
                        ssize_t prefix_len = (match_pos <= visible_len ? match_pos : visible_len);
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
        sbuf_appendf(eb->extra, "[ic-info]No actions found - case %s%s[/]\n",
                     session_case_sensitive ? "sensitive" : "insensitive", mouse_suffix);
    }

    if (!env->no_help) {
        sbuf_append(eb->extra,
                    "[ic-diminish](↑↓/wheel:navigate shift+↑/↓:page enter/tab:run alt+c:case "
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
        sbuf_clear(eb->extra);
        eb->disable_undo = false;
        editor_undo_restore(eb, false);
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
    }

    if (c == KEY_ENTER || c == KEY_TAB) {
        if (match_count <= 0 || selected_idx < 0 || selected_idx >= match_count) {
            term_beep(env->term);
            goto again;
        }

        const command_palette_match_t* selected_match = &matches[selected_idx];
        bool selected_is_custom = selected_match->is_custom;
        ic_key_action_t selected_action = IC_KEY_ACTION_NONE;
        ic_command_palette_entry_t selected_custom_entry = {0};

        if (selected_is_custom) {
            if (selected_match->item_idx < 0 ||
                selected_match->item_idx >= command_palette_custom_count(env)) {
                term_beep(env->term);
                goto again;
            }
            const ic_command_palette_entry_internal_t* custom_entry =
                &env->command_palette_entries[selected_match->item_idx];
            selected_custom_entry.id = custom_entry->id;
            selected_custom_entry.name = custom_entry->name;
            selected_custom_entry.description = custom_entry->description;
            selected_custom_entry.keywords = custom_entry->keywords;
        } else {
            if (selected_match->item_idx < 0 ||
                selected_match->item_idx >= command_palette_action_count()) {
                term_beep(env->term);
                goto again;
            }
            selected_action = command_palette_actions[selected_match->item_idx].action;
        }

        sbuf_clear(eb->extra);
        eb->disable_undo = false;
        editor_undo_restore(eb, false);
        mem_free(env->mem, matches);
        eb->prompt_text = prompt_text;
        eb->replace_prompt_line_with_number = prompt_replacement;
        eb->force_prompt_text_visible = force_prompt_visibility;
        eb->line_number_column_width = saved_line_number_width;
        ic_enable_hint(old_hint);
        ic_enable_highlight(old_highlight);
        edit_disable_menu_mouse_scroll(env, menu_mouse_scroll_enabled);

        bool handled = false;
        if (selected_is_custom) {
            if (env->command_palette_handler != NULL) {
                handled = env->command_palette_handler(&selected_custom_entry,
                                                       env->command_palette_handler_arg);
            }
        } else {
            handled = key_action_execute(env, eb, selected_action, KEY_NONE);
        }

        if (!handled) {
            term_beep(env->term);
        }
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

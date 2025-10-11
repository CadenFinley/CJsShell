/* ----------------------------------------------------------------------------
  Copyright (c) 2021, Daan Leijen
  Largely Modified by Caden Finley 2025 for CJ's Shell
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License. A copy of the license can be
  found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/

//-------------------------------------------------------------
// Help: this is included into editline.c
//-------------------------------------------------------------

typedef enum help_line_type_e {
    HELP_LINE_BLANK,
    HELP_LINE_HEADING,
    HELP_LINE_BINDING,
    HELP_LINE_STATIC
} help_line_type_t;

typedef struct help_line_s {
    help_line_type_t type;
    const char* text;
    const char* description;
    ic_key_action_t action;
    const char* default_specs;
} help_line_t;

#include "isocline.h"

static const help_line_t help_lines[] = {
    {HELP_LINE_BLANK, NULL, NULL, IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_HEADING, "Navigation:", NULL, IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_BINDING, NULL, "go one character to the left", IC_KEY_ACTION_CURSOR_LEFT, NULL},
    {HELP_LINE_BINDING, NULL, "go one character to the right",
     IC_KEY_ACTION_CURSOR_RIGHT_OR_COMPLETE, NULL},
    {HELP_LINE_BINDING, NULL, "go one row up, or back in the history", IC_KEY_ACTION_CURSOR_UP,
     NULL},
    {HELP_LINE_BINDING, NULL, "go one row down, or forward in the history",
     IC_KEY_ACTION_CURSOR_DOWN, NULL},
    {HELP_LINE_BINDING, NULL, "go to the start of the previous word",
     IC_KEY_ACTION_CURSOR_WORD_PREV, NULL},
    {HELP_LINE_BINDING, NULL, "go to the end of the current word",
     IC_KEY_ACTION_CURSOR_WORD_NEXT_OR_COMPLETE, NULL},
    {HELP_LINE_BINDING, NULL, "go to the start of the current line",
     IC_KEY_ACTION_CURSOR_LINE_START, NULL},
    {HELP_LINE_BINDING, NULL, "go to the end of the current line", IC_KEY_ACTION_CURSOR_LINE_END,
     NULL},
    {HELP_LINE_BINDING, NULL, "go to the start of the current input",
     IC_KEY_ACTION_CURSOR_INPUT_START, NULL},
    {HELP_LINE_BINDING, NULL, "go to the end of the current input", IC_KEY_ACTION_CURSOR_INPUT_END,
     NULL},
    {HELP_LINE_BINDING, NULL, "jump to matching brace", IC_KEY_ACTION_CURSOR_MATCH_BRACE, NULL},
    {HELP_LINE_BINDING, NULL, "go back in the history", IC_KEY_ACTION_HISTORY_PREV, NULL},
    {HELP_LINE_BINDING, NULL, "go forward in the history", IC_KEY_ACTION_HISTORY_NEXT, NULL},
    {HELP_LINE_BINDING, NULL, "search the history starting with the current word",
     IC_KEY_ACTION_HISTORY_SEARCH, NULL},
    {HELP_LINE_BLANK, NULL, NULL, IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_HEADING, "Deletion:", NULL, IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_BINDING, NULL, "delete the current character", IC_KEY_ACTION_DELETE_FORWARD, NULL},
    {HELP_LINE_BINDING, NULL, "delete the previous character", IC_KEY_ACTION_DELETE_BACKWARD, NULL},
    {HELP_LINE_BINDING, NULL, "delete to preceding white space", IC_KEY_ACTION_DELETE_WORD_START_WS,
     NULL},
    {HELP_LINE_BINDING, NULL, "delete to the start of the current word",
     IC_KEY_ACTION_DELETE_WORD_START, NULL},
    {HELP_LINE_BINDING, NULL, "delete to the end of the current word",
     IC_KEY_ACTION_DELETE_WORD_END, NULL},
    {HELP_LINE_BINDING, NULL, "delete to the start of the current line",
     IC_KEY_ACTION_DELETE_LINE_START, NULL},
    {HELP_LINE_BINDING, NULL, "delete to the end of the current line",
     IC_KEY_ACTION_DELETE_LINE_END, NULL},
    {HELP_LINE_STATIC, "esc", "delete the current input, or done with empty input",
     IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_BLANK, NULL, NULL, IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_HEADING, "Editing:", NULL, IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_STATIC, "enter", "accept current input", IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_BINDING, NULL, "create a new line for multi-line input",
     IC_KEY_ACTION_INSERT_NEWLINE, NULL},
    {HELP_LINE_BINDING, NULL, "clear screen", IC_KEY_ACTION_CLEAR_SCREEN, NULL},
    {HELP_LINE_BINDING, NULL, "swap with previous character (move character backward)",
     IC_KEY_ACTION_TRANSPOSE_CHARS, NULL},
    {HELP_LINE_BINDING, NULL, "undo", IC_KEY_ACTION_UNDO, NULL},
    {HELP_LINE_BINDING, NULL, "redo", IC_KEY_ACTION_REDO, NULL},
    {HELP_LINE_BINDING, NULL, "try to complete the current input", IC_KEY_ACTION_COMPLETE, NULL},
    {HELP_LINE_BLANK, NULL, NULL, IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_HEADING, "In the completion menu:", NULL, IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_STATIC, "enter,left", "use the currently selected completion", IC_KEY_ACTION__MAX,
     NULL},
    {HELP_LINE_STATIC, "1 - 9", "use completion N from the menu", IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_STATIC, "tab,down", "select the next completion", IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_STATIC, "shift-tab,up", "select the previous completion", IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_STATIC, "esc", "exit menu without completing", IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_STATIC, "pgdn,^j", "show all further possible completions", IC_KEY_ACTION__MAX,
     NULL},
    {HELP_LINE_BLANK, NULL, NULL, IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_HEADING, "In incremental history search:", NULL, IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_STATIC, "enter", "use the currently found history entry", IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_STATIC, "backsp,^z", "go back to the previous match (undo)", IC_KEY_ACTION__MAX,
     NULL},
    {HELP_LINE_STATIC, "tab,^r", "find the next match", IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_STATIC, "shift-tab,^s", "find an earlier match", IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_STATIC, "esc", "exit search", IC_KEY_ACTION__MAX, NULL},
    {HELP_LINE_STATIC, " ", "", IC_KEY_ACTION__MAX, NULL},
};

static const char* help_initial =
    "[ic-info]"
    "Isocline v1.0, copyright (c) 2021 Daan Leijen.\n"
    "Largely Modified by Caden Finley 2025 for CJ's Shell.\n"
    "This is free software; you can redistribute it and/or\n"
    "modify it under the terms of the MIT License.\n"
    "See <[url]https://github.com/daanx/isocline[/url]> for further "
    "information.\n"
    "We use ^<key> as a shorthand for ctrl-<key>.\n"
    "\n"
    "Overview:\n"
    "\n[ansi-lightgray]"
    "       home,ctrl-a      cursor     end,ctrl-e\n"
    "         ┌────────────────┼───────────────┐    (navigate)\n"
//"       │                │               │\n"
#ifndef __APPLE__
    "         │    ctrl-left   │  ctrl-right   │\n"
#else
    "         │     alt-left   │   alt-right   │\n"
#endif
    "         │        ┌───────┼──────┐        │    ctrl-r   : search history\n"
    "         ▼        ▼       ▼      ▼        ▼    tab      : complete word\n"
    "  prompt> [ansi-darkgray]it's the quintessential language[/]     "
    "shift-tab: insert new line\n"
    "         ▲        ▲              ▲        ▲    esc      : delete input, "
    "done\n"
    "         │        └──────────────┘        │    ctrl-z   : undo\n"
    "         │   alt-backsp        alt-d      │\n"
    //"       │                │               │\n"
    "         └────────────────────────────────┘    (delete)\n"
    "       ctrl-u                          ctrl-k\n"
    "[/ansi-lightgray][/ic-info]\n";

static bool key_triggers_action(ic_env_t* env, ic_keycode_t key, ic_key_action_t action) {
    ic_key_action_t configured;
    if (ic_get_key_binding(key, &configured)) {
        return configured == action;
    }
    return true;
}

static void beautify_key_label(char* label) {
    if (label == NULL)
        return;
    size_t len = strlen(label);
    if (len >= 6 && ic_strnicmp(label, "ctrl+", 5) == 0 && strchr(label + 5, '+') == NULL &&
        strlen(label + 5) == 1) {
        label[0] = '^';
        label[1] = label[5];
        label[2] = '\0';
        return;
    }
    for (size_t i = 0; i < len; ++i) {
        if (label[i] == '+')
            label[i] = '-';
    }
}

#define HELP_MAX_LABELS 16
#define HELP_LABEL_LEN 64

static bool key_label_equals(const char* a, const char* b) {
    return (ic_stricmp(a, b) == 0);
}

static bool help_label_exists(char labels[][HELP_LABEL_LEN], size_t count, const char* label) {
    for (size_t i = 0; i < count; ++i) {
        if (key_label_equals(labels[i], label))
            return true;
    }
    return false;
}

static void help_label_add(char labels[][HELP_LABEL_LEN], size_t* count, const char* label) {
    if (label == NULL || labels == NULL || count == NULL)
        return;
    if (*count >= HELP_MAX_LABELS)
        return;
    if (help_label_exists(labels, *count, label))
        return;
    ic_strncpy(labels[*count], HELP_LABEL_LEN, label, HELP_LABEL_LEN - 1);
    beautify_key_label(labels[*count]);
    (*count)++;
}

static void format_binding_keys(ic_env_t* env, ic_key_action_t action, const char* default_specs,
                                char* buffer, size_t buflen) {
    if (buffer == NULL || buflen == 0)
        return;
    char labels[HELP_MAX_LABELS][HELP_LABEL_LEN];
    size_t label_count = 0;

    const char* specs_to_use = default_specs;
    if ((specs_to_use == NULL || specs_to_use[0] == '\0') && action > IC_KEY_ACTION_NONE &&
        action < IC_KEY_ACTION__MAX) {
        specs_to_use = ic_key_binding_profile_default_specs(action);
    }

    if (specs_to_use != NULL && specs_to_use[0] != '\0') {
        const char* spec = specs_to_use;
        size_t len = strlen(specs_to_use);
        size_t start = 0;
        for (size_t i = 0; i <= len; ++i) {
            if (specs_to_use[i] == '|' || specs_to_use[i] == '\0') {
                size_t tok_len = i - start;
                if (tok_len > 0 && tok_len < 64) {
                    char token[64];
                    memcpy(token, spec + start, tok_len);
                    token[tok_len] = '\0';
                    // trim spaces
                    size_t left = 0;
                    while (token[left] == ' ')
                        left++;
                    size_t right = strlen(token);
                    while (right > left && token[right - 1] == ' ')
                        right--;
                    token[right] = '\0';
                    if (right > left) {
                        const char* trimmed = token + left;
                        ic_keycode_t key;
                        if (ic_parse_key_spec(trimmed, &key) &&
                            key_triggers_action(env, key, action)) {
                            char formatted[64];
                            if (ic_format_key_spec(key, formatted, sizeof(formatted))) {
                                help_label_add(labels, &label_count, formatted);
                            }
                        }
                    }
                }
                start = i + 1;
            }
        }
    }

    if (env != NULL && env->key_binding_count > 0 && env->key_bindings != NULL) {
        for (ssize_t i = 0; i < env->key_binding_count; ++i) {
            ic_key_binding_entry_t entry = env->key_bindings[i];
            if (entry.action != action)
                continue;
            char formatted[64];
            if (ic_format_key_spec(entry.key, formatted, sizeof(formatted))) {
                help_label_add(labels, &label_count, formatted);
            }
        }
    }

    if (label_count == 0) {
        ic_strncpy(buffer, (ssize_t)buflen, "(unbound)", (ssize_t)buflen - 1);
        return;
    }

    buffer[0] = '\0';
    size_t pos = 0;
    for (size_t i = 0; i < label_count; ++i) {
        int written = snprintf(buffer + pos, buflen - pos, "%s%s", (i == 0 ? "" : ", "), labels[i]);
        if (written < 0)
            break;
        pos += (size_t)written;
        if (pos >= buflen)
            break;
    }
}

static void edit_show_help(ic_env_t* env, editor_t* eb) {
    edit_clear(env, eb);
    bbcode_println(env->bbcode, help_initial);

    const size_t line_count = sizeof(help_lines) / sizeof(help_lines[0]);
    for (size_t i = 0; i < line_count; ++i) {
        const help_line_t* line = &help_lines[i];
        switch (line->type) {
            case HELP_LINE_BLANK:
                bbcode_println(env->bbcode, "");
                break;
            case HELP_LINE_HEADING:
                bbcode_printf(env->bbcode, "[ic-info]%s[/]\n", line->text);
                break;
            case HELP_LINE_STATIC:
                bbcode_printf(env->bbcode, "  [ic-emphasis]%-13s[/][ansi-lightgray]%s%s[/]\n",
                              line->text, (line->description[0] == 0 ? "" : ": "),
                              line->description);
                break;
            case HELP_LINE_BINDING: {
                char key_buffer[256];
                format_binding_keys(env, line->action, line->default_specs, key_buffer,
                                    sizeof(key_buffer));
                bbcode_printf(env->bbcode, "  [ic-emphasis]%-13s[/][ansi-lightgray]%s%s[/]\n",
                              key_buffer, (line->description[0] == 0 ? "" : ": "),
                              line->description);
                break;
            }
        }
    }

    eb->cur_rows = 0;
    eb->cur_row = 0;
    edit_refresh(env, eb);
}

/* ----------------------------------------------------------------------------
  Key binding type declarations for CJ's Shell Isocline integration.
  Keeping these in a dedicated header keeps the public API surface of
  `isocline.h` lighter for callers that don't need key binding metadata.
-----------------------------------------------------------------------------*/
#pragma once
#ifndef IC_KEYBINDINGS_H
#define IC_KEYBINDINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "keycodes.h"

/// Key action identifiers returned by key binding queries and APIs.
typedef enum ic_key_action_e {
    IC_KEY_ACTION_NONE = 0,
    IC_KEY_ACTION_COMPLETE,
    IC_KEY_ACTION_HISTORY_SEARCH,
    IC_KEY_ACTION_HISTORY_PREV,
    IC_KEY_ACTION_HISTORY_NEXT,
    IC_KEY_ACTION_CLEAR_SCREEN,
    IC_KEY_ACTION_UNDO,
    IC_KEY_ACTION_REDO,
    IC_KEY_ACTION_SHOW_HELP,
    IC_KEY_ACTION_CURSOR_LEFT,
    IC_KEY_ACTION_CURSOR_RIGHT_OR_COMPLETE,
    IC_KEY_ACTION_CURSOR_UP,
    IC_KEY_ACTION_CURSOR_DOWN,
    IC_KEY_ACTION_CURSOR_LINE_START,
    IC_KEY_ACTION_CURSOR_LINE_END,
    IC_KEY_ACTION_CURSOR_WORD_PREV,
    IC_KEY_ACTION_CURSOR_WORD_NEXT_OR_COMPLETE,
    IC_KEY_ACTION_CURSOR_INPUT_START,
    IC_KEY_ACTION_CURSOR_INPUT_END,
    IC_KEY_ACTION_CURSOR_MATCH_BRACE,
    IC_KEY_ACTION_DELETE_BACKWARD,
    IC_KEY_ACTION_DELETE_FORWARD,
    IC_KEY_ACTION_DELETE_WORD_END,
    IC_KEY_ACTION_DELETE_WORD_START_WS,
    IC_KEY_ACTION_DELETE_WORD_START,
    IC_KEY_ACTION_DELETE_LINE_START,
    IC_KEY_ACTION_DELETE_LINE_END,
    IC_KEY_ACTION_TRANSPOSE_CHARS,
    IC_KEY_ACTION_INSERT_NEWLINE,
    IC_KEY_ACTION__MAX
} ic_key_action_t;

/// Entry describing a single key binding.
typedef struct ic_key_binding_entry_s {
    ic_keycode_t key;
    ic_key_action_t action;
} ic_key_binding_entry_t;

/// Human-readable information about a key binding profile.
typedef struct ic_key_binding_profile_info_s {
    const char* name;
    const char* description;
} ic_key_binding_profile_info_t;

#ifdef __cplusplus
}
#endif

#endif  // IC_KEYBINDINGS_H

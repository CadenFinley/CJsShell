/* ----------------------------------------------------------------------------
  Copyright (c) 2025, Caden Finley
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License. A copy of the license can be
  found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/

/* ----------------------------------------------------------------------------
  Input buffer access API for readline.
  Allows reading and modifying the current input line and cursor position.
-----------------------------------------------------------------------------*/

#include <string.h>

#include "common.h"
#include "isocline.h"

//-------------------------------------------------------------
// Internal editor state tracking
//-------------------------------------------------------------

// Forward declare the editor structure from editline.c
typedef struct editor_s editor_t;

// These functions are implemented in editline.c
extern void ic_set_active_editor_impl(editor_t* eb);
extern void ic_clear_active_editor_impl(void);
extern editor_t* ic_get_active_editor_impl(void);

// Helper functions implemented at the end of editline.c
extern const char* ic_editor_get_input(editor_t* eb);
extern ssize_t ic_editor_get_pos(editor_t* eb);
extern bool ic_editor_set_input(editor_t* eb, const char* text, ssize_t pos);

//-------------------------------------------------------------
// Public buffer access API
//-------------------------------------------------------------

ic_public const char* ic_get_input_line(void) {
    editor_t* eb = ic_get_active_editor_impl();
    if (eb == NULL)
        return NULL;
    return ic_editor_get_input(eb);
}

ic_public ssize_t ic_get_cursor_pos(void) {
    editor_t* eb = ic_get_active_editor_impl();
    if (eb == NULL)
        return -1;
    return ic_editor_get_pos(eb);
}

ic_public bool ic_set_input_line(const char* text, ssize_t cursor_pos) {
    editor_t* eb = ic_get_active_editor_impl();
    if (eb == NULL || text == NULL)
        return false;
    return ic_editor_set_input(eb, text, cursor_pos);
}

ic_public bool ic_clear_input_line(void) {
    return ic_set_input_line("", 0);
}

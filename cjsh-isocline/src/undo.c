/*
  undo.c

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

#include "undo.h"

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "completions.h"
#include "env.h"
#include "isocline.h"
#include "stringbuf.h"

//-------------------------------------------------------------
// edit state
//-------------------------------------------------------------
struct editstate_s {
    struct editstate_s* next;
    const char* input;  // input
    ssize_t pos;        // cursor position
};

ic_private void editstate_init(editstate_t** es) {
    *es = NULL;
}

ic_private void editstate_done(alloc_t* mem, editstate_t** es) {
    while (*es != NULL) {
        editstate_t* next = (*es)->next;
        mem_free(mem, (*es)->input);
        mem_free(mem, *es);
        *es = next;
    }
    *es = NULL;
}

ic_private void editstate_capture(alloc_t* mem, editstate_t** es, const char* input, ssize_t pos) {
    if (input == NULL)
        input = "";
    // alloc
    editstate_t* entry = mem_zalloc_tp(mem, editstate_t);
    if (entry == NULL)
        return;
    // initialize
    entry->input = mem_strdup(mem, input);
    entry->pos = pos;
    if (entry->input == NULL) {
        mem_free(mem, entry);
        return;
    }
    // and push
    entry->next = *es;
    *es = entry;
}

// caller should free *input
ic_private bool editstate_restore(alloc_t* mem, editstate_t** es, const char** input,
                                  ssize_t* pos) {
    if (*es == NULL)
        return false;
    // pop
    editstate_t* entry = *es;
    *es = entry->next;
    *input = entry->input;
    *pos = entry->pos;
    mem_free(mem, entry);
    return true;
}

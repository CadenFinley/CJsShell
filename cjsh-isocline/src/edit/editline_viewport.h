/*
  editline_viewport.h

  This file is part of isocline

  MIT License

  Copyright (c) 2026 Caden Finley

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

#ifndef IC_EDITLINE_VIEWPORT_H
#define IC_EDITLINE_VIEWPORT_H

#include "common.h"

typedef struct editline_viewport_s {
    ssize_t input_first_row;
    ssize_t input_row_count;
    ssize_t extra_row_count;
} editline_viewport_t;

static inline editline_viewport_t editline_viewport_for(ssize_t input_rows, ssize_t extra_rows,
                                                        ssize_t cursor_row, ssize_t available_rows,
                                                        size_t max_input_rows) {
    if (input_rows < 1) {
        input_rows = 1;
    }
    if (extra_rows < 0) {
        extra_rows = 0;
    }
    if (cursor_row < 0) {
        cursor_row = 0;
    } else if (cursor_row >= input_rows) {
        cursor_row = input_rows - 1;
    }
    if (available_rows < 1) {
        available_rows = 1;
    }

    ssize_t input_row_count = input_rows;
    const ssize_t input_limit = (max_input_rows < 1 ? 1 : (ssize_t)max_input_rows);
    if (input_row_count > input_limit) {
        input_row_count = input_limit;
    }
    if (input_row_count > available_rows) {
        input_row_count = available_rows;
    }

    ssize_t extra_row_count = extra_rows;
    if (input_row_count + extra_row_count > available_rows) {
        // Extra rows are already windowed by the menu builders. Preserve as many of them as
        // possible while always leaving one editable input row visible.
        const ssize_t max_extra_rows = available_rows - 1;
        if (extra_row_count > max_extra_rows) {
            extra_row_count = max_extra_rows;
        }
        input_row_count = available_rows - extra_row_count;
    }

    ssize_t input_first_row = cursor_row - input_row_count + 1;
    if (input_first_row < 0) {
        input_first_row = 0;
    }
    const ssize_t max_first_row = input_rows - input_row_count;
    if (input_first_row > max_first_row) {
        input_first_row = max_first_row;
    }

    editline_viewport_t viewport = {
        .input_first_row = input_first_row,
        .input_row_count = input_row_count,
        .extra_row_count = extra_row_count,
    };
    return viewport;
}

#endif  // IC_EDITLINE_VIEWPORT_H

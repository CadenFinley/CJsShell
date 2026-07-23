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
    ssize_t first_row;
    ssize_t last_row;
} editline_viewport_t;

static inline editline_viewport_t editline_viewport_for(ssize_t row_count, ssize_t cursor_row,
                                                        ssize_t visible_rows,
                                                        size_t bottom_line_count) {
    if (row_count < 1) {
        row_count = 1;
    }
    if (visible_rows < 1) {
        visible_rows = 1;
    } else if (visible_rows > row_count) {
        visible_rows = row_count;
    }
    if (cursor_row < 0) {
        cursor_row = 0;
    } else if (cursor_row >= row_count) {
        cursor_row = row_count - 1;
    }

    ssize_t bottom_rows =
        (bottom_line_count >= (size_t)visible_rows ? visible_rows - 1 : (ssize_t)bottom_line_count);
    ssize_t first_row = cursor_row - (visible_rows - bottom_rows - 1);
    if (first_row < 0) {
        first_row = 0;
    }

    const ssize_t max_first_row = row_count - visible_rows;
    if (first_row > max_first_row) {
        first_row = max_first_row;
    }

    editline_viewport_t viewport = {
        .first_row = first_row,
        .last_row = first_row + visible_rows - 1,
    };
    return viewport;
}

#endif  // IC_EDITLINE_VIEWPORT_H

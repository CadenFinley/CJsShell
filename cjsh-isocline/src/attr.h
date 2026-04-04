/*
  attr.h

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

#pragma once
#ifndef IC_ATTR_H
#define IC_ATTR_H

#include "common.h"
#include "stringbuf.h"

//-------------------------------------------------------------
// text attributes
//-------------------------------------------------------------

#define IC_ON (1)
#define IC_OFF (-1)
#define IC_NONE (0)

// Each color channel is stored separately to allow underline specific coloring.
typedef struct attr_s {
    struct {
        ic_color_t color;
        signed int bold;
        signed int reverse;
        ic_color_t bgcolor;
        signed int underline;
        signed int italic;
        ic_color_t underline_color;
    } x;
} attr_t;

ic_private attr_t attr_none(void);
ic_private attr_t attr_default(void);

ic_private bool attr_is_none(attr_t attr);
ic_private bool attr_is_eq(attr_t attr1, attr_t attr2);

ic_private attr_t attr_update_with(attr_t attr, attr_t newattr);

ic_private attr_t attr_from_sgr(const char* s, ssize_t len);
ic_private attr_t attr_from_esc_sgr(const char* s, ssize_t len);

//-------------------------------------------------------------
// attribute buffer used for rich rendering
//-------------------------------------------------------------

struct attrbuf_s;
typedef struct attrbuf_s attrbuf_t;

ic_private attrbuf_t* attrbuf_new(alloc_t* mem);
ic_private void attrbuf_free(attrbuf_t* ab);    // ab can be NULL
ic_private void attrbuf_clear(attrbuf_t* ab);   // ab can be NULL
ic_private ssize_t attrbuf_len(attrbuf_t* ab);  // ab can be NULL
ic_private const attr_t* attrbuf_attrs(attrbuf_t* ab, ssize_t expected_len);
ic_private ssize_t attrbuf_append_n(stringbuf_t* sb, attrbuf_t* ab, const char* s, ssize_t len,
                                    attr_t attr);

ic_private void attrbuf_set_at(attrbuf_t* ab, ssize_t pos, ssize_t count, attr_t attr);
ic_private void attrbuf_update_at(attrbuf_t* ab, ssize_t pos, ssize_t count, attr_t attr);
ic_private void attrbuf_insert_at(attrbuf_t* ab, ssize_t pos, ssize_t count, attr_t attr);

ic_private attr_t attrbuf_attr_at(attrbuf_t* ab, ssize_t pos);
ic_private void attrbuf_delete_at(attrbuf_t* ab, ssize_t pos, ssize_t count);

#endif  // IC_ATTR_H

/* ----------------------------------------------------------------------------
  Copyright (c) 2021, Daan Leijen
  Largely Modified by Caden Finley 2025 for CJ's Shell
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License. A copy of the license can be
  found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/
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
ic_private attr_t attr_from_color(ic_color_t color);

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

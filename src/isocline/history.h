/* ----------------------------------------------------------------------------
  Copyright (c) 2021, Daan Leijen
  Largely Modified by Caden Finley 2025 for CJ's Shell
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License.

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
-----------------------------------------------------------------------------*/
#pragma once
#ifndef IC_HISTORY_H
#define IC_HISTORY_H

#include <time.h>

#include "common.h"

//-------------------------------------------------------------
// History
//-------------------------------------------------------------

struct history_s;
typedef struct history_s history_t;

#define IC_HISTORY_EXIT_CODE_UNKNOWN (-1)

typedef struct history_entry_s {
    char* command;
    int exit_code;
    time_t timestamp;
} history_entry_t;

typedef struct history_snapshot_s {
    history_entry_t* entries;
    ssize_t count;
    ssize_t capacity;
} history_snapshot_t;

ic_private history_t* history_new(alloc_t* mem);
ic_private void history_free(history_t* h);
ic_private void history_clear(history_t* h);
ic_private bool history_enable_duplicates(history_t* h, bool enable);
ic_private bool history_set_fuzzy_case_sensitive(history_t* h, bool enable);
ic_private bool history_is_fuzzy_case_sensitive(const history_t* h);
ic_private ssize_t history_count(const history_t* h);

ic_private void history_load_from(history_t* h, const char* fname, long max_entries);
ic_private void history_load(history_t* h);
ic_private void history_save(const history_t* h);

ic_private bool history_push(history_t* h, const char* entry);
ic_private bool history_push_with_exit_code(history_t* h, const char* entry, int exit_code);
ic_private bool history_update(history_t* h, const char* entry);
ic_private const char* history_get(const history_t* h, ssize_t n);
ic_private void history_remove_last(history_t* h);

ic_private bool history_search(const history_t* h, ssize_t from, const char* search, bool backward,
                               ssize_t* hidx, ssize_t* hpos);

ic_private bool history_search_prefix(const history_t* h, ssize_t from, const char* prefix,
                                      bool backward, ssize_t* hidx);

ic_private bool history_snapshot_load(history_t* h, history_snapshot_t* snap, bool dedup);
ic_private void history_snapshot_free(history_t* h, history_snapshot_t* snap);
ic_private const history_entry_t* history_snapshot_get(const history_snapshot_t* snap, ssize_t n);
ic_private ssize_t history_snapshot_count(const history_snapshot_t* snap);

typedef struct history_match_s {
    ssize_t hidx;       // history index
    int score;          // match score (higher is better)
    ssize_t match_pos;  // position of first match
    ssize_t match_len;  // length of match
} history_match_t;

ic_private bool history_fuzzy_search(const history_t* h, const char* query,
                                     history_match_t* matches, ssize_t max_matches,
                                     ssize_t* match_count, bool* exit_filter_applied,
                                     int* exit_filter_value);

ic_private bool history_fuzzy_search_with_case(const history_t* h, const char* query,
                                               history_match_t* matches, ssize_t max_matches,
                                               ssize_t* match_count, bool* exit_filter_applied,
                                               int* exit_filter_value, bool case_sensitive);

#endif  // IC_HISTORY_H

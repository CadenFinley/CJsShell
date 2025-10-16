/* ----------------------------------------------------------------------------
  Copyright (c) 2021, Daan Leijen
  Largely Modified by Caden Finley 2025 for CJ's Shell
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License. A copy of the license can be
  found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/
#pragma once
#ifndef IC_HISTORY_H
#define IC_HISTORY_H

#include "common.h"

//-------------------------------------------------------------
// History
//-------------------------------------------------------------

struct history_s;
typedef struct history_s history_t;

typedef struct history_snapshot_s {
    char** entries;
    ssize_t count;
    ssize_t capacity;
} history_snapshot_t;

ic_private history_t* history_new(alloc_t* mem);
ic_private void history_free(history_t* h);
ic_private void history_clear(history_t* h);
ic_private bool history_enable_duplicates(history_t* h, bool enable);
ic_private ssize_t history_count(const history_t* h);

ic_private void history_load_from(history_t* h, const char* fname, long max_entries);
ic_private void history_load(history_t* h);
ic_private void history_save(const history_t* h);

ic_private bool history_push(history_t* h, const char* entry);
ic_private bool history_update(history_t* h, const char* entry);
ic_private const char* history_get(const history_t* h, ssize_t n);
ic_private void history_remove_last(history_t* h);

ic_private bool history_search(const history_t* h, ssize_t from, const char* search, bool backward,
                               ssize_t* hidx, ssize_t* hpos);

ic_private bool history_search_prefix(const history_t* h, ssize_t from, const char* prefix,
                                      bool backward, ssize_t* hidx);

ic_private bool history_snapshot_load(history_t* h, history_snapshot_t* snap, bool dedup);
ic_private void history_snapshot_free(history_t* h, history_snapshot_t* snap);
ic_private const char* history_snapshot_get(const history_snapshot_t* snap, ssize_t n);
ic_private ssize_t history_snapshot_count(const history_snapshot_t* snap);

typedef struct history_match_s {
    ssize_t hidx;       // history index
    int score;          // match score (higher is better)
    ssize_t match_pos;  // position of first match
    ssize_t match_len;  // length of match
} history_match_t;

ic_private bool history_fuzzy_search(const history_t* h, const char* query,
                                     history_match_t* matches, ssize_t max_matches,
                                     ssize_t* match_count);

#endif  // IC_HISTORY_H

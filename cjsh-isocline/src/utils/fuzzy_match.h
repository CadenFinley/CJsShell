/*
  fuzzy_match.h

  Shared fuzzy matching helpers for isocline menus.

  MIT License
*/

#pragma once
#ifndef IC_FUZZY_MATCH_H
#define IC_FUZZY_MATCH_H

#include <stdbool.h>
#include <stddef.h>

#include "common.h"

ic_private bool ic_fuzzy_char_equals(char left, char right, bool case_sensitive);
ic_private bool ic_fuzzy_find_substring(const char* haystack, const char* needle,
                                        bool case_sensitive, ssize_t* pos_out, ssize_t* len_out);
ic_private int ic_fuzzy_match_score(const char* entry, const char* query, ssize_t* match_pos,
                                    ssize_t* match_len, bool case_sensitive);
ic_private bool ic_fuzzy_next_token(const char** cursor, const char** token_start,
                                    size_t* token_len);
ic_private bool ic_fuzzy_trim_token(const char** token_start, size_t* token_len);

#endif  // IC_FUZZY_MATCH_H

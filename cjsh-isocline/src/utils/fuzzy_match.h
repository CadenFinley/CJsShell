/*
  fuzzy_match.h

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

/* Shared fuzzy matching helpers for isocline menus. */

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

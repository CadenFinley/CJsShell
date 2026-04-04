/*
  prompt_line_replacement.h

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
#ifndef IC_PROMPT_LINE_REPLACEMENT_H
#define IC_PROMPT_LINE_REPLACEMENT_H

#include "common.h"

typedef struct ic_prompt_line_replacement_state_s {
    bool replace_prompt_line_with_line_number;
    bool prompt_has_prefix_lines;
    bool prompt_begins_with_newline;
    bool line_numbers_enabled;
    bool input_has_content;
} ic_prompt_line_replacement_state_t;

ic_private bool ic_prompt_line_replacement_should_activate(
    const ic_prompt_line_replacement_state_t* state);

#endif  // IC_PROMPT_LINE_REPLACEMENT_H

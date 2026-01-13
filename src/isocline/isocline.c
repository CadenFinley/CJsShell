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

//-------------------------------------------------------------
// Single-translation-unit aggregation helper
//-------------------------------------------------------------
#if !defined(IC_SEPARATE_OBJS)
#ifndef _CRT_NONSTDC_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS  // for msvc
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS  // for msvc
#endif
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE
#include "attr.c"
#include "bbcode.c"
#include "common.c"
#include "completers.c"
#include "completions.c"
#include "editline.c"
#include "highlight.c"
#include "history.c"
#include "isocline_env.c"
#include "isocline_keybindings.c"
#include "isocline_options.c"
#include "isocline_print.c"
#include "isocline_readline.c"
#include "isocline_terminal.c"
#include "stringbuf.c"
#include "term.c"
#include "tty.c"
#include "tty_esc.c"
#include "undo.c"
#include "unicode.c"
#else
static void ic_isocline_translation_unit_anchor(void) {
}
#endif

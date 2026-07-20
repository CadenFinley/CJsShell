/*
  isocline.c

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
#include "../terminal/attr.c"
#include "../terminal/bbcode.c"
#include "common.c"
#include "../completion/completers.c"
#include "../completion/completions.c"
#include "../edit/editline.c"
#include "../completion/highlight.c"
#include "../edit/history.c"
#include "isocline_env.c"
#include "../keybinding/isocline_keybindings.c"
#include "isocline_options.c"
#include "../terminal/isocline_print.c"
#include "../edit/isocline_readline.c"
#include "../terminal/isocline_terminal.c"
#include "../edit/prompt_line_replacement.c"
#include "../utils/stringbuf.c"
#include "../terminal/term.c"
#include "../terminal/tty.c"
#include "../terminal/tty_esc.c"
#include "../edit/undo.c"
#include "../terminal/unicode.c"
#else
#if defined(__GNUC__) || defined(__clang__)
#define IC_ANCHOR_UNUSED __attribute__((unused))
#else
#define IC_ANCHOR_UNUSED
#endif
static void IC_ANCHOR_UNUSED ic_isocline_translation_unit_anchor(void) {
}
#undef IC_ANCHOR_UNUSED
#endif

/* ----------------------------------------------------------------------------
  Copyright (c) 2021, Daan Leijen
  Largely Modified by Caden Finley 2025 for CJ's Shell
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License. A copy of the license can be
  found in the "LICENSE" file at the root of this distribution.
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
#endif

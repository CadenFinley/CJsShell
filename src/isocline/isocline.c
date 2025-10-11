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
#include "isocline/attr.c"
#include "isocline/bbcode.c"
#include "isocline/common.c"
#include "isocline/completers.c"
#include "isocline/completions.c"
#include "isocline/editline.c"
#include "isocline/highlight.c"
#include "isocline/history.c"
#include "isocline/isocline_env.c"
#include "isocline/isocline_keybindings.c"
#include "isocline/isocline_options.c"
#include "isocline/isocline_print.c"
#include "isocline/isocline_readline.c"
#include "isocline/isocline_terminal.c"
#include "isocline/stringbuf.c"
#include "isocline/term.c"
#include "isocline/tty.c"
#include "isocline/tty_esc.c"
#include "isocline/undo.c"
#endif

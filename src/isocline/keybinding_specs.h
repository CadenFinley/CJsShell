/*
  keybinding_specs.h

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
#ifndef CJSH_KEYBINDING_SPECS_H
#define CJSH_KEYBINDING_SPECS_H

// Emacs-style keybindings, this is the default

#ifdef __APPLE__
#define SPEC_CURSOR_WORD_PREV "shift+left|alt+b"
#define SPEC_CURSOR_WORD_NEXT "shift+right|alt+f"
#define SPEC_INSERT_NEWLINE "shift+tab|ctrl+j"
#else
#define SPEC_CURSOR_WORD_PREV "ctrl+left|alt+b"
#define SPEC_CURSOR_WORD_NEXT "ctrl+right|alt+f"
#define SPEC_INSERT_NEWLINE "ctrl+enter|ctrl+j"
#endif

#define SPEC_CURSOR_LEFT "left|ctrl+b"
#define SPEC_CURSOR_RIGHT "right|ctrl+f"
#define SPEC_CURSOR_UP "up"
#define SPEC_CURSOR_DOWN "down"
#define SPEC_CURSOR_LINE_START "home|ctrl+a"
#define SPEC_CURSOR_LINE_END "end|ctrl+e"
#define SPEC_CURSOR_INPUT_START "ctrl+home|shift+home|pageup|alt+<"
#define SPEC_CURSOR_INPUT_END "ctrl+end|shift+end|pagedown|alt+>"
#define SPEC_CURSOR_MATCH_BRACE "alt+m"
#define SPEC_HISTORY_PREV "ctrl+p"
#define SPEC_HISTORY_NEXT "ctrl+n"
#define SPEC_HISTORY_SEARCH "ctrl+r|ctrl+s"
#define SPEC_DELETE_FORWARD "delete|ctrl+d"
#define SPEC_DELETE_BACKWARD "backspace|ctrl+h"
#define SPEC_DELETE_WORD_END "alt+d"
#define SPEC_DELETE_WORD_START_WS "ctrl+w"
#define SPEC_DELETE_WORD_START "alt+backspace|alt+delete"
#define SPEC_DELETE_LINE_START "ctrl+u"
#define SPEC_DELETE_LINE_END "ctrl+k"
#define SPEC_TRANSPOSE "ctrl+t"
#define SPEC_CLEAR_SCREEN "ctrl+l"
#define SPEC_UNDO "ctrl+z|ctrl+_"
#define SPEC_REDO "ctrl+y"
#define SPEC_COMPLETE "tab|alt+?"
#define SPEC_SHOW_HELP "f1"

#endif  // CJSH_KEYBINDING_SPECS_H

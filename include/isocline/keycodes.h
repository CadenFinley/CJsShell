/* ----------------------------------------------------------------------------
  Copyright (c) 2021, Daan Leijen
  Largely Modified by Caden Finley 2025 for CJ's Shell
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License. A copy of the license can be
  found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/
#pragma once
#ifndef IC_KEYCODES_H
#define IC_KEYCODES_H

#include <stdint.h>

// Key code
typedef uint32_t ic_keycode_t;

// Modifier masks
#define IC_KEY_MOD_SHIFT (0x10000000U)
#define IC_KEY_MOD_ALT (0x20000000U)
#define IC_KEY_MOD_CTRL (0x40000000U)

// Helpers to compose key codes with modifiers
#define IC_KEY_WITH_SHIFT(x) ((x) | IC_KEY_MOD_SHIFT)
#define IC_KEY_WITH_ALT(x) ((x) | IC_KEY_MOD_ALT)
#define IC_KEY_WITH_CTRL(x) ((x) | IC_KEY_MOD_CTRL)

#define IC_KEY_NO_MODS(k) ((k) & 0x0FFFFFFFU)
#define IC_KEY_MODS(k) ((k) & 0xF0000000U)

// Basic control characters and ASCII control combinations
#define IC_KEY_NONE (0)
#define IC_KEY_CTRL_A (1)
#define IC_KEY_CTRL_B (2)
#define IC_KEY_CTRL_C (3)
#define IC_KEY_CTRL_D (4)
#define IC_KEY_CTRL_E (5)
#define IC_KEY_CTRL_F (6)
#define IC_KEY_BELL (7)
#define IC_KEY_BACKSP (8)
#define IC_KEY_TAB (9)
#define IC_KEY_LINEFEED (10)
#define IC_KEY_CTRL_K (11)
#define IC_KEY_CTRL_L (12)
#define IC_KEY_ENTER (13)
#define IC_KEY_CTRL_N (14)
#define IC_KEY_CTRL_O (15)
#define IC_KEY_CTRL_P (16)
#define IC_KEY_CTRL_Q (17)
#define IC_KEY_CTRL_R (18)
#define IC_KEY_CTRL_S (19)
#define IC_KEY_CTRL_T (20)
#define IC_KEY_CTRL_U (21)
#define IC_KEY_CTRL_V (22)
#define IC_KEY_CTRL_W (23)
#define IC_KEY_CTRL_X (24)
#define IC_KEY_CTRL_Y (25)
#define IC_KEY_CTRL_Z (26)
#define IC_KEY_ESC (27)
#define IC_KEY_SPACE (32)
#define IC_KEY_RUBOUT (127)
#define IC_KEY_UNICODE_MAX (0x0010FFFFU)

// Virtual keys
#define IC_KEY_VIRT (0x01000000U)
#define IC_KEY_UP (IC_KEY_VIRT + 0)
#define IC_KEY_DOWN (IC_KEY_VIRT + 1)
#define IC_KEY_LEFT (IC_KEY_VIRT + 2)
#define IC_KEY_RIGHT (IC_KEY_VIRT + 3)
#define IC_KEY_HOME (IC_KEY_VIRT + 4)
#define IC_KEY_END (IC_KEY_VIRT + 5)
#define IC_KEY_DEL (IC_KEY_VIRT + 6)
#define IC_KEY_PAGEUP (IC_KEY_VIRT + 7)
#define IC_KEY_PAGEDOWN (IC_KEY_VIRT + 8)
#define IC_KEY_INS (IC_KEY_VIRT + 9)

#define IC_KEY_F1 (IC_KEY_VIRT + 11)
#define IC_KEY_F2 (IC_KEY_VIRT + 12)
#define IC_KEY_F3 (IC_KEY_VIRT + 13)
#define IC_KEY_F4 (IC_KEY_VIRT + 14)
#define IC_KEY_F5 (IC_KEY_VIRT + 15)
#define IC_KEY_F6 (IC_KEY_VIRT + 16)
#define IC_KEY_F7 (IC_KEY_VIRT + 17)
#define IC_KEY_F8 (IC_KEY_VIRT + 18)
#define IC_KEY_F9 (IC_KEY_VIRT + 19)
#define IC_KEY_F10 (IC_KEY_VIRT + 20)
#define IC_KEY_F11 (IC_KEY_VIRT + 21)
#define IC_KEY_F12 (IC_KEY_VIRT + 22)
#define IC_KEY_F(n) (IC_KEY_F1 + (n) - 1)

// Event codes
#define IC_KEY_EVENT_BASE (0x02000000U)
#define IC_KEY_EVENT_RESIZE (IC_KEY_EVENT_BASE + 1)
#define IC_KEY_EVENT_AUTOTAB (IC_KEY_EVENT_BASE + 2)
#define IC_KEY_EVENT_STOP (IC_KEY_EVENT_BASE + 3)

// Convenience macros (mirroring legacy names)
#define IC_KEY_CTRL_UP (IC_KEY_WITH_CTRL(IC_KEY_UP))
#define IC_KEY_CTRL_DOWN (IC_KEY_WITH_CTRL(IC_KEY_DOWN))
#define IC_KEY_CTRL_LEFT (IC_KEY_WITH_CTRL(IC_KEY_LEFT))
#define IC_KEY_CTRL_RIGHT (IC_KEY_WITH_CTRL(IC_KEY_RIGHT))
#define IC_KEY_CTRL_HOME (IC_KEY_WITH_CTRL(IC_KEY_HOME))
#define IC_KEY_CTRL_END (IC_KEY_WITH_CTRL(IC_KEY_END))
#define IC_KEY_CTRL_DEL (IC_KEY_WITH_CTRL(IC_KEY_DEL))
#define IC_KEY_CTRL_PAGEUP (IC_KEY_WITH_CTRL(IC_KEY_PAGEUP))
#define IC_KEY_CTRL_PAGEDOWN (IC_KEY_WITH_CTRL(IC_KEY_PAGEDOWN))
#define IC_KEY_CTRL_INS (IC_KEY_WITH_CTRL(IC_KEY_INS))

#define IC_KEY_SHIFT_TAB (IC_KEY_WITH_SHIFT(IC_KEY_TAB))

// Helper inline constructors matching historical helpers
static inline ic_keycode_t ic_key_char(char c) {
    return (uint8_t)c;
}

static inline ic_keycode_t ic_key_unicode(uint32_t u) {
    return u;
}

#endif  // IC_KEYCODES_H

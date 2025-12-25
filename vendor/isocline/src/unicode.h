/* ----------------------------------------------------------------------------
  Copyright (c) 2021, Daan Leijen
  Largely Modified by Caden Finley 2025 for CJ's Shell
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License. A copy of the license can be
  found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/

#pragma once
#ifndef IC_UNICODE_H
#define IC_UNICODE_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t unicode_codepoint_t;

// Core UTF-8 encoding/decoding functions
bool unicode_decode_utf8(const uint8_t* data, ssize_t length, unicode_codepoint_t* codepoint,
                         ssize_t* bytes_read);
int unicode_encode_utf8(unicode_codepoint_t codepoint, uint8_t out[4]);
bool unicode_is_valid_codepoint(unicode_codepoint_t codepoint);

// Character width and properties
int unicode_codepoint_width(unicode_codepoint_t codepoint);
bool unicode_is_combining_codepoint(unicode_codepoint_t codepoint);
bool unicode_is_control_codepoint(unicode_codepoint_t codepoint);

// Display width calculation
// Calculates the display width of a UTF-8 string with ANSI escape sequences
// If count_ansi_chars is not NULL, it will be set to the number of ANSI escape characters
// If count_visible_chars is not NULL, it will be set to the number of visible characters
size_t unicode_calculate_display_width(const char* str, size_t str_len, size_t* count_ansi_chars,
                                       size_t* count_visible_chars);

// Calculates the display width of a UTF-8 string without ANSI escape sequences
size_t unicode_calculate_utf8_width(const char* str, size_t str_len);

#ifdef __cplusplus
}
#endif

#endif  // IC_UNICODE_H

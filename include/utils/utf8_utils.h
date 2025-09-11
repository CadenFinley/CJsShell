#pragma once

#include <cstdint>
#include <string>

extern "C" {
#include <utf8proc.h>
}

namespace utf8_utils {

/**
 * Calculate the display width of a UTF-8 string, properly handling:
 * - ANSI escape sequences (excluded from width calculation)
 * - Unicode characters with proper width calculation
 * - Emoji and wide characters
 * - Control characters
 *
 * @param str The UTF-8 string to measure
 * @param count_ansi_chars If not null, will be set to the number of ANSI escape
 * characters
 * @param count_visible_chars If not null, will be set to the number of visible
 * characters
 * @return The display width in terminal columns
 */
size_t calculate_display_width(const std::string& str,
                               size_t* count_ansi_chars = nullptr,
                               size_t* count_visible_chars = nullptr);

/**
 * Calculate the display width of a UTF-8 string without ANSI sequences
 *
 * @param str The UTF-8 string to measure (should not contain ANSI sequences)
 * @return The display width in terminal columns
 */
size_t calculate_utf8_width(const std::string& str);

/**
 * Get the width of a single Unicode codepoint
 *
 * @param codepoint The Unicode codepoint
 * @return The display width (0, 1, or 2)
 */
int get_codepoint_width(utf8proc_int32_t codepoint);

/**
 * Check if a codepoint is a control character
 *
 * @param codepoint The Unicode codepoint
 * @return true if it's a control character
 */
bool is_control_character(utf8proc_int32_t codepoint);

/**
 * Check if a codepoint is a combining character
 *
 * @param codepoint The Unicode codepoint
 * @return true if it's a combining character
 */
bool is_combining_character(utf8proc_int32_t codepoint);

/**
 * Convert a UTF-8 string to lowercase using Unicode-aware case folding
 *
 * @param str The input UTF-8 string
 * @return The lowercase version of the string
 */
std::string to_lowercase(const std::string& str);

/**
 * Convert a UTF-8 string to uppercase using Unicode-aware case conversion
 *
 * @param str The input UTF-8 string
 * @return The uppercase version of the string
 */
std::string to_uppercase(const std::string& str);

/**
 * Normalize a UTF-8 string using NFC (Canonical Composition)
 *
 * @param str The input UTF-8 string
 * @return The normalized string
 */
std::string normalize_nfc(const std::string& str);

/**
 * Check if a Unicode grapheme boundary exists between two codepoints
 *
 * @param cp1 First codepoint
 * @param cp2 Second codepoint
 * @return true if a grapheme boundary exists
 */
bool is_grapheme_boundary(utf8proc_int32_t cp1, utf8proc_int32_t cp2);

}  // namespace utf8_utils

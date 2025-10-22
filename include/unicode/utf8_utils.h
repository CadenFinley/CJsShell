#pragma once

#include <string>

// UTF-8 utilities now provided by isocline's unicode module
// This header provides a C++ interface to those functions

namespace utf8_utils {

// Calculate the display width of a string, accounting for ANSI escape sequences
// and multi-byte UTF-8 characters
size_t calculate_display_width(const std::string& str, size_t* count_ansi_chars = nullptr,
                               size_t* count_visible_chars = nullptr);

// Calculate the display width of a UTF-8 string (no ANSI sequences)
size_t calculate_utf8_width(const std::string& str);

// Get the display width of a single Unicode codepoint
int get_codepoint_width(uint32_t codepoint);

// Check if a codepoint is a control character
bool is_control_character(uint32_t codepoint);

// Check if a codepoint is a combining character
bool is_combining_character(uint32_t codepoint);

// Convert string to lowercase (basic ASCII only)
std::string to_lowercase(const std::string& str);

// Convert string to uppercase (basic ASCII only)
std::string to_uppercase(const std::string& str);

// NFC normalization (currently returns input as-is)
std::string normalize_nfc(const std::string& str);

// Check if there's a grapheme boundary between two codepoints
bool is_grapheme_boundary(uint32_t cp1, uint32_t cp2);

}  // namespace utf8_utils

#pragma once

#include <string>

#include "utils/unicode_support.h"

namespace utf8_utils {

size_t calculate_display_width(const std::string& str, size_t* count_ansi_chars = nullptr,
                               size_t* count_visible_chars = nullptr);

size_t calculate_utf8_width(const std::string& str);

int get_codepoint_width(unicode_codepoint_t codepoint);

bool is_control_character(unicode_codepoint_t codepoint);

bool is_combining_character(unicode_codepoint_t codepoint);

std::string to_lowercase(const std::string& str);

std::string to_uppercase(const std::string& str);

std::string normalize_nfc(const std::string& str);

bool is_grapheme_boundary(unicode_codepoint_t cp1, unicode_codepoint_t cp2);

}  // namespace utf8_utils

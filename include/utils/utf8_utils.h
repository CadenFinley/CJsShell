#pragma once

#include <cstdint>
#include <string>

extern "C" {
#include <utf8proc.h>
}

namespace utf8_utils {

size_t calculate_display_width(const std::string& str,
                               size_t* count_ansi_chars = nullptr,
                               size_t* count_visible_chars = nullptr);

size_t calculate_utf8_width(const std::string& str);

int get_codepoint_width(utf8proc_int32_t codepoint);

bool is_control_character(utf8proc_int32_t codepoint);

bool is_combining_character(utf8proc_int32_t codepoint);

std::string to_lowercase(const std::string& str);

std::string to_uppercase(const std::string& str);

std::string normalize_nfc(const std::string& str);

bool is_grapheme_boundary(utf8proc_int32_t cp1, utf8proc_int32_t cp2);

}  // namespace utf8_utils

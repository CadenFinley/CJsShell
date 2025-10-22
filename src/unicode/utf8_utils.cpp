#include "utf8_utils.h"

#include <cstring>

// Include isocline's unicode module
extern "C" {
#include "isocline/unicode.h"
}

namespace utf8_utils {

size_t calculate_display_width(const std::string& str, size_t* count_ansi_chars,
                               size_t* count_visible_chars) {
    return unicode_calculate_display_width(str.c_str(), str.length(), count_ansi_chars,
                                           count_visible_chars);
}

size_t calculate_utf8_width(const std::string& str) {
    return unicode_calculate_utf8_width(str.c_str(), str.length());
}

int get_codepoint_width(uint32_t codepoint) {
    return unicode_codepoint_width(codepoint);
}

bool is_control_character(uint32_t codepoint) {
    return unicode_is_control_codepoint(codepoint);
}

bool is_combining_character(uint32_t codepoint) {
    return unicode_is_combining_codepoint(codepoint);
}

std::string to_lowercase(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    const uint8_t* data = reinterpret_cast<const uint8_t*>(str.c_str());
    ssize_t len = static_cast<ssize_t>(str.length());
    ssize_t pos = 0;

    while (pos < len) {
        uint32_t codepoint = 0;
        ssize_t bytes_read = 0;
        bool ok = unicode_decode_utf8(data + pos, len - pos, &codepoint, &bytes_read);
        if (!ok || bytes_read <= 0) {
            result.push_back(static_cast<char>(data[pos]));
            pos += (bytes_read > 0) ? bytes_read : 1;
            continue;
        }

        if (codepoint >= 'A' && codepoint <= 'Z') {
            codepoint = static_cast<uint32_t>(codepoint + 32);
        }

        uint8_t buffer[4] = {0};
        int written = unicode_encode_utf8(codepoint, buffer);
        if (written <= 0) {
            result.append(str, static_cast<size_t>(pos), static_cast<size_t>(bytes_read));
        } else {
            result.append(reinterpret_cast<char*>(buffer), static_cast<size_t>(written));
        }

        pos += bytes_read;
    }

    return result;
}

std::string to_uppercase(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    const uint8_t* data = reinterpret_cast<const uint8_t*>(str.c_str());
    ssize_t len = static_cast<ssize_t>(str.length());
    ssize_t pos = 0;

    while (pos < len) {
        uint32_t codepoint = 0;
        ssize_t bytes_read = 0;
        bool ok = unicode_decode_utf8(data + pos, len - pos, &codepoint, &bytes_read);
        if (!ok || bytes_read <= 0) {
            result.push_back(static_cast<char>(data[pos]));
            pos += (bytes_read > 0) ? bytes_read : 1;
            continue;
        }

        if (codepoint >= 'a' && codepoint <= 'z') {
            codepoint = static_cast<uint32_t>(codepoint - 32);
        }

        uint8_t buffer[4] = {0};
        int written = unicode_encode_utf8(codepoint, buffer);
        if (written <= 0) {
            result.append(str, static_cast<size_t>(pos), static_cast<size_t>(bytes_read));
        } else {
            result.append(reinterpret_cast<char*>(buffer), static_cast<size_t>(written));
        }

        pos += bytes_read;
    }

    return result;
}

std::string normalize_nfc(const std::string& str) {
    // NFC normalization requires full Unicode decomposition and composition.
    // Until a dedicated normalization routine is added, return the input.
    return str;
}

bool is_grapheme_boundary(uint32_t /*cp1*/, uint32_t cp2) {
    return !unicode_is_combining_codepoint(cp2);
}

}  // namespace utf8_utils

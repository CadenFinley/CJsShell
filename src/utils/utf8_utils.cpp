#include "utf8_utils.h"
#include <cstring>
extern "C" {
#include "isocline/unicode.h"
}

namespace utf8_utils {

namespace {
template <typename Transform>
std::string transform_utf8_ascii(const std::string& str, Transform&& transform) {
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

        uint32_t transformed = transform(codepoint);

        if (transformed == codepoint) {
            result.append(str, static_cast<size_t>(pos), static_cast<size_t>(bytes_read));
        } else {
            uint8_t buffer[4] = {0};
            int written = unicode_encode_utf8(transformed, buffer);
            if (written <= 0) {
                result.append(str, static_cast<size_t>(pos), static_cast<size_t>(bytes_read));
            } else {
                result.append(reinterpret_cast<char*>(buffer), static_cast<size_t>(written));
            }
        }

        pos += bytes_read;
    }

    return result;
}
}  // namespace

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
    return transform_utf8_ascii(str, [](uint32_t codepoint) {
        if (codepoint >= 'A' && codepoint <= 'Z') {
            return static_cast<uint32_t>(codepoint + 32);
        }
        return codepoint;
    });
}

std::string to_uppercase(const std::string& str) {
    return transform_utf8_ascii(str, [](uint32_t codepoint) {
        if (codepoint >= 'a' && codepoint <= 'z') {
            return static_cast<uint32_t>(codepoint - 32);
        }
        return codepoint;
    });
}

bool is_grapheme_boundary(uint32_t /*cp1*/, uint32_t cp2) {
    return !unicode_is_combining_codepoint(cp2);
}

}  // namespace utf8_utils

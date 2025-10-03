#include "utils/utf8_utils.h"

#include <cstring>

namespace utf8_utils {

size_t calculate_display_width(const std::string& str, size_t* count_ansi_chars,
                               size_t* count_visible_chars) {
    size_t display_width = 0;
    size_t ansi_chars = 0;
    size_t visible_chars = 0;

    const uint8_t* data = reinterpret_cast<const uint8_t*>(str.c_str());
    ssize_t len = static_cast<ssize_t>(str.length());
    ssize_t pos = 0;

    while (pos < len) {
        if (data[pos] == '\033' && pos + 1 < len) {
            pos++;
            ansi_chars++;

            if (data[pos] == '[') {
                pos++;
                ansi_chars++;

                while (pos < len && ((data[pos] >= '0' && data[pos] <= '9') || data[pos] == ';' ||
                                     data[pos] == ':' || data[pos] == '<' || data[pos] == '=' ||
                                     data[pos] == '>' || data[pos] == '?')) {
                    pos++;
                    ansi_chars++;
                }

                if (pos < len) {
                    pos++;
                    ansi_chars++;
                }
            } else if (data[pos] == ']') {
                pos++;
                ansi_chars++;

                while (pos < len) {
                    if (data[pos] == '\007') {
                        pos++;
                        ansi_chars++;
                        break;
                    }
                    if (data[pos] == '\033' && pos + 1 < len && data[pos + 1] == '\\') {
                        pos += 2;
                        ansi_chars += 2;
                        break;
                    }
                    pos++;
                    ansi_chars++;
                }
            } else {
                pos++;
                ansi_chars++;
            }
            continue;
        }

        unicode_codepoint_t codepoint = 0;
        ssize_t bytes_read = 0;
        bool ok = unicode_decode_utf8(data + pos, len - pos, &codepoint, &bytes_read);
        if (!ok || bytes_read <= 0) {
            ++display_width;
            ++visible_chars;
            pos += (bytes_read > 0) ? bytes_read : 1;
            continue;
        }

        int char_width = unicode_codepoint_width(codepoint);
        if (char_width > 0) {
            display_width += static_cast<size_t>(char_width);
            ++visible_chars;
        }

        pos += bytes_read;
    }

    if (count_ansi_chars != nullptr) {
        *count_ansi_chars = ansi_chars;
    }
    if (count_visible_chars != nullptr) {
        *count_visible_chars = visible_chars;
    }

    return display_width;
}

size_t calculate_utf8_width(const std::string& str) {
    size_t width = 0;

    const uint8_t* data = reinterpret_cast<const uint8_t*>(str.c_str());
    ssize_t len = static_cast<ssize_t>(str.length());
    ssize_t pos = 0;

    while (pos < len) {
        unicode_codepoint_t codepoint = 0;
        ssize_t bytes_read = 0;
        bool ok = unicode_decode_utf8(data + pos, len - pos, &codepoint, &bytes_read);
        if (!ok || bytes_read <= 0) {
            ++width;
            pos += (bytes_read > 0) ? bytes_read : 1;
            continue;
        }

        int char_width = unicode_codepoint_width(codepoint);
        width += (char_width > 0) ? static_cast<size_t>(char_width) : 0;

        pos += bytes_read;
    }

    return width;
}

int get_codepoint_width(unicode_codepoint_t codepoint) {
    return unicode_codepoint_width(codepoint);
}

bool is_control_character(unicode_codepoint_t codepoint) {
    return unicode_is_control_codepoint(codepoint);
}

bool is_combining_character(unicode_codepoint_t codepoint) {
    return unicode_is_combining_codepoint(codepoint);
}

std::string to_lowercase(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    const uint8_t* data = reinterpret_cast<const uint8_t*>(str.c_str());
    ssize_t len = static_cast<ssize_t>(str.length());
    ssize_t pos = 0;

    while (pos < len) {
        unicode_codepoint_t codepoint = 0;
        ssize_t bytes_read = 0;
        bool ok = unicode_decode_utf8(data + pos, len - pos, &codepoint, &bytes_read);
        if (!ok || bytes_read <= 0) {
            result.push_back(static_cast<char>(data[pos]));
            pos += (bytes_read > 0) ? bytes_read : 1;
            continue;
        }

        if (codepoint >= 'A' && codepoint <= 'Z') {
            codepoint = static_cast<unicode_codepoint_t>(codepoint + 32);
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
        unicode_codepoint_t codepoint = 0;
        ssize_t bytes_read = 0;
        bool ok = unicode_decode_utf8(data + pos, len - pos, &codepoint, &bytes_read);
        if (!ok || bytes_read <= 0) {
            result.push_back(static_cast<char>(data[pos]));
            pos += (bytes_read > 0) ? bytes_read : 1;
            continue;
        }

        if (codepoint >= 'a' && codepoint <= 'z') {
            codepoint = static_cast<unicode_codepoint_t>(codepoint - 32);
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

bool is_grapheme_boundary(unicode_codepoint_t /*cp1*/, unicode_codepoint_t cp2) {
    return !unicode_is_combining_codepoint(cp2);
}

}  // namespace utf8_utils

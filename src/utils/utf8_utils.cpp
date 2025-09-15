#include "utils/utf8_utils.h"
#include <cstring>
#include <memory>

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

        while (pos < len &&
               ((data[pos] >= '0' && data[pos] <= '9') || data[pos] == ';' ||
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

    utf8proc_int32_t codepoint;
    ssize_t bytes_read = utf8proc_iterate(data + pos, len - pos, &codepoint);

    if (bytes_read < 0) {
      display_width++;
      visible_chars++;
      pos++;
      continue;
    }

    int char_width = utf8proc_charwidth(codepoint);

    if (char_width >= 0) {
      display_width += static_cast<size_t>(char_width);
      if (char_width > 0) {
        visible_chars++;
      }
    } else {
      display_width++;
      visible_chars++;
    }

    pos += bytes_read;
  }

  if (count_ansi_chars) {
    *count_ansi_chars = ansi_chars;
  }
  if (count_visible_chars) {
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
    utf8proc_int32_t codepoint;
    ssize_t bytes_read = utf8proc_iterate(data + pos, len - pos, &codepoint);

    if (bytes_read < 0) {
      width++;
      pos++;
      continue;
    }

    int char_width = utf8proc_charwidth(codepoint);
    if (char_width >= 0) {
      width += static_cast<size_t>(char_width);
    } else {
      width++;
    }

    pos += bytes_read;
  }

  return width;
}

int get_codepoint_width(utf8proc_int32_t codepoint) {
  return utf8proc_charwidth(codepoint);
}

bool is_control_character(utf8proc_int32_t codepoint) {
  utf8proc_category_t category = utf8proc_category(codepoint);
  return category == UTF8PROC_CATEGORY_CC || category == UTF8PROC_CATEGORY_CF;
}

bool is_combining_character(utf8proc_int32_t codepoint) {
  utf8proc_category_t category = utf8proc_category(codepoint);
  return category == UTF8PROC_CATEGORY_MN || category == UTF8PROC_CATEGORY_MC ||
         category == UTF8PROC_CATEGORY_ME;
}

std::string to_lowercase(const std::string& str) {
  const uint8_t* input = reinterpret_cast<const uint8_t*>(str.c_str());
  uint8_t* result = nullptr;

  ssize_t result_len = utf8proc_map(
      input, static_cast<ssize_t>(str.length()), &result,
      static_cast<utf8proc_option_t>(UTF8PROC_NULLTERM | UTF8PROC_STABLE |
                                     UTF8PROC_CASEFOLD));

  if (result_len < 0 || !result) {
    return str;
  }

  std::string output(reinterpret_cast<char*>(result),
                     static_cast<size_t>(result_len));
  free(result);
  return output;
}

std::string to_uppercase(const std::string& str) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(str.c_str());
  ssize_t len = static_cast<ssize_t>(str.length());
  ssize_t pos = 0;
  std::string result;
  result.reserve(str.length() * 2);

  while (pos < len) {
    utf8proc_int32_t codepoint;
    ssize_t bytes_read = utf8proc_iterate(data + pos, len - pos, &codepoint);

    if (bytes_read < 0) {
      result += str[pos];
      pos++;
      continue;
    }

    utf8proc_int32_t upper_cp = utf8proc_toupper(codepoint);

    uint8_t utf8_buf[4];
    ssize_t encoded_len = utf8proc_encode_char(upper_cp, utf8_buf);

    if (encoded_len > 0) {
      result.append(reinterpret_cast<char*>(utf8_buf),
                    static_cast<size_t>(encoded_len));
    } else {
      result.append(str, static_cast<size_t>(pos),
                    static_cast<size_t>(bytes_read));
    }

    pos += bytes_read;
  }

  return result;
}

std::string normalize_nfc(const std::string& str) {
  const uint8_t* input = reinterpret_cast<const uint8_t*>(str.c_str());
  uint8_t* result = utf8proc_NFC(input);

  if (!result) {
    return str;
  }

  std::string output(reinterpret_cast<char*>(result));
  free(result);
  return output;
}

bool is_grapheme_boundary(utf8proc_int32_t cp1, utf8proc_int32_t cp2) {
  return utf8proc_grapheme_break(cp1, cp2);
}

}  // namespace utf8_utils

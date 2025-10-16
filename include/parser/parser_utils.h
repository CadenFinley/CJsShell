#pragma once

#include <string>
#include <utility>

extern const std::string SUBST_LITERAL_START;
extern const std::string SUBST_LITERAL_END;

std::string trim_trailing_whitespace(std::string s);
std::string trim_leading_whitespace(std::string s);
std::string trim_whitespace(const std::string& s);
bool is_valid_identifier(const std::string& name);
bool looks_like_assignment(const std::string& value);
std::pair<std::string, bool> strip_noenv_sentinels(const std::string& s);
bool strip_subst_literal_markers(std::string& value);

inline bool is_char_escaped(const char* str, size_t pos) {
    if (pos == 0)
        return false;
    size_t backslash_count = 0;
    size_t i = pos - 1;
    while (true) {
        if (str[i] == '\\') {
            ++backslash_count;
            if (i == 0)
                break;
            --i;
        } else {
            break;
        }
    }
    return (backslash_count % 2) == 1;
}

inline bool is_char_escaped(const std::string& str, size_t pos) {
    return is_char_escaped(str.c_str(), pos);
}

size_t find_matching_paren(const std::string& text, size_t start_pos);
size_t find_matching_brace(const std::string& text, size_t start_pos);

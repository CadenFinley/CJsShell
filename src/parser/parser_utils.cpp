#include "parser/parser_utils.h"
#include "parser/quote_info.h"

#include <cctype>

const std::string& subst_literal_start() {
    static const std::string kValue = "\x1E__SUBST_LITERAL_START__\x1E";
    return kValue;
}

const std::string& subst_literal_end() {
    static const std::string kValue = "\x1E__SUBST_LITERAL_END__\x1E";
    return kValue;
}

bool is_hex_digit(char c) {
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

bool is_char_escaped(const char* str, size_t pos) {
    if (pos == 0) {
        return false;
    }
    size_t backslash_count = 0;
    size_t i = pos - 1;
    while (true) {
        if (str[i] == '\\') {
            ++backslash_count;
            if (i == 0) {
                break;
            }
            --i;
        } else {
            break;
        }
    }
    return (backslash_count % 2) == 1;
}

bool is_char_escaped(const std::string& str, size_t pos) {
    return is_char_escaped(str.c_str(), pos);
}

std::string trim_trailing_whitespace(std::string s) {
    s.erase(s.find_last_not_of(" \t\n\r") + 1);
    return s;
}

std::string trim_leading_whitespace(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : s.substr(start);
}

std::string trim_whitespace(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

bool is_valid_identifier_start(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    return (std::isalpha(uc) != 0) || c == '_';
}

bool is_valid_identifier_char(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    return (std::isalnum(uc) != 0) || c == '_';
}

bool is_valid_identifier(const std::string& name) {
    if (name.empty() || !is_valid_identifier_start(name[0])) {
        return false;
    }
    for (size_t i = 1; i < name.length(); ++i) {
        if (!is_valid_identifier_char(name[i])) {
            return false;
        }
    }
    return true;
}

bool looks_like_assignment(const std::string& value) {
    size_t equals_pos = value.find('=');
    if (equals_pos == std::string::npos || equals_pos == 0) {
        return false;
    }

    size_t name_end = equals_pos;
    if (name_end > 0 && value[name_end - 1] == '+') {
        name_end--;
    }

    if (name_end == 0) {
        return false;
    }

    return is_valid_identifier(value.substr(0, name_end));
}

std::pair<std::string, bool> strip_noenv_sentinels(const std::string& s) {
    const std::string start = "\x1E__NOENV_START__\x1E";
    const std::string end = "\x1E__NOENV_END__\x1E";

    bool changed = false;
    std::string result;
    result.reserve(s.size());

    for (size_t i = 0; i < s.size();) {
        if (s.compare(i, start.size(), start) == 0) {
            i += start.size();
            changed = true;
            continue;
        }
        if (s.compare(i, end.size(), end) == 0) {
            i += end.size();
            changed = true;
            continue;
        }
        result += s[i];
        ++i;
    }

    if (!changed) {
        return {s, false};
    }

    return {result, true};
}

bool strip_subst_literal_markers(std::string& value) {
    const std::string& start = subst_literal_start();
    const std::string& end = subst_literal_end();

    bool changed = false;
    std::string result;
    result.reserve(value.size());

    for (size_t i = 0; i < value.size();) {
        if (value.compare(i, start.size(), start) == 0) {
            i += start.size();
            changed = true;
        } else if (value.compare(i, end.size(), end) == 0) {
            i += end.size();
            changed = true;
        } else {
            result += value[i];
            ++i;
        }
    }

    if (changed) {
        value = std::move(result);
    }

    return changed;
}

size_t find_matching_paren(const std::string& text, size_t start_pos) {
    if (start_pos >= text.length() || text[start_pos] != '(') {
        return std::string::npos;
    }

    int depth = 0;
    for (size_t i = start_pos; i < text.length(); ++i) {
        if (is_inside_quotes(text, i)) {
            continue;
        }

        if (text[i] == '(') {
            depth++;
        } else if (text[i] == ')') {
            depth--;
            if (depth == 0) {
                return i;
            }
        }
    }

    return std::string::npos;
}

size_t find_matching_brace(const std::string& text, size_t start_pos) {
    if (start_pos >= text.length() || text[start_pos] != '{') {
        return std::string::npos;
    }

    int depth = 0;
    for (size_t i = start_pos; i < text.length(); ++i) {
        if (is_inside_quotes(text, i)) {
            continue;
        }

        if (text[i] == '{') {
            depth++;
        } else if (text[i] == '}') {
            depth--;
            if (depth == 0) {
                return i;
            }
        }
    }

    return std::string::npos;
}

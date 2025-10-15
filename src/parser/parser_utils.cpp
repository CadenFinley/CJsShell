#include "parser/parser_utils.h"

#include <cctype>

const std::string SUBST_LITERAL_START = "\x1E__SUBST_LITERAL_START__\x1E";
const std::string SUBST_LITERAL_END = "\x1E__SUBST_LITERAL_END__\x1E";

std::string trim_trailing_whitespace(std::string s) {
    s.erase(s.find_last_not_of(" \t\n\r") + 1);
    return s;
}

std::string trim_leading_whitespace(std::string s) {
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

bool is_valid_identifier(const std::string& name) {
    if (name.empty() || (!std::isalpha(static_cast<unsigned char>(name[0])) && name[0] != '_')) {
        return false;
    }
    for (size_t i = 1; i < name.length(); ++i) {
        unsigned char ch = static_cast<unsigned char>(name[i]);
        if (!std::isalnum(ch) && ch != '_') {
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
    size_t a = s.find(start);
    size_t b = s.rfind(end);
    if (a != std::string::npos && b != std::string::npos && b >= a + start.size()) {
        std::string mid = s.substr(a + start.size(), b - (a + start.size()));
        std::string out = s.substr(0, a) + mid + s.substr(b + end.size());
        return {out, true};
    }
    return {s, false};
}

bool strip_subst_literal_markers(std::string& value) {
    if (value.empty()) {
        return false;
    }

    bool changed = false;
    std::string result;
    result.reserve(value.size());

    for (size_t i = 0; i < value.size();) {
        if (value.compare(i, SUBST_LITERAL_START.size(), SUBST_LITERAL_START) == 0) {
            i += SUBST_LITERAL_START.size();
            changed = true;
        } else if (value.compare(i, SUBST_LITERAL_END.size(), SUBST_LITERAL_END) == 0) {
            i += SUBST_LITERAL_END.size();
            changed = true;
        } else {
            result += value[i];
            i++;
        }
    }

    if (changed) {
        value = std::move(result);
    }

    return changed;
}

bool is_char_escaped(const char* str, size_t pos) {
    size_t backslash_count = 0;
    for (size_t j = pos; j > 0; --j) {
        if (str[j - 1] == '\\') {
            backslash_count++;
        } else {
            break;
        }
    }
    return (backslash_count % 2) == 1;
}

bool is_char_escaped(const std::string& str, size_t pos) {
    return is_char_escaped(str.c_str(), pos);
}

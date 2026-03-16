/*
  parser_utils.cpp

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "parser_utils.h"
#include "quote_info.h"
#include "quote_state.h"
#include "string_utils.h"

#include <cctype>

const std::string& subst_literal_start() {
    static const std::string kValue = "\x1E__SUBST_LITERAL_START__\x1E";
    return kValue;
}

const std::string& subst_literal_end() {
    static const std::string kValue = "\x1E__SUBST_LITERAL_END__\x1E";
    return kValue;
}

const std::string& noenv_start() {
    static const std::string kValue = "\x1E__NOENV_START__\x1E";
    return kValue;
}

const std::string& noenv_end() {
    static const std::string kValue = "\x1E__NOENV_END__\x1E";
    return kValue;
}

const std::string& subst_literal_start_plain() {
    static const std::string kValue = "__SUBST_LITERAL_START__";
    return kValue;
}

const std::string& subst_literal_end_plain() {
    static const std::string kValue = "__SUBST_LITERAL_END__";
    return kValue;
}

const std::string& noenv_start_plain() {
    static const std::string kValue = "__NOENV_START__";
    return kValue;
}

const std::string& noenv_end_plain() {
    static const std::string kValue = "__NOENV_END__";
    return kValue;
}

const std::string& substitution_placeholder() {
    static const std::string kValue = "__CJSH_SUBST__";
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
    return string_utils::trim_right_ascii_whitespace_copy(s);
}

std::string trim_leading_whitespace(const std::string& s) {
    return string_utils::trim_left_ascii_whitespace_copy(s);
}

std::string trim_whitespace(const std::string& s) {
    return string_utils::trim_ascii_whitespace_copy(s);
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

bool split_on_first_equals(const std::string& value, std::string& left, std::string& right,
                           bool require_nonempty_left);

bool parse_assignment(const std::string& arg, std::string& name, std::string& value,
                      bool strip_surrounding_quotes) {
    if (!split_on_first_equals(arg, name, value, true)) {
        return false;
    }

    if (strip_surrounding_quotes && value.size() >= 2) {
        if ((value.front() == '"' && value.back() == '"') ||
            (value.front() == '\'' && value.back() == '\'')) {
            value = value.substr(1, value.size() - 2);
        }
    }

    return true;
}

bool parse_env_assignment(const std::string& arg, std::string& name, std::string& value,
                          bool strip_surrounding_quotes) {
    std::string parsed_name;
    std::string parsed_value;
    if (!parse_assignment(arg, parsed_name, parsed_value, strip_surrounding_quotes)) {
        return false;
    }

    parsed_name = trim_whitespace(parsed_name);
    if (!is_valid_identifier(parsed_name)) {
        return false;
    }

    name = std::move(parsed_name);
    value = std::move(parsed_value);
    return true;
}

bool parse_assignment_operand(const std::string& arg, AssignmentOperand& operand,
                              bool strip_surrounding_quotes) {
    std::string parsed_name;
    std::string parsed_value;
    if (parse_assignment(arg, parsed_name, parsed_value, strip_surrounding_quotes)) {
        operand.name = std::move(parsed_name);
        operand.value = std::move(parsed_value);
        operand.has_assignment = true;
        return true;
    }

    operand.name = arg;
    operand.value.clear();
    operand.has_assignment = false;
    return true;
}

size_t find_token_end_with_quotes(const std::string& text, size_t start, size_t end,
                                  const std::string& delimiter_chars, bool stop_on_whitespace) {
    if (start >= end || start >= text.size()) {
        return start;
    }

    utils::QuoteState quote_state;
    size_t i = start;
    while (i < end) {
        char ch = text[i];
        if (ch == '\\' && !quote_state.in_single_quote && i + 1 < end) {
            i += 2;
            continue;
        }
        (void)quote_state.consume_forward(ch);
        if (!quote_state.inside_quotes()) {
            if ((stop_on_whitespace && (std::isspace(static_cast<unsigned char>(ch)) != 0)) ||
                (!delimiter_chars.empty() && delimiter_chars.find(ch) != std::string::npos)) {
                break;
            }
        }
        ++i;
    }

    return i;
}

bool split_on_first_equals(const std::string& value, std::string& left, std::string& right,
                           bool require_nonempty_left) {
    size_t equals_pos = value.find('=');
    if (equals_pos == std::string::npos) {
        return false;
    }
    if (require_nonempty_left && equals_pos == 0) {
        return false;
    }

    left = value.substr(0, equals_pos);
    right = value.substr(equals_pos + 1);
    return true;
}

bool looks_like_assignment(const std::string& value) {
    std::string name;
    std::string rhs;
    if (!split_on_first_equals(value, name, rhs, true)) {
        return false;
    }

    size_t name_end = name.size();
    if (name_end > 0 && value[name_end - 1] == '+') {
        name_end--;
    }

    if (name_end == 0) {
        return false;
    }

    return is_valid_identifier(name.substr(0, name_end));
}

bool has_line_continuation_suffix(const std::string& text, bool trim_newlines) {
    size_t pos = text.size();
    if (trim_newlines) {
        while (pos > 0 && (text[pos - 1] == '\n' || text[pos - 1] == '\r')) {
            --pos;
        }
    }

    while (pos > 0 && (text[pos - 1] == ' ' || text[pos - 1] == '\t')) {
        --pos;
    }

    if (pos == 0 || text[pos - 1] != '\\') {
        return false;
    }

    size_t slash_count = 0;
    while (pos > 0 && text[pos - 1] == '\\') {
        ++slash_count;
        --pos;
    }

    return (slash_count % 2) == 1;
}

std::pair<std::string, bool> strip_noenv_sentinels(const std::string& s) {
    const std::string& start = noenv_start();
    const std::string& end = noenv_end();

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

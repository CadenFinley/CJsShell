/*
  pattern_matcher.cpp

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

#include "pattern_matcher.h"

#include <string>

bool PatternMatcher::matches_pattern(const std::string& text, const std::string& pattern) const {
    auto sanitize_quotes = [](const std::string& raw_pattern) {
        std::string cleaned;
        cleaned.reserve(raw_pattern.size());

        for (size_t i = 0; i < raw_pattern.size(); ++i) {
            char ch = raw_pattern[i];

            if (ch == '\\' && i + 1 < raw_pattern.size() &&
                (raw_pattern[i + 1] == '\'' || raw_pattern[i + 1] == '"')) {
                cleaned += ch;
                cleaned += raw_pattern[i + 1];
                ++i;
                continue;
            }

            if (ch == '\'' || ch == '"') {
                continue;
            }

            cleaned += ch;
        }

        return cleaned;
    };

    std::string sanitized_pattern = sanitize_quotes(pattern);

    if (pattern.find('|') != std::string::npos) {
        size_t start = 0;
        while (start < sanitized_pattern.length()) {
            size_t pipe_pos = sanitized_pattern.find('|', start);
            std::string sub_pattern;
            if (pipe_pos == std::string::npos) {
                sub_pattern = sanitized_pattern.substr(start);
                start = sanitized_pattern.length();
            } else {
                sub_pattern = sanitized_pattern.substr(start, pipe_pos - start);
                start = pipe_pos + 1;
            }

            if (matches_single_pattern(text, sub_pattern)) {
                return true;
            }
        }
        return false;
    }

    return matches_single_pattern(text, sanitized_pattern);
}

bool PatternMatcher::matches_single_pattern(const std::string& text,
                                            const std::string& pattern) const {
    size_t ti = 0;
    size_t pi = 0;
    size_t star_idx = std::string::npos;
    size_t match_idx = 0;

    while (ti < text.length() || pi < pattern.length()) {
        if (ti >= text.length()) {
            while (pi < pattern.length() && pattern[pi] == '*') {
                pi++;
            }
            return pi == pattern.length();
        }

        if (pi >= pattern.length()) {
            if (star_idx != std::string::npos) {
                pi = star_idx + 1;
                ti = ++match_idx;
            } else {
                return false;
            }
        } else if (pattern[pi] == '[') {
            size_t class_end = pattern.find(']', pi);
            if (class_end != std::string::npos) {
                std::string char_class = pattern.substr(pi, class_end - pi + 1);
                if (matches_char_class(text[ti], char_class)) {
                    ti++;
                    pi = class_end + 1;
                } else if (star_idx != std::string::npos) {
                    pi = star_idx + 1;
                    ti = ++match_idx;
                } else {
                    return false;
                }
            } else {
                if (pattern[pi] == text[ti]) {
                    ti++;
                    pi++;
                } else if (star_idx != std::string::npos) {
                    pi = star_idx + 1;
                    ti = ++match_idx;
                } else {
                    return false;
                }
            }
        } else if (pattern[pi] == '\\' && pi + 1 < pattern.length()) {
            char escaped_char = pattern[pi + 1];
            if (escaped_char == text[ti]) {
                ti++;
                pi += 2;
            } else if (star_idx != std::string::npos) {
                pi = star_idx + 1;
                ti = ++match_idx;
            } else {
                return false;
            }
        } else if (pattern[pi] == '?') {
            ti++;
            pi++;
        } else if (pattern[pi] == '*') {
            star_idx = pi;
            match_idx = ti;
            pi++;
        } else if (pattern[pi] == text[ti]) {
            ti++;
            pi++;
        } else if (star_idx != std::string::npos) {
            pi = star_idx + 1;
            ti = ++match_idx;
        } else {
            return false;
        }
    }

    return true;
}

bool PatternMatcher::matches_char_class(char c, const std::string& char_class) const {
    if (char_class.length() < 3 || char_class[0] != '[' || char_class.back() != ']') {
        return false;
    }

    std::string class_content = char_class.substr(1, char_class.length() - 2);
    bool negated = false;

    if (!class_content.empty() && (class_content[0] == '^' || class_content[0] == '!')) {
        negated = true;
        class_content = class_content.substr(1);
    }

    bool matches = false;

    for (size_t i = 0; i < class_content.length(); ++i) {
        if (i + 2 < class_content.length() && class_content[i + 1] == '-') {
            char start = class_content[i];
            char end = class_content[i + 2];
            if (c >= start && c <= end) {
                matches = true;
                break;
            }
            i += 2;
        } else {
            if (c == class_content[i]) {
                matches = true;
                break;
            }
        }
    }

    return negated ? !matches : matches;
}

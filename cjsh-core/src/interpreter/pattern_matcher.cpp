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

#include <fnmatch.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "shell_env.h"

namespace {

enum class PatternNodeKind : std::uint8_t {
    Literal,
    AnyCharacter,
    AnyString,
    CharacterClass,
    ExtendedGroup
};

struct PatternNode {
    PatternNodeKind kind = PatternNodeKind::Literal;
    char value = '\0';
    std::string character_class;
    std::vector<std::vector<PatternNode>> alternatives;
};

void append_unique(std::vector<size_t>& values, size_t value) {
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

class GlobPatternParser {
   public:
    explicit GlobPatternParser(const std::string& pattern, bool top_level_alternatives)
        : pattern_(pattern), top_level_alternatives_(top_level_alternatives) {
    }

    std::vector<std::vector<PatternNode>> parse() {
        return parse_alternatives(false);
    }

   private:
    const std::string& pattern_;
    bool top_level_alternatives_ = false;
    size_t position_ = 0;

    static bool is_extglob_operator(char ch) {
        return ch == '?' || ch == '*' || ch == '+' || ch == '@' || ch == '!';
    }

    std::vector<std::vector<PatternNode>> parse_alternatives(bool stop_at_close) {
        std::vector<std::vector<PatternNode>> alternatives;
        alternatives.emplace_back();

        while (position_ < pattern_.size()) {
            char ch = pattern_[position_];
            if ((ch == '|' && (stop_at_close || top_level_alternatives_)) ||
                (stop_at_close && ch == ')')) {
                if (ch == '|') {
                    ++position_;
                    alternatives.emplace_back();
                    continue;
                }
                break;
            }

            PatternNode node;
            if (ch == '\\' && position_ + 1 < pattern_.size()) {
                node.kind = PatternNodeKind::Literal;
                node.value = pattern_[position_ + 1];
                position_ += 2;
            } else if (config::extglob_enabled && is_extglob_operator(ch) &&
                       position_ + 1 < pattern_.size() && pattern_[position_ + 1] == '(') {
                node.kind = PatternNodeKind::ExtendedGroup;
                node.value = ch;
                position_ += 2;
                node.alternatives = parse_alternatives(true);
                if (position_ < pattern_.size() && pattern_[position_] == ')') {
                    ++position_;
                } else {
                    node.kind = PatternNodeKind::Literal;
                    node.value = ch;
                    node.alternatives.clear();
                }
            } else if (ch == '*') {
                node.kind = PatternNodeKind::AnyString;
                ++position_;
            } else if (ch == '?') {
                node.kind = PatternNodeKind::AnyCharacter;
                ++position_;
            } else if (ch == '[') {
                size_t close = position_ + 1;
                if (close < pattern_.size() && (pattern_[close] == '!' || pattern_[close] == '^')) {
                    ++close;
                }
                if (close < pattern_.size() && pattern_[close] == ']') {
                    ++close;
                }
                for (; close < pattern_.size(); ++close) {
                    if (pattern_[close] == '[' && close + 1 < pattern_.size() &&
                        (pattern_[close + 1] == ':' || pattern_[close + 1] == '.' ||
                         pattern_[close + 1] == '=')) {
                        const char marker = pattern_[close + 1];
                        size_t nested_close = pattern_.find(std::string{marker, ']'}, close + 2);
                        if (nested_close == std::string::npos) {
                            close = pattern_.size();
                            break;
                        }
                        close = nested_close + 1;
                        continue;
                    }
                    if (pattern_[close] == ']') {
                        break;
                    }
                }
                if (close >= pattern_.size()) {
                    close = std::string::npos;
                }
                if (close != std::string::npos) {
                    node.kind = PatternNodeKind::CharacterClass;
                    node.character_class = pattern_.substr(position_, close - position_ + 1);
                    position_ = close + 1;
                } else {
                    node.kind = PatternNodeKind::Literal;
                    node.value = ch;
                    ++position_;
                }
            } else {
                node.kind = PatternNodeKind::Literal;
                node.value = ch;
                ++position_;
            }
            alternatives.back().push_back(std::move(node));
        }

        return alternatives;
    }
};

bool character_class_matches(char character, const std::string& pattern) {
    if (pattern.size() < 3 || pattern.front() != '[' || pattern.back() != ']') {
        return false;
    }
    const std::string candidate(1, character);
    return fnmatch(pattern.c_str(), candidate.c_str(), 0) == 0;
}

bool matches_simple_sequence(const std::vector<PatternNode>& sequence, const std::string& text) {
    if (sequence.size() != text.size() &&
        std::none_of(sequence.begin(), sequence.end(), [](const PatternNode& node) {
            return node.kind == PatternNodeKind::AnyString;
        })) {
        return false;
    }
    size_t node_index = 0;
    size_t text_index = 0;
    size_t star_index = sequence.size();
    size_t star_text_index = 0;

    // Ordinary globs need only a full match, not every possible endpoint. On a
    // mismatch, extend the most recent star by one byte and retry its suffix.
    while (text_index < text.size()) {
        if (node_index < sequence.size()) {
            const auto& node = sequence[node_index];
            if (node.kind == PatternNodeKind::AnyString) {
                star_index = node_index++;
                star_text_index = text_index;
                continue;
            }
            if ((node.kind == PatternNodeKind::Literal && node.value == text[text_index]) ||
                node.kind == PatternNodeKind::AnyCharacter ||
                (node.kind == PatternNodeKind::CharacterClass &&
                 character_class_matches(text[text_index], node.character_class))) {
                ++node_index;
                ++text_index;
                continue;
            }
        }
        if (star_index == sequence.size()) {
            return false;
        }
        node_index = star_index + 1;
        text_index = ++star_text_index;
    }
    while (node_index < sequence.size() &&
           sequence[node_index].kind == PatternNodeKind::AnyString) {
        ++node_index;
    }
    return node_index == sequence.size();
}

std::vector<size_t> match_sequence(const std::vector<PatternNode>& sequence, size_t node_index,
                                   const std::string& text, size_t text_index);

std::vector<size_t> match_alternatives(const std::vector<std::vector<PatternNode>>& alternatives,
                                       const std::string& text, size_t text_index) {
    std::vector<size_t> endpoints;
    for (const auto& alternative : alternatives) {
        for (size_t endpoint : match_sequence(alternative, 0, text, text_index)) {
            append_unique(endpoints, endpoint);
        }
    }
    return endpoints;
}

std::vector<size_t> repeat_group(const PatternNode& node, const std::string& text,
                                 const std::vector<size_t>& initial) {
    std::vector<size_t> endpoints = initial;
    // This worklist grows while matching; iterators would be invalidated by append_unique.
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (size_t cursor = 0; cursor < endpoints.size(); ++cursor) {
        size_t begin = endpoints[cursor];
        for (size_t endpoint : match_alternatives(node.alternatives, text, begin)) {
            if (endpoint != begin) {
                append_unique(endpoints, endpoint);
            }
        }
    }
    return endpoints;
}

std::vector<size_t> match_node(const PatternNode& node, const std::string& text,
                               size_t text_index) {
    switch (node.kind) {
        case PatternNodeKind::Literal:
            return text_index < text.size() && text[text_index] == node.value
                       ? std::vector<size_t>{text_index + 1}
                       : std::vector<size_t>{};
        case PatternNodeKind::AnyCharacter:
            return text_index < text.size() ? std::vector<size_t>{text_index + 1}
                                            : std::vector<size_t>{};
        case PatternNodeKind::AnyString: {
            std::vector<size_t> endpoints;
            endpoints.reserve(text.size() - text_index + 1);
            for (size_t endpoint = text_index; endpoint <= text.size(); ++endpoint) {
                endpoints.push_back(endpoint);
            }
            return endpoints;
        }
        case PatternNodeKind::CharacterClass:
            return text_index < text.size() &&
                           character_class_matches(text[text_index], node.character_class)
                       ? std::vector<size_t>{text_index + 1}
                       : std::vector<size_t>{};
        case PatternNodeKind::ExtendedGroup:
            break;
    }

    std::vector<size_t> direct = match_alternatives(node.alternatives, text, text_index);
    if (node.value == '@') {
        return direct;
    }
    if (node.value == '?') {
        append_unique(direct, text_index);
        return direct;
    }
    if (node.value == '*') {
        return repeat_group(node, text, {text_index});
    }
    if (node.value == '+') {
        return repeat_group(node, text, direct);
    }
    if (node.value == '!') {
        std::vector<size_t> endpoints;
        for (size_t endpoint = text_index; endpoint <= text.size(); ++endpoint) {
            if (std::find(direct.begin(), direct.end(), endpoint) == direct.end()) {
                endpoints.push_back(endpoint);
            }
        }
        return endpoints;
    }
    return {};
}

std::vector<size_t> match_sequence(const std::vector<PatternNode>& sequence, size_t node_index,
                                   const std::string& text, size_t text_index) {
    if (node_index >= sequence.size()) {
        return {text_index};
    }

    std::vector<size_t> endpoints;
    for (size_t next_index : match_node(sequence[node_index], text, text_index)) {
        for (size_t endpoint : match_sequence(sequence, node_index + 1, text, next_index)) {
            append_unique(endpoints, endpoint);
        }
    }
    return endpoints;
}

}  // namespace

bool PatternMatcher::matches_pattern(const std::string& text, const std::string& pattern,
                                     bool top_level_alternatives) const {
    auto sanitize_quotes = [](const std::string& raw_pattern) {
        std::string cleaned;
        cleaned.reserve(raw_pattern.size());
        char quote = '\0';

        for (size_t i = 0; i < raw_pattern.size(); ++i) {
            char ch = raw_pattern[i];

            if (ch == '\\' && quote != '\'' && i + 1 < raw_pattern.size()) {
                cleaned += ch;
                cleaned += raw_pattern[i + 1];
                ++i;
                continue;
            }

            if (ch == '\'' || ch == '"') {
                if (quote == '\0') {
                    quote = ch;
                } else if (quote == ch) {
                    quote = '\0';
                } else {
                    cleaned += ch;
                }
                continue;
            }

            if (quote != '\0' && (ch == '*' || ch == '?' || ch == '[' || ch == '|' || ch == '\\')) {
                cleaned += '\\';
            }
            cleaned += ch;
        }

        return cleaned;
    };

    struct CompiledPattern {
        std::string pattern;
        bool extglob_enabled;
        bool top_level_alternatives;
        std::vector<std::vector<PatternNode>> alternatives;
    };
    // Parameter replacement tests many substrings against the same pattern.
    // Keep one parsed pattern per thread, with every parsing option in the key.
    // Character classes still use the current locale when they are matched.
    thread_local std::optional<CompiledPattern> cached;
    if (!cached || cached->pattern != pattern ||
        cached->extglob_enabled != config::extglob_enabled ||
        cached->top_level_alternatives != top_level_alternatives) {
        const std::string sanitized_pattern = sanitize_quotes(pattern);
        GlobPatternParser parser(sanitized_pattern, top_level_alternatives);
        cached = CompiledPattern{pattern, config::extglob_enabled, top_level_alternatives,
                                 parser.parse()};
    }
    for (const auto& alternative : cached->alternatives) {
        const bool has_extended_group = std::any_of(
            alternative.begin(), alternative.end(),
            [](const PatternNode& node) { return node.kind == PatternNodeKind::ExtendedGroup; });
        if (!has_extended_group) {
            if (matches_simple_sequence(alternative, text)) {
                return true;
            }
            continue;
        }
        auto endpoints = match_sequence(alternative, 0, text, 0);
        if (std::find(endpoints.begin(), endpoints.end(), text.size()) != endpoints.end()) {
            return true;
        }
    }
    return false;
}

/*
  completion_utils.cpp

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

#include "completion_utils.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "cjsh_completions.h"
#include "quote_state.h"

namespace completion_utils {

std::string quote_path_if_needed(const std::string& path) {
    if (path.empty())
        return path;

    bool needs_quoting = false;
    for (char c : path) {
        if (c == ' ' || c == '\t' || c == '\'' || c == '"' || c == '\\' || c == '(' || c == ')' ||
            c == '[' || c == ']' || c == '{' || c == '}' || c == '&' || c == '|' || c == ';' ||
            c == '<' || c == '>' || c == '*' || c == '?' || c == '$' || c == '`') {
            needs_quoting = true;
            break;
        }
    }

    if (!needs_quoting)
        return path;

    std::string result = "\"";
    for (char c : path) {
        if (c == '"' || c == '\\') {
            result += '\\';
        }
        result += c;
    }
    result += "\"";

    return result;
}

std::string unquote_path(const std::string& path) {
    if (path.empty())
        return path;

    std::string result;
    bool in_single_quote = false;
    bool in_double_quote = false;
    bool escaped = false;

    for (size_t i = 0; i < path.length(); ++i) {
        char c = path[i];

        if (escaped) {
            result += c;
            escaped = false;
            continue;
        }

        if (c == '\\' && !in_single_quote) {
            escaped = true;
            continue;
        }

        if (c == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            continue;
        }

        if (c == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            continue;
        }

        result += c;
    }

    return result;
}

std::vector<std::string> tokenize_command_line(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current_token;
    bool in_single_quote = false;
    bool in_double_quote = false;
    bool escaped = false;

    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];

        if (escaped) {
            current_token += c;
            escaped = false;
            continue;
        }

        if (c == '\\') {
            if (in_single_quote) {
                current_token += c;
            } else {
                escaped = true;
            }
            continue;
        }

        if (c == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            continue;
        }

        if (c == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            continue;
        }

        if ((c == ' ' || c == '\t') && !in_single_quote && !in_double_quote) {
            if (!current_token.empty()) {
                tokens.push_back(current_token);
                current_token.clear();
            }
            continue;
        }

        current_token += c;
    }

    if (!current_token.empty()) {
        tokens.push_back(current_token);
    }

    return tokens;
}

size_t find_last_unquoted_space(const std::string& str) {
    utils::QuoteState quote_state;
    size_t last_space = std::string::npos;

    for (size_t i = 0; i < str.length(); ++i) {
        char c = str[i];

        if (quote_state.consume_forward(c) == utils::QuoteAdvanceResult::Continue) {
            continue;
        }

        if ((c == ' ' || c == '\t') && !quote_state.inside_quotes()) {
            last_space = i;
        }
    }

    return last_space;
}

std::string normalize_for_comparison(const std::string& value) {
    if (is_completion_case_sensitive()) {
        return value;
    }

    std::string lower_value = value;
    std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower_value;
}

bool starts_with_case_insensitive(const std::string& str, const std::string& prefix) {
    if (prefix.length() > str.length()) {
        return false;
    }

    return std::equal(prefix.begin(), prefix.end(), str.begin(),
                      [](char a, char b) { return std::tolower(a) == std::tolower(b); });
}

bool starts_with_case_sensitive(const std::string& str, const std::string& prefix) {
    if (prefix.length() > str.length()) {
        return false;
    }

    return std::equal(prefix.begin(), prefix.end(), str.begin());
}

bool matches_completion_prefix(const std::string& str, const std::string& prefix) {
    if (is_completion_case_sensitive()) {
        return starts_with_case_sensitive(str, prefix);
    }

    return starts_with_case_insensitive(str, prefix);
}

bool equals_completion_token(const std::string& value, const std::string& target) {
    if (is_completion_case_sensitive()) {
        return value == target;
    }

    if (value.length() != target.length()) {
        return false;
    }

    return std::equal(value.begin(), value.end(), target.begin(),
                      [](char a, char b) { return std::tolower(a) == std::tolower(b); });
}

std::string sanitize_job_command_summary(const std::string& command) {
    std::string summary;
    summary.reserve(command.size());
    bool last_was_space = true;

    for (char ch : command) {
        unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isspace(uch) != 0) {
            if (!summary.empty() && !last_was_space) {
                summary.push_back(' ');
                last_was_space = true;
            }
            continue;
        }
        if (std::isprint(uch) == 0)
            continue;
        summary.push_back(ch);
        last_was_space = false;
        if (summary.size() >= 80)
            break;
    }

    while (!summary.empty() && summary.back() == ' ')
        summary.pop_back();

    return summary;
}

}  // namespace completion_utils

/*
  expansion_engine.cpp

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

#include "parser/expansion_engine.h"

#include <glob.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <type_traits>

#include "cjsh.h"
#include "shell.h"

ExpansionEngine::ExpansionEngine(Shell* shell) : shell(shell) {
}

std::vector<std::string> ExpansionEngine::expand_braces(const std::string& pattern) {
    std::vector<std::string> result;

    result.reserve(8);

    size_t open_pos = pattern.find('{');
    if (open_pos == std::string::npos) {
        result.push_back(pattern);
        return result;
    }

    int depth = 1;
    size_t close_pos = open_pos + 1;

    while (close_pos < pattern.size() && depth > 0) {
        if (pattern[close_pos] == '{') {
            depth++;
        } else if (pattern[close_pos] == '}') {
            depth--;
        }

        if (depth > 0) {
            close_pos++;
        }
    }

    if (depth != 0) {
        result.push_back(pattern);
        return result;
    }

    std::string prefix = pattern.substr(0, open_pos);
    std::string content = pattern.substr(open_pos + 1, close_pos - open_pos - 1);
    std::string suffix = pattern.substr(close_pos + 1);

    if (content.empty() && prefix.empty() && suffix.empty()) {
        result.push_back(pattern);
        return result;
    }

    size_t range_pos = content.find("..");
    if (range_pos != std::string::npos) {
        std::string start_str = content.substr(0, range_pos);
        std::string end_str = content.substr(range_pos + 2);

        auto is_numeric = [](const std::string& str) {
            if (str.empty())
                return false;
            size_t start = 0;
            if (str[0] == '-' || str[0] == '+')
                start = 1;
            if (start >= str.length())
                return false;
            return std::all_of(str.begin() + start, str.end(),
                               [](char c) { return std::isdigit(c); });
        };

        bool is_numeric_range = is_numeric(start_str) && is_numeric(end_str);

        if (is_numeric_range) {
            int start_int = std::stoi(start_str);
            int end_int = std::stoi(end_str);
            size_t range_size = std::abs(end_int - start_int) + 1;
            if (range_size > MAX_EXPANSION_SIZE) {
                result.push_back(pattern);
                return result;
            }
            result.reserve(range_size);
            expand_range(start_int, end_int, prefix, suffix, result);
            return result;
        }

        if (start_str.length() == 1 && end_str.length() == 1 && (std::isalpha(start_str[0]) != 0) &&
            (std::isalpha(end_str[0]) != 0)) {
            char start_char = start_str[0];
            char end_char = end_str[0];

            bool both_lower = std::islower(start_char) && std::islower(end_char);
            bool both_upper = std::isupper(start_char) && std::isupper(end_char);

            if (both_lower || both_upper) {
                size_t char_range_size = std::abs(end_char - start_char) + 1;
                if (char_range_size > MAX_EXPANSION_SIZE) {
                    result.push_back(pattern);
                    return result;
                }
                result.reserve(char_range_size);
                expand_range(start_char, end_char, prefix, suffix, result);
                return result;
            }
        }

        result.push_back(pattern);
        return result;
    }

    std::vector<std::string> options;
    size_t start = 0;
    depth = 0;

    for (size_t i = 0; i <= content.size(); ++i) {
        if (i == content.size() || (content[i] == ',' && depth == 0)) {
            options.emplace_back(content.substr(start, i - start));
            start = i + 1;
        } else if (content[i] == '{') {
            depth++;
        } else if (content[i] == '}') {
            depth--;
        }
    }

    if (options.size() > MAX_EXPANSION_SIZE) {
        result.push_back(pattern);
        return result;
    }

    result.reserve(options.size());

    for (const auto& option : options) {
        std::string combined;
        combined.reserve(prefix.size() + option.size() + suffix.size());
        combined = prefix;
        combined += option;
        combined += suffix;
        expand_and_append_results(combined, result);
    }

    return result;
}

std::vector<std::string> ExpansionEngine::expand_wildcards(const std::string& pattern) {
    std::vector<std::string> result;

    result.reserve(4);

    if (shell != nullptr && shell->get_shell_option("noglob")) {
        result.push_back(pattern);
        return result;
    }

    bool has_wildcards = false;

    for (size_t i = 0; i < pattern.length(); ++i) {
        char c = pattern[i];

        if (c == '\x1F' && i + 1 < pattern.length()) {
            i++;
            continue;
        }

        if (c == '*' || c == '?' || c == '[') {
            has_wildcards = true;
            break;
        }
    }

    std::string unescaped;
    unescaped.reserve(pattern.length());

    for (size_t i = 0; i < pattern.length(); ++i) {
        if (pattern[i] == '\x1F') {
            if (i + 1 < pattern.length()) {
                i++;
                unescaped += pattern[i];
            }
        } else {
            unescaped += pattern[i];
        }
    }

    if (!has_wildcards) {
        result.push_back(unescaped);
        return result;
    }

    glob_t glob_result;
    memset(&glob_result, 0, sizeof(glob_result));

    int return_value = glob(unescaped.c_str(), GLOB_TILDE | GLOB_MARK, nullptr, &glob_result);
    if (return_value == 0) {
        result.reserve(glob_result.gl_pathc);
        for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
            result.emplace_back(glob_result.gl_pathv[i]);
        }
        globfree(&glob_result);
    } else if (return_value == GLOB_NOMATCH) {
        result.push_back(std::move(unescaped));
    }

    return result;
}

void ExpansionEngine::expand_and_append_results(const std::string& combined,
                                                std::vector<std::string>& result) {
    std::vector<std::string> expanded_results = expand_braces(combined);
    result.insert(result.end(), std::make_move_iterator(expanded_results.begin()),
                  std::make_move_iterator(expanded_results.end()));
}

template <typename T>
void ExpansionEngine::expand_range(T start, T end, const std::string& prefix,
                                   const std::string& suffix, std::vector<std::string>& result) {
    if constexpr (std::is_same_v<T, int>) {
        if (start <= end) {
            for (T i = start; i <= end; ++i) {
                std::string combined;
                combined.reserve(prefix.size() + 12 + suffix.size());
                combined = prefix;
                combined += std::to_string(i);
                combined += suffix;
                expand_and_append_results(combined, result);
            }
        } else {
            for (T i = start; i >= end; --i) {
                std::string combined;
                combined.reserve(prefix.size() + 12 + suffix.size());
                combined = prefix;
                combined += std::to_string(i);
                combined += suffix;
                expand_and_append_results(combined, result);
            }
        }
    } else if constexpr (std::is_same_v<T, char>) {
        if (start <= end) {
            for (T c = start; c <= end; ++c) {
                std::string combined;
                combined.reserve(prefix.size() + 1 + suffix.size());
                combined = prefix;
                combined.append(1, c);
                combined.append(suffix);
                expand_and_append_results(combined, result);
            }
        } else {
            for (T c = start; c >= end; --c) {
                std::string combined;
                combined.reserve(prefix.size() + 1 + suffix.size());
                combined = prefix;
                combined.append(1, c);
                combined.append(suffix);
                expand_and_append_results(combined, result);
            }
        }
    }
}

template void ExpansionEngine::expand_range<int>(int start, int end, const std::string& prefix,
                                                 const std::string& suffix,
                                                 std::vector<std::string>& result);
template void ExpansionEngine::expand_range<char>(char start, char end, const std::string& prefix,
                                                  const std::string& suffix,
                                                  std::vector<std::string>& result);

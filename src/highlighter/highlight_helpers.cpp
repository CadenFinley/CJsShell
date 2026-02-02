/*
  highlight_helpers.cpp

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

#include "highlight_helpers.h"

#include <cctype>
#include <string>

#include "token_classifier.h"
#include "utils/quote_state.h"

namespace highlight_helpers {

static size_t parse_history_modifiers(const char* input, size_t start, size_t len) {
    size_t pos = start;

    if (pos < len && input[pos] == ':') {
        pos++;
        while (pos < len && (std::isdigit(input[pos]) || input[pos] == '-' || input[pos] == '^' ||
                             input[pos] == '$' || input[pos] == '*')) {
            pos++;
        }
    }

    if (pos < len && input[pos] == ':') {
        pos++;
        if (pos < len) {
            if (input[pos] == 'h' || input[pos] == 't' || input[pos] == 'r' || input[pos] == 'e' ||
                input[pos] == 'p' || input[pos] == 'q' || input[pos] == 'x' || input[pos] == 'u' ||
                input[pos] == 'l') {
                pos++;
            } else if (input[pos] == 's' ||
                       (input[pos] == 'g' && pos + 1 < len && input[pos + 1] == 's')) {
                if (input[pos] == 'g')
                    pos++;
                if (input[pos] == 's')
                    pos++;
                if (pos < len && (input[pos] == '/' || input[pos] == ':' || input[pos] == ';')) {
                    char delim = input[pos];
                    pos++;
                    int delim_count = 1;
                    while (pos < len && delim_count < 3) {
                        if (input[pos] == delim)
                            delim_count++;
                        pos++;
                    }
                }
            }
        }
    }

    return pos;
}

static size_t find_matching_parenthesis(const char* input, size_t search_start, size_t end,
                                        int depth) {
    bool inner_single = false;
    bool inner_double = false;
    bool inner_escaped = false;

    for (size_t pos = search_start; pos < end; ++pos) {
        char inner = input[pos];
        if (inner_escaped) {
            inner_escaped = false;
        } else if (inner == '\\' && !inner_single) {
            inner_escaped = true;
        } else if (inner == '\'' && !inner_double) {
            inner_single = !inner_single;
        } else if (inner == '"' && !inner_single) {
            inner_double = !inner_double;
        } else if (!inner_single) {
            if (inner == '(') {
                depth++;
            } else if (inner == ')') {
                depth--;
                if (depth == 0) {
                    return pos;
                }
            }
        }
    }

    return std::string::npos;
}

void highlight_variable_assignment(ic_highlight_env_t* henv, const char* input,
                                   size_t absolute_start, const std::string& token) {
    size_t eq_pos = token.find('=');
    if (eq_pos == std::string::npos) {
        highlight_quotes_and_variables(henv, input, absolute_start, token.length());
        return;
    }

    if (eq_pos == 0) {
        ic_highlight(henv, static_cast<long>(absolute_start), static_cast<long>(token.length()),
                     "cjsh-variable");
        return;
    }

    ic_highlight(henv, static_cast<long>(absolute_start), static_cast<long>(eq_pos),
                 "cjsh-variable");
    ic_highlight(henv, static_cast<long>(absolute_start + eq_pos), 1L, "cjsh-operator");

    if (eq_pos + 1 >= token.length()) {
        return;
    }

    std::string value = token.substr(eq_pos + 1);
    highlight_assignment_value(henv, input, absolute_start + eq_pos + 1, value);
}

void highlight_assignment_value(ic_highlight_env_t* henv, const char* input, size_t absolute_start,
                                const std::string& value) {
    if (value.empty()) {
        return;
    }

    ic_highlight(henv, static_cast<long>(absolute_start), static_cast<long>(value.length()),
                 "cjsh-assignment-value");

    char quote_type = 0;
    if (token_classifier::is_quoted_string(value, quote_type)) {
        ic_highlight(henv, static_cast<long>(absolute_start), static_cast<long>(value.length()),
                     "cjsh-string");
        return;
    }

    if (token_classifier::is_numeric_literal(value)) {
        ic_highlight(henv, static_cast<long>(absolute_start), static_cast<long>(value.length()),
                     "cjsh-number");
        return;
    }

    if (!value.empty() && value[0] == '$') {
        ic_highlight(henv, static_cast<long>(absolute_start), static_cast<long>(value.length()),
                     "cjsh-variable");
        highlight_quotes_and_variables(henv, input, absolute_start, value.length());
        return;
    }

    if (value.find('$') != std::string::npos || value.find('`') != std::string::npos ||
        value.find("$(") != std::string::npos) {
        highlight_quotes_and_variables(henv, input, absolute_start, value.length());
    }
}

void highlight_quotes_and_variables(ic_highlight_env_t* henv, const char* input, size_t start,
                                    size_t length) {
    if (length == 0) {
        return;
    }

    const size_t end = start + length;

    bool in_single_quote = false;
    bool in_double_quote = false;
    bool escaped = false;
    size_t single_quote_start = 0;
    size_t double_quote_start = 0;

    for (size_t i = start; i < end; ++i) {
        char c = input[i];

        if (escaped) {
            escaped = false;
            continue;
        }

        if (c == '\\' && !in_single_quote) {
            escaped = true;
            continue;
        }

        if (!in_single_quote && c == '$' && i + 1 < end && input[i + 1] == '(') {
            bool is_arithmetic = (i + 2 < end && input[i + 2] == '(');
            size_t match = find_matching_parenthesis(input, i + 2, end, 1);
            if (match != std::string::npos) {
                size_t highlight_len = (match + 1) - i;
                ic_highlight(henv, static_cast<long>(i), static_cast<long>(highlight_len),
                             is_arithmetic ? "cjsh-arithmetic" : "cjsh-command-substitution");
                i = match;
                continue;
            }
        }

        if (!in_single_quote && c == '`') {
            size_t j = i + 1;
            bool inner_escaped = false;
            while (j < end) {
                char inner = input[j];
                if (inner_escaped) {
                    inner_escaped = false;
                } else if (inner == '\\') {
                    inner_escaped = true;
                } else if (inner == '`') {
                    break;
                }
                ++j;
            }

            if (j < end) {
                size_t highlight_len = (j + 1) - i;
                ic_highlight(henv, static_cast<long>(i), static_cast<long>(highlight_len),
                             "cjsh-command-substitution");
                i = j;
                continue;
            }
        }

        if (!in_single_quote && !in_double_quote && c == '(' && i + 1 < end &&
            input[i + 1] == '(') {
            size_t match = find_matching_parenthesis(input, i + 2, end, 2);
            if (match != std::string::npos) {
                size_t highlight_len = (match + 1) - i;
                ic_highlight(henv, static_cast<long>(i), static_cast<long>(highlight_len),
                             "cjsh-arithmetic");
                i = match;
                continue;
            }
        }

        if (c == '\'' && !in_double_quote) {
            if (!in_single_quote) {
                in_single_quote = true;
                single_quote_start = i;
            } else {
                in_single_quote = false;
                ic_highlight(henv, static_cast<long>(single_quote_start),
                             static_cast<long>(i - single_quote_start + 1), "cjsh-string");
            }
            continue;
        }

        if (c == '"' && !in_single_quote) {
            if (!in_double_quote) {
                in_double_quote = true;
                double_quote_start = i;
            } else {
                in_double_quote = false;
                ic_highlight(henv, static_cast<long>(double_quote_start),
                             static_cast<long>(i - double_quote_start + 1), "cjsh-string");
            }
            continue;
        }

        if (c == '$' && !in_single_quote) {
            size_t var_start = i;
            size_t var_end = i + 1;

            if (var_end < end && input[var_end] == '{') {
                ++var_end;
                while (var_end < end && input[var_end] != '}') {
                    ++var_end;
                }
                if (var_end < end) {
                    ++var_end;
                }
            } else {
                while (var_end < end) {
                    char vc = input[var_end];
                    if ((std::isalnum(static_cast<unsigned char>(vc)) != 0) || vc == '_' ||
                        (var_end == var_start + 1 &&
                         (std::isdigit(static_cast<unsigned char>(vc)) != 0))) {
                        ++var_end;
                    } else {
                        break;
                    }
                }
            }

            if (var_end > var_start + 1) {
                ic_highlight(henv, static_cast<long>(var_start),
                             static_cast<long>(var_end - var_start), "cjsh-variable");
                i = var_end - 1;
            }
        }
    }
}

void highlight_history_expansions(ic_highlight_env_t* henv, const char* input, size_t len) {
    if (len == 0) {
        return;
    }

    utils::QuoteState quote_state;

    for (size_t i = 0; i < len; ++i) {
        char c = input[i];

        if (quote_state.consume_forward(c) == utils::QuoteAdvanceResult::Continue) {
            continue;
        }

        if (quote_state.inside_quotes()) {
            continue;
        }

        if (c == '^' && i == 0) {
            size_t end = i + 1;
            int caret_count = 1;
            while (end < len) {
                if (input[end] == '^') {
                    caret_count++;
                    if (caret_count >= 2) {
                        size_t highlight_len = end - i + 1;
                        if (caret_count == 3 || end == len - 1) {
                            ic_highlight(henv, static_cast<long>(i),
                                         static_cast<long>(highlight_len),
                                         "cjsh-history-expansion");
                            i = end;
                            break;
                        }
                    }
                }
                end++;
            }
            continue;
        }

        if (c == '!') {
            bool at_word_boundary =
                (i == 0) || std::isspace(static_cast<unsigned char>(input[i - 1])) ||
                input[i - 1] == ';' || input[i - 1] == '|' || input[i - 1] == '&' ||
                input[i - 1] == '(' || input[i - 1] == ')';

            if (!at_word_boundary) {
                continue;
            }

            size_t start = i;
            size_t end = i + 1;

            if (end >= len) {
                continue;
            }

            auto highlight_range = [&](size_t highlight_end) {
                ic_highlight(henv, static_cast<long>(start),
                             static_cast<long>(highlight_end - start), "cjsh-history-expansion");
                i = highlight_end - 1;
            };

            auto highlight_with_modifiers = [&](size_t highlight_end) {
                highlight_range(parse_history_modifiers(input, highlight_end, len));
            };

            char next = input[end];

            if (next == '!') {
                highlight_with_modifiers(end + 1);
                continue;
            }

            if (next == '#') {
                highlight_range(end + 1);
                continue;
            }

            if (next == '?') {
                ++end;
                while (end < len && input[end] != '?' &&
                       std::isspace(static_cast<unsigned char>(input[end])) == 0) {
                    ++end;
                }
                if (end < len && input[end] == '?') {
                    ++end;
                }
                highlight_range(end);
                continue;
            }

            if (next == '$' || next == '^' || next == '*') {
                highlight_with_modifiers(end + 1);
                continue;
            }

            if (std::isdigit(static_cast<unsigned char>(next)) || next == '-') {
                ++end;
                while (end < len && std::isdigit(static_cast<unsigned char>(input[end]))) {
                    ++end;
                }
                highlight_with_modifiers(end);
                continue;
            }

            if (std::isalpha(static_cast<unsigned char>(next)) || next == '_') {
                ++end;
                while (end < len && (std::isalnum(static_cast<unsigned char>(input[end])) ||
                                     input[end] == '_' || input[end] == '-' || input[end] == '.')) {
                    ++end;
                }
                highlight_with_modifiers(end);
                continue;
            }
        }
    }
}

}  // namespace highlight_helpers

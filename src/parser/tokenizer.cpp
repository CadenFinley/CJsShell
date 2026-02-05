/*
  tokenizer.cpp

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

#include "tokenizer.h"

#include <cctype>
#include <cstdlib>
#include <stdexcept>

#include "parser_utils.h"
#include "quote_info.h"
#include "shell_env.h"

namespace {
inline bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

}  // namespace

std::vector<std::string> Tokenizer::tokenize_command(const std::string& cmdline) {
    std::vector<std::string> tokens;
    tokens.reserve(std::min(cmdline.length() / 4 + 4, size_t(64)));
    std::string current_token;
    current_token.reserve(128);
    bool in_quotes = false;
    char quote_char = '\0';
    bool escaped = false;
    int arith_depth = 0;
    int brace_depth = 0;
    int bracket_depth = 0;
    bool in_subst_literal = false;

    bool token_saw_single = false;
    bool token_saw_double = false;

    const size_t cmdline_len = cmdline.length();

    auto flush_current_token = [&]() {
        if (!current_token.empty() || token_saw_single || token_saw_double) {
            if (token_saw_single || token_saw_double) {
                char quote_type = token_saw_double ? QUOTE_DOUBLE : QUOTE_SINGLE;
                tokens.push_back(create_quote_tag(quote_type, current_token));
            } else {
                tokens.push_back(current_token);
            }
            current_token.clear();
            token_saw_single = token_saw_double = false;
        }
    };

    const std::string& subst_start = subst_literal_start();
    const std::string& subst_end = subst_literal_end();

    for (size_t i = 0; i < cmdline_len; ++i) {
        if (!in_subst_literal && cmdline.compare(i, subst_start.size(), subst_start) == 0) {
            in_subst_literal = true;
            i += subst_start.size() - 1;
            continue;
        }

        if (in_subst_literal && cmdline.compare(i, subst_end.size(), subst_end) == 0) {
            in_subst_literal = false;
            i += subst_end.size() - 1;
            continue;
        }

        char c = cmdline[i];

        if (in_subst_literal) {
            current_token += c;
            continue;
        }

        if (escaped) {
            if (in_quotes && quote_char == '"') {
                if (c == '$') {
                    current_token += '\\';
                    current_token += c;
                } else if (c == '`' || c == '"' || c == '\\' || c == '\n') {
                    current_token += c;
                } else {
                    current_token += '\\';
                    current_token += c;
                }
            } else {
                if (c == '*' || c == '?' || c == '[' || c == ']') {
                    current_token += '\x1F';
                }
                current_token += c;
            }
            escaped = false;
        } else if (!in_subst_literal && c == '\\' && (!in_quotes || quote_char != '\'')) {
            escaped = true;
        } else if ((c == '"' || c == '\'') && !in_quotes && !in_subst_literal) {
            in_quotes = true;
            quote_char = c;
            if (c == '\'') {
                token_saw_single = true;
            } else {
                token_saw_double = true;
            }
        } else if (c == quote_char && in_quotes && !in_subst_literal) {
            in_quotes = false;
            quote_char = '\0';
        } else if (!in_quotes) {
            if (c == '{' && current_token.length() >= 1 && current_token.back() == '$') {
                brace_depth++;
                current_token += c;
            }

            else if (c == '}' && brace_depth > 0) {
                brace_depth--;
                current_token += c;
            }

            else if (is_whitespace(c)) {
                if ((!current_token.empty() || token_saw_single || token_saw_double) &&
                    arith_depth == 0 && brace_depth == 0) {
                    flush_current_token();
                } else if (arith_depth > 0 || brace_depth > 0) {
                    current_token += c;
                }
            }

            else if (c == '(' && i >= 1 && cmdline[i - 1] == '(' && current_token.length() >= 1 &&
                     current_token.back() == '$') {
                arith_depth++;
                current_token += c;
            }

            else if (c == ')' && i + 1 < cmdline_len && cmdline[i + 1] == ')' && arith_depth > 0) {
                arith_depth--;
                current_token += c;
                current_token += cmdline[i + 1];
                i++;
            }

            else if (c == '[' && i + 1 < cmdline_len && cmdline[i + 1] == '[') {
                bracket_depth++;
                flush_current_token();
                tokens.push_back("[[");
                i++;
            }

            else if (c == ']' && i + 1 < cmdline_len && cmdline[i + 1] == ']' &&
                     bracket_depth > 0) {
                bracket_depth--;
                flush_current_token();
                tokens.push_back("]]");
                i++;
            }

            else if (bracket_depth > 0 && i + 1 < cmdline_len &&
                     ((c == '&' && cmdline[i + 1] == '&') || (c == '|' && cmdline[i + 1] == '|'))) {
                flush_current_token();

                tokens.emplace_back(2, c);
                tokens.back()[1] = cmdline[i + 1];
                i++;
            }

            else if ((c == '(' || c == ')' || c == '<' || c == '>' ||
                      (c == '&' && arith_depth == 0 && brace_depth == 0 && bracket_depth == 0) ||
                      (c == '|' && arith_depth == 0 && brace_depth == 0 && bracket_depth == 0))) {
                flush_current_token();

                bool handled_special = false;
                if ((c == '<' || c == '>') && i + 1 < cmdline_len && cmdline[i + 1] == '(') {
                    size_t j = i + 2;
                    int paren_depth = 1;
                    bool in_single = false;
                    bool in_double = false;

                    while (j < cmdline_len) {
                        char ch = cmdline[j];
                        if (!in_double && ch == '\'' && (j == i + 2 || cmdline[j - 1] != '\\')) {
                            in_single = !in_single;
                        } else if (!in_single && ch == '"' &&
                                   (j == i + 2 || cmdline[j - 1] != '\\')) {
                            in_double = !in_double;
                        } else if (!in_single && !in_double) {
                            if (ch == '(') {
                                paren_depth++;
                            } else if (ch == ')') {
                                paren_depth--;
                                if (paren_depth == 0) {
                                    break;
                                }
                            }
                        }

                        if (ch == '\\' && !in_single) {
                            j += 2;
                            continue;
                        }
                        j++;
                    }

                    if (paren_depth == 0 && j < cmdline_len) {
                        tokens.push_back(cmdline.substr(i, j - i + 1));
                        i = j;
                        handled_special = true;
                    }
                }

                if (handled_special) {
                    continue;
                }

                if (c == '<' && i + 1 < cmdline_len && cmdline[i + 1] == '&') {
                    size_t j = i + 2;
                    while (j < cmdline_len &&
                           ((std::isdigit(static_cast<unsigned char>(cmdline[j])) != 0) ||
                            cmdline[j] == '-')) {
                        j++;
                    }
                    if (j > i + 2) {
                        tokens.push_back(cmdline.substr(i, j - i));
                        i = j - 1;
                        continue;
                    }
                }
                if (c == '>' && i + 1 < cmdline_len && cmdline[i + 1] == '&') {
                    size_t j = i + 2;
                    while (j < cmdline_len &&
                           ((std::isdigit(static_cast<unsigned char>(cmdline[j])) != 0) ||
                            cmdline[j] == '-')) {
                        j++;
                    }
                    if (j > i + 2) {
                        tokens.push_back(cmdline.substr(i, j - i));
                        i = j - 1;
                        continue;
                    }
                }
                if (c == '&' && !tokens.empty() &&
                    (tokens.back() == "<" || tokens.back() == ">" || tokens.back() == ">>") &&
                    i + 1 < cmdline_len) {
                    size_t j = i + 1;
                    while (j < cmdline_len &&
                           ((std::isdigit(static_cast<unsigned char>(cmdline[j])) != 0) ||
                            cmdline[j] == '-')) {
                        j++;
                    }
                    if (j > i + 1) {
                        size_t merged_size = tokens.back().size() + 1 + (j - i - 1);
                        std::string merged;
                        merged.reserve(merged_size);
                        merged = tokens.back();
                        merged += '&';
                        merged.append(cmdline, i + 1, j - i - 1);
                        tokens.back() = std::move(merged);
                        i = j - 1;
                        continue;
                    }
                }
                tokens.emplace_back(1, c);
            } else {
                current_token += c;
            }
        } else {
            current_token += c;
        }
    }

    flush_current_token();

    if (in_quotes) {
        throw std::runtime_error("cjsh: Unclosed quote: missing closing " +
                                 std::string(1, quote_char));
    }

    return tokens;
}

std::vector<std::string> Tokenizer::merge_redirection_tokens(
    const std::vector<std::string>& tokens) {
    std::vector<std::string> result;
    result.reserve(tokens.size());

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];

        if (token == "2" && i + 1 < tokens.size()) {
            if (tokens[i + 1] == ">&1") {
                result.push_back("2>&1");
                i++;
            } else if (i + 3 < tokens.size() && tokens[i + 1] == ">" && tokens[i + 2] == "&" &&
                       tokens[i + 3] == "1") {
                result.push_back("2>&1");
                i += 3;
            } else if (i + 2 < tokens.size() && tokens[i + 1] == ">" && tokens[i + 2] == ">") {
                result.push_back("2>>");
                i += 2;
            } else if (i + 1 < tokens.size() && tokens[i + 1] == ">") {
                result.push_back("2>");
                i++;
            } else {
                result.push_back(token);
            }
        }

        else if (token == "&" && i + 1 < tokens.size() && tokens[i + 1] == ">") {
            result.push_back("&>");
            i++;
        }

        else if (token == ">" && i + 1 < tokens.size() && tokens[i + 1] == "|") {
            result.push_back(">|");
            i++;
        }

        else if (token == "<" && i + 2 < tokens.size() && tokens[i + 1] == "<" &&
                 tokens[i + 2] == "<") {
            result.push_back("<<<");
            i += 2;
        }

        else if (token == "<" && i + 2 < tokens.size() && tokens[i + 1] == "<" &&
                 tokens[i + 2] == "-") {
            result.push_back("<<-");
            i += 2;
        }

        else if (token == "<" && i + 1 < tokens.size() && tokens[i + 1] == "<") {
            result.push_back("<<");
            i++;
        }

        else if (token == "<<" && i + 1 < tokens.size() && tokens[i + 1] == "<") {
            result.push_back("<<<");
            i++;
        }

        else if (token == "<<" && i + 1 < tokens.size() && tokens[i + 1] == "-") {
            result.push_back("<<-");
            i++;
        }

        else if (token == "<" && i + 2 < tokens.size() && tokens[i + 1] == "&" &&
                 tokens[i + 2].length() > 0 && (std::isdigit(tokens[i + 2][0]) != 0)) {
            result.push_back("<&" + tokens[i + 2]);
            i += 2;
        }

        else if (token == ">" && i + 1 < tokens.size() && tokens[i + 1] == ">") {
            result.push_back(">>");
            i++;
        }

        else if ((token == ">>" || token == ">") && i + 1 < tokens.size() &&
                 (tokens[i + 1] == "&1" || tokens[i + 1] == "&2")) {
            result.push_back(token + tokens[i + 1]);
            i++;
        }

        else if (token == ">" && i + 2 < tokens.size() && tokens[i + 1] == "&" &&
                 tokens[i + 2] == "2") {
            result.push_back(">&2");
            i += 2;
        }

        else if (token == ">" && i + 2 < tokens.size() && tokens[i + 1] == "&" &&
                 tokens[i + 2] == "1") {
            result.push_back(">&1");
            i += 2;
        }

        else if (token == "2" && i + 2 < tokens.size() && tokens[i + 1] == "&" &&
                 tokens[i + 2] == "1") {
            result.push_back("2>&1");
            i += 2;
        }

        else if (token == "2" && i + 1 < tokens.size() &&
                 (tokens[i + 1] == ">" || tokens[i + 1] == ">>")) {
            result.push_back("2" + tokens[i + 1]);
            i++;
        }

        else if ((std::isdigit(token[0]) != 0) && token.length() == 1 && i + 1 < tokens.size() &&
                 (tokens[i + 1] == "<" || tokens[i + 1] == ">")) {
            result.push_back(token + tokens[i + 1]);
            i++;
        }

        else if ((std::isdigit(token[0]) != 0) &&
                 token.find_first_not_of("0123456789") == std::string::npos &&
                 i + 1 < tokens.size() && tokens[i + 1].rfind("<&", 0) == 0) {
            result.push_back(token + tokens[i + 1]);
            i++;
        }

        else if ((std::isdigit(token[0]) != 0) &&
                 token.find_first_not_of("0123456789") == std::string::npos &&
                 i + 1 < tokens.size() && tokens[i + 1].rfind(">&", 0) == 0) {
            result.push_back(token + tokens[i + 1]);
            i++;
        }

        else if ((std::isdigit(token[0]) != 0) && token.length() > 1) {
            size_t first_non_digit = 0;
            while (first_non_digit < token.length() &&
                   (std::isdigit(token[first_non_digit]) != 0)) {
                first_non_digit++;
            }
            if (first_non_digit > 0 && first_non_digit < token.length()) {
                std::string rest = token.substr(first_non_digit);
                if (rest == "<" || rest == ">" || rest.find(">&") == 0) {
                    result.push_back(token);
                } else {
                    result.push_back(token);
                }
            } else {
                result.push_back(token);
            }
        } else {
            result.push_back(token);
        }
    }

    return result;
}

std::vector<std::string> Tokenizer::split_by_ifs(const std::string& input) {
    std::vector<std::string> result;

    std::string ifs = " \t\n";
    if (cjsh_env::shell_variable_is_set("IFS")) {
        ifs = cjsh_env::get_shell_variable_value("IFS");
    }

    if (input.empty()) {
        return result;
    }

    if (looks_like_assignment(input)) {
        result.push_back(input);
        return result;
    }

    if (ifs.empty()) {
        result.push_back(input);
        return result;
    }

    std::string current_word;
    bool in_word = false;
    int process_depth = 0;
    bool in_single = false;
    bool in_double = false;

    size_t idx = 0;
    while (idx < input.size()) {
        char c = input[idx];

        if (process_depth == 0) {
            if ((c == '<' || c == '>') && idx + 1 < input.size() && input[idx + 1] == '(') {
                if (!in_word) {
                    current_word.clear();
                }
                current_word += c;
                current_word += '(';
                in_word = true;
                process_depth = 1;
                idx += 2;
                continue;
            }

            if (ifs.find(c) != std::string::npos) {
                if (in_word) {
                    result.push_back(current_word);
                    current_word.clear();
                    in_word = false;
                }
                idx++;
                continue;
            }
        } else {
            if (!in_double && c == '\'' && (idx == 0 || input[idx - 1] != '\\')) {
                in_single = !in_single;
                current_word += c;
                idx++;
                continue;
            }
            if (!in_single && c == '"' && (idx == 0 || input[idx - 1] != '\\')) {
                in_double = !in_double;
                current_word += c;
                idx++;
                continue;
            }
            if (!in_single && !in_double) {
                if (c == '(') {
                    process_depth++;
                } else if (c == ')') {
                    process_depth--;
                    if (process_depth == 0) {
                        in_single = false;
                        in_double = false;
                    }
                }
            }
            if (c == '\\' && !in_single && idx + 1 < input.size()) {
                current_word += c;
                current_word += input[idx + 1];
                idx += 2;
                continue;
            }
        }

        current_word += c;
        in_word = true;
        idx++;
    }

    if (in_word) {
        result.push_back(current_word);
    }

    return result;
}

bool Tokenizer::looks_like_assignment(const std::string& input) {
    return ::looks_like_assignment(input);
}

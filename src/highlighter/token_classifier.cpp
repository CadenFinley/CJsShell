/*
  token_classifier.cpp

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

#include "token_classifier.h"

#include <cctype>
#include <string>

#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "shell.h"
#include "token_constants.h"

namespace token_classifier {

bool is_external_command(const std::string& token) {
    return !cjsh_filesystem::find_executable_in_path(token).empty();
}

bool is_shell_keyword(const std::string& token) {
    return token_constants::shell_keywords().count(token) > 0;
}

bool is_shell_builtin(const std::string& token) {
    if (g_shell != nullptr && g_shell->get_built_ins() != nullptr) {
        return g_shell->get_built_ins()->is_builtin_command(token);
    }
    return false;
}

bool is_variable_reference(const std::string& token) {
    if (token.empty())
        return false;

    if (token[0] == '$')
        return true;

    size_t eq_pos = token.find('=');
    if (eq_pos != std::string::npos && eq_pos > 0) {
        std::string var_name = token.substr(0, eq_pos);

        if (!var_name.empty() && ((std::isalpha(var_name[0]) != 0) || var_name[0] == '_')) {
            return true;
        }
    }

    return false;
}

bool is_quoted_string(const std::string& token, char& quote_type) {
    if (token.length() < 2)
        return false;

    char first = token[0];
    char last = token[token.length() - 1];

    if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
        quote_type = first;
        return true;
    }

    return false;
}

bool is_redirection_operator(const std::string& token) {
    if (token_constants::redirection_operators().count(token) > 0) {
        return true;
    }

    if (token.size() < 3) {
        return false;
    }

    size_t pos = 0;
    while (pos < token.size() && (std::isdigit(static_cast<unsigned char>(token[pos])) != 0)) {
        pos++;
    }

    if (pos >= token.size()) {
        return false;
    }

    if (token[pos] != '>' && token[pos] != '<') {
        return false;
    }
    pos++;

    if (pos >= token.size() || token[pos] != '&') {
        return false;
    }
    pos++;

    if (pos >= token.size()) {
        return false;
    }

    if (token[pos] == '-') {
        return pos + 1 == token.size();
    }

    if (std::isdigit(static_cast<unsigned char>(token[pos])) == 0) {
        return false;
    }

    while (pos < token.size() && (std::isdigit(static_cast<unsigned char>(token[pos])) != 0)) {
        pos++;
    }

    return pos == token.size();
}

bool is_glob_pattern(const std::string& token) {
    return token.find_first_of("*?[]{}") != std::string::npos;
}

bool is_option(const std::string& token) {
    if (token.size() < 2 || token[0] != '-') {
        return false;
    }

    if (token == "-" || token == "--") {
        return false;
    }

    if (token.rfind("--", 0) == 0) {
        return token.size() > 2;
    }

    bool numeric_like = true;
    for (size_t idx = 1; idx < token.size(); ++idx) {
        unsigned char uch = static_cast<unsigned char>(token[idx]);
        if ((std::isdigit(uch) == 0) && token[idx] != '.') {
            numeric_like = false;
            break;
        }
    }

    return !numeric_like;
}

bool is_numeric_literal(const std::string& token) {
    if (token.empty()) {
        return false;
    }

    size_t start = 0;
    if (token[start] == '+' || token[start] == '-') {
        start++;
    }

    if (start >= token.size()) {
        return false;
    }

    if (token.size() - start > 2 && token[start] == '0' &&
        (token[start + 1] == 'x' || token[start + 1] == 'X')) {
        if (start + 2 >= token.size()) {
            return false;
        }
        for (size_t idx = start + 2; idx < token.size(); ++idx) {
            unsigned char uch = static_cast<unsigned char>(token[idx]);
            if ((std::isdigit(uch) == 0) && (uch < 'a' || uch > 'f') && (uch < 'A' || uch > 'F')) {
                return false;
            }
        }
        return true;
    }

    bool saw_digit = false;
    bool saw_dot = false;
    bool saw_exponent = false;

    for (size_t i = start; i < token.size(); ++i) {
        unsigned char uch = static_cast<unsigned char>(token[i]);

        if ((std::isdigit(uch) != 0)) {
            saw_digit = true;
            continue;
        }

        if (token[i] == '.' && !saw_dot && !saw_exponent) {
            saw_dot = true;
            continue;
        }

        if ((token[i] == 'e' || token[i] == 'E') && !saw_exponent && saw_digit) {
            saw_exponent = true;
            saw_digit = false;
            if (i + 1 < token.size() && (token[i + 1] == '+' || token[i + 1] == '-')) {
                ++i;
            }
            continue;
        }

        return false;
    }

    return saw_digit;
}

bool is_function_definition(const std::string& input, size_t& func_name_start,
                            size_t& func_name_end) {
    func_name_start = 0;
    func_name_end = 0;

    const std::string& trimmed = input;

    size_t first_non_space = trimmed.find_first_not_of(" \t");
    if (first_non_space == std::string::npos) {
        return false;
    }

    if (trimmed.substr(first_non_space, 8) == "function") {
        size_t name_start = first_non_space + 8;

        while (name_start < trimmed.length() && (std::isspace(trimmed[name_start]) != 0)) {
            name_start++;
        }

        if (name_start >= trimmed.length()) {
            return false;
        }

        size_t name_end = name_start;
        while (name_end < trimmed.length() && (std::isspace(trimmed[name_end]) == 0) &&
               trimmed[name_end] != '{') {
            name_end++;
        }

        if (name_end > name_start) {
            func_name_start = name_start;
            func_name_end = name_end;
            return true;
        }
    }

    size_t paren_pos = trimmed.find("()");
    if (paren_pos != std::string::npos && paren_pos >= first_non_space) {
        size_t name_start = first_non_space;
        size_t name_end = paren_pos;

        while (name_end > name_start &&
               (std::isspace(static_cast<unsigned char>(trimmed[name_end - 1])) != 0)) {
            name_end--;
        }

        if (name_end > name_start) {
            std::string func_name = trimmed.substr(name_start, name_end - name_start);
            if (!func_name.empty() && func_name.find(' ') == std::string::npos &&
                func_name.find('\t') == std::string::npos) {
                func_name_start = name_start;
                func_name_end = name_end;
                return true;
            }
        }
    }

    return false;
}

}  // namespace token_classifier

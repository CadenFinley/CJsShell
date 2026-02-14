/*
  validation_variable_usage.cpp

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

#include "interpreter.h"

#include "interpreter_utils.h"
#include "parser_utils.h"
#include "validation_common.h"

#include <cctype>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

using shell_script_interpreter::detail::should_skip_line;
using shell_script_interpreter::detail::strip_inline_comment;
using shell_script_interpreter::detail::trim;
using namespace shell_validation::internal;

namespace {

enum class SeparatorToken : std::uint8_t {
    Semicolon,
    DoubleSemicolon,
    Pipe,
    Or,
    Amp,
    AmpCaret,
    AmpCaretBang,
    And,
    LParen,
    RParen,
    LBrace,
    RBrace,
    Do,
    Then,
    Elif,
    Fi,
    Done
};

const char* separator_token_text(SeparatorToken token) {
    switch (token) {
        case SeparatorToken::Semicolon:
            return ";";
        case SeparatorToken::DoubleSemicolon:
            return ";;";
        case SeparatorToken::Pipe:
            return "|";
        case SeparatorToken::Or:
            return "||";
        case SeparatorToken::Amp:
            return "&";
        case SeparatorToken::AmpCaret:
            return "&^";
        case SeparatorToken::AmpCaretBang:
            return "&^!";
        case SeparatorToken::And:
            return "&&";
        case SeparatorToken::LParen:
            return "(";
        case SeparatorToken::RParen:
            return ")";
        case SeparatorToken::LBrace:
            return "{";
        case SeparatorToken::RBrace:
            return "}";
        case SeparatorToken::Do:
            return "do";
        case SeparatorToken::Then:
            return "then";
        case SeparatorToken::Elif:
            return "elif";
        case SeparatorToken::Fi:
            return "fi";
        case SeparatorToken::Done:
            return "done";
    }
    return "";
}

enum class KeywordToken : std::uint8_t {
    If,
    Elif,
    While,
    Until,
    Then,
    Do
};

const char* keyword_token_text(KeywordToken token) {
    switch (token) {
        case KeywordToken::If:
            return "if";
        case KeywordToken::Elif:
            return "elif";
        case KeywordToken::While:
            return "while";
        case KeywordToken::Until:
            return "until";
        case KeywordToken::Then:
            return "then";
        case KeywordToken::Do:
            return "do";
    }
    return "";
}

struct TokenInfo {
    std::string text;
    size_t start;
    size_t end;
};

std::vector<TokenInfo> tokenize_shell_segment(const std::string& text, size_t start, size_t end) {
    std::vector<TokenInfo> tokens;
    if (start >= end || start >= text.size()) {
        return tokens;
    }

    size_t i = start;
    while (i < end) {
        while (i < end && (std::isspace(static_cast<unsigned char>(text[i])) != 0)) {
            ++i;
        }
        if (i >= end) {
            break;
        }

        if (i + 3 <= end) {
            const std::string three_chars = text.substr(i, 3);
            if (three_chars == "&^!") {
                tokens.push_back({three_chars, i, i + 3});
                i += 3;
                continue;
            }
        }

        if (i + 2 <= end) {
            const std::string two_chars = text.substr(i, 2);
            if (two_chars == "&&" || two_chars == "||" || two_chars == ";;" || two_chars == "&^") {
                tokens.push_back({two_chars, i, i + 2});
                i += 2;
                continue;
            }
        }

        if (text[i] == ';' || text[i] == '|' || text[i] == '&' || text[i] == '(' ||
            text[i] == ')' || text[i] == '{' || text[i] == '}') {
            tokens.push_back({std::string(1, text[i]), i, i + 1});
            ++i;
            continue;
        }

        size_t token_start = i;
        bool in_single = false;
        bool in_double = false;
        while (i < end) {
            char ch = text[i];
            if (ch == '\\' && !in_single && i + 1 < end) {
                i += 2;
                continue;
            }
            if (!in_double && ch == '\'') {
                in_single = !in_single;
                ++i;
                continue;
            }
            if (!in_single && ch == '"') {
                in_double = !in_double;
                ++i;
                continue;
            }
            if (!in_single && !in_double) {
                if ((std::isspace(static_cast<unsigned char>(ch)) != 0) || ch == ';' || ch == '|' ||
                    ch == '&' || ch == '(' || ch == ')' || ch == '{' || ch == '}') {
                    break;
                }
            }
            ++i;
        }
        tokens.push_back({text.substr(token_start, i - token_start), token_start, i});
    }

    return tokens;
}

bool is_command_separator_token(const std::string& token) {
    static const SeparatorToken separators[] = {
        SeparatorToken::Semicolon,    SeparatorToken::DoubleSemicolon,
        SeparatorToken::Pipe,         SeparatorToken::Or,
        SeparatorToken::Amp,          SeparatorToken::AmpCaret,
        SeparatorToken::AmpCaretBang, SeparatorToken::And,
        SeparatorToken::LParen,       SeparatorToken::RParen,
        SeparatorToken::LBrace,       SeparatorToken::RBrace,
        SeparatorToken::Do,           SeparatorToken::Then,
        SeparatorToken::Elif,         SeparatorToken::Fi,
        SeparatorToken::Done};
    for (const auto sep : separators) {
        if (token == separator_token_text(sep)) {
            return true;
        }
    }
    return false;
}

bool is_special_shell_variable(const std::string& name) {
    static const std::unordered_set<std::string> kSpecialVars = {
        "IFS",         "PATH",          "HOME",       "PWD",         "OLDPWD",     "MAIL",
        "MAILPATH",    "PS1",           "PS2",        "PS3",         "PS4",        "LANG",
        "LC_ALL",      "LC_CTYPE",      "LC_COLLATE", "LC_MESSAGES", "LC_NUMERIC", "OPTIND",
        "OPTARG",      "SECONDS",       "RANDOM",     "LINENO",      "HISTFILE",   "HISTSIZE",
        "HISTCONTROL", "PROMPT_COMMAND"};
    return kSpecialVars.find(name) != kSpecialVars.end();
}

bool is_test_context_token(const std::string& token) {
    return token == "[[" || token == "[" || token == "test";
}

bool is_assignment_token(const std::string& token) {
    size_t eq_pos = token.find('=');
    if (eq_pos == std::string::npos || eq_pos == 0) {
        return false;
    }
    if (token[0] == '$') {
        return false;
    }
    if (eq_pos + 1 < token.size() && (token[eq_pos + 1] == '=' || token[eq_pos + 1] == '~')) {
        return false;
    }
    return true;
}

std::string normalize_assignment_identifier(const std::string& token) {
    size_t eq_pos = token.find('=');
    if (eq_pos == std::string::npos) {
        return "";
    }

    std::string lhs = token.substr(0, eq_pos);
    if (!lhs.empty() && lhs.back() == '+') {
        lhs.pop_back();
    }

    size_t bracket_pos = lhs.find('[');
    if (bracket_pos != std::string::npos) {
        lhs = lhs.substr(0, bracket_pos);
    }

    return lhs;
}

void collect_leading_assignments_from_tokens(
    const std::vector<TokenInfo>& tokens, const std::string& original_line, size_t display_line,
    std::map<std::string, std::vector<size_t>>& defined_vars) {
    bool command_started = false;
    std::string previous_token;

    for (const auto& token : tokens) {
        if (token.text.empty()) {
            continue;
        }

        if (is_command_separator_token(token.text)) {
            command_started = false;
            previous_token.clear();
            continue;
        }

        if (!command_started) {
            if (!is_test_context_token(previous_token) && is_assignment_token(token.text)) {
                std::string var_name = normalize_assignment_identifier(token.text);
                if (!var_name.empty() && is_valid_identifier(var_name)) {
                    defined_vars[var_name].push_back(
                        adjust_display_line(original_line, display_line, token.start));
                }
                previous_token = token.text;
                continue;
            }
            command_started = true;
        }

        previous_token = token.text;
    }
}

size_t find_unquoted_keyword(const std::string& line, const std::string& keyword,
                             size_t search_from) {
    if (keyword.empty() || search_from >= line.size()) {
        return std::string::npos;
    }

    QuoteState state;
    for (size_t i = search_from; i + keyword.size() <= line.size(); ++i) {
        char c = line[i];
        if (!should_process_char(state, c, false)) {
            continue;
        }

        if (line.compare(i, keyword.size(), keyword) == 0 &&
            is_word_boundary(line, i, keyword.size())) {
            return i;
        }
    }

    return std::string::npos;
}

void detect_keyword_assignments(const std::string& line_without_comments,
                                const std::string& trimmed_line, const std::string& original_line,
                                size_t display_line,
                                std::map<std::string, std::vector<size_t>>& defined_vars) {
    struct KeywordInfo {
        KeywordToken keyword;
        KeywordToken terminator;
    };

    static const KeywordInfo kKeywordInfos[] = {{KeywordToken::If, KeywordToken::Then},
                                                {KeywordToken::Elif, KeywordToken::Then},
                                                {KeywordToken::While, KeywordToken::Do},
                                                {KeywordToken::Until, KeywordToken::Do}};

    for (const auto& info : kKeywordInfos) {
        const char* keyword_text = keyword_token_text(info.keyword);
        const char* terminator_text = keyword_token_text(info.terminator);
        if (!starts_with_keyword_token(trimmed_line, keyword_text)) {
            continue;
        }

        size_t keyword_pos = line_without_comments.find(keyword_text);
        if (keyword_pos == std::string::npos) {
            continue;
        }

        size_t command_start = keyword_pos + std::strlen(keyword_text);
        while (
            command_start < line_without_comments.size() &&
            (std::isspace(static_cast<unsigned char>(line_without_comments[command_start])) != 0)) {
            ++command_start;
        }

        size_t command_end =
            find_unquoted_keyword(line_without_comments, terminator_text, command_start);
        if (command_end == std::string::npos) {
            command_end = line_without_comments.size();
        }

        if (command_end <= command_start) {
            continue;
        }

        auto tokens = tokenize_shell_segment(line_without_comments, command_start, command_end);
        collect_leading_assignments_from_tokens(tokens, original_line, display_line, defined_vars);
    }
}

bool read_option_consumes_argument(const std::string& option) {
    if (option.size() < 2 || option[0] != '-') {
        return false;
    }

    char flag = option[1];
    switch (flag) {
        case 'p':
        case 'u':
        case 't':
        case 'd':
        case 'N':
        case 'n':
        case 'i':
        case 'k':
            return option.size() == 2;
        default:
            return false;
    }
}

void collect_read_variable_definitions(const std::vector<TokenInfo>& tokens,
                                       const std::string& original_line, size_t display_line,
                                       std::map<std::string, std::vector<size_t>>& defined_vars) {
    size_t idx = 0;
    while (idx < tokens.size()) {
        if (tokens[idx].text != "read") {
            ++idx;
            continue;
        }

        size_t j = idx + 1;
        while (j < tokens.size()) {
            const auto& current = tokens[j];
            if (is_command_separator_token(current.text)) {
                break;
            }

            if (!current.text.empty() && current.text[0] == '-') {
                bool consumes_next = read_option_consumes_argument(current.text);
                ++j;
                if (consumes_next && j < tokens.size() &&
                    !is_command_separator_token(tokens[j].text) && !tokens[j].text.empty() &&
                    tokens[j].text[0] != '-') {
                    ++j;
                }
                continue;
            }

            if (!current.text.empty() && (current.text[0] == '<' || current.text[0] == '>')) {
                ++j;
                continue;
            }

            std::string var_name = extract_identifier_from_token(current.text);
            if (!var_name.empty() && is_valid_identifier(var_name)) {
                defined_vars[var_name].push_back(
                    adjust_display_line(original_line, display_line, current.start));
            }
            ++j;
        }

        idx = j;
    }
}

}  // namespace

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::validate_variable_usage(
    const std::vector<std::string>& lines) {
    std::vector<SyntaxError> errors;
    std::map<std::string, std::vector<size_t>> defined_vars;
    std::map<std::string, std::vector<size_t>> used_vars;

    for (size_t line_num = 0; line_num < lines.size(); ++line_num) {
        const std::string& original_line = lines[line_num];
        size_t display_line = line_num + 1;

        if (should_skip_line(original_line)) {
            continue;
        }

        std::string line_without_comments = strip_inline_comment(original_line);
        std::string trimmed_line = trim(line_without_comments);
        if (trimmed_line.empty()) {
            continue;
        }

        if (starts_with_keyword_token(trimmed_line, "for")) {
            auto tokens = tokenize_whitespace(trimmed_line);
            if (tokens.size() >= 2) {
                std::string loop_var = extract_identifier_from_token(tokens[1]);
                if (!loop_var.empty() && is_valid_identifier(loop_var)) {
                    size_t var_pos = line_without_comments.find(loop_var);
                    size_t offset = (var_pos != std::string::npos) ? var_pos : 0;
                    defined_vars[loop_var].push_back(
                        adjust_display_line(original_line, display_line, offset));
                }
            }
        }

        {
            auto export_tokens =
                tokenize_shell_segment(line_without_comments, 0, line_without_comments.size());
            bool command_started = false;

            for (size_t i = 0; i < export_tokens.size(); ++i) {
                const auto& token = export_tokens[i];
                if (token.text.empty()) {
                    continue;
                }

                if (is_command_separator_token(token.text)) {
                    command_started = false;
                    continue;
                }

                if (!command_started && is_assignment_token(token.text)) {
                    continue;
                }

                if (!command_started && token.text == "export") {
                    for (size_t j = i + 1; j < export_tokens.size(); ++j) {
                        const auto& arg = export_tokens[j];
                        if (arg.text.empty()) {
                            continue;
                        }
                        if (is_command_separator_token(arg.text)) {
                            break;
                        }
                        if (!arg.text.empty() && arg.text[0] == '-') {
                            continue;
                        }

                        if (arg.text.find('=') != std::string::npos &&
                            !is_assignment_token(arg.text)) {
                            continue;
                        }

                        std::string exported_name = is_assignment_token(arg.text)
                                                        ? normalize_assignment_identifier(arg.text)
                                                        : arg.text;
                        if (!exported_name.empty() && is_valid_identifier(exported_name)) {
                            defined_vars[exported_name].push_back(
                                adjust_display_line(original_line, display_line, arg.start));
                        }
                    }
                }

                command_started = true;
            }
        }

        detect_keyword_assignments(line_without_comments, trimmed_line, original_line, display_line,
                                   defined_vars);

        auto tokens =
            tokenize_shell_segment(line_without_comments, 0, line_without_comments.size());
        collect_read_variable_definitions(tokens, original_line, display_line, defined_vars);

        size_t eq_pos = line_without_comments.find('=');
        if (eq_pos != std::string::npos) {
            std::string before_eq = line_without_comments.substr(0, eq_pos);

            size_t start = before_eq.find_first_not_of(" \t");
            if (start != std::string::npos) {
                before_eq = before_eq.substr(start);
                before_eq = trim(before_eq);

                if (is_valid_identifier(before_eq)) {
                    defined_vars[before_eq].push_back(
                        adjust_display_line(original_line, display_line, eq_pos));
                }
            }
        }

        QuoteState quote_state;
        for (size_t i = 0; i < line_without_comments.length(); ++i) {
            char c = line_without_comments[i];

            if (!should_process_char(quote_state, c, true)) {
                continue;
            }

            if (c == '$' && i + 1 < line_without_comments.length()) {
                if (i + 2 < line_without_comments.length() && line_without_comments[i + 1] == '(' &&
                    line_without_comments[i + 2] == '(') {
                    const auto bounds =
                        analyze_arithmetic_expansion_bounds(line_without_comments, i);

                    if (bounds.closed) {
                        std::string expr = line_without_comments.substr(
                            bounds.expr_start, bounds.expr_end - bounds.expr_start);

                        size_t pos = 0;
                        while (pos < expr.length()) {
                            char ec = expr[pos];
                            if ((std::isalpha(static_cast<unsigned char>(ec)) != 0) || ec == '_') {
                                size_t start_pos = pos;
                                pos++;
                                while (
                                    pos < expr.length() &&
                                    ((std::isalnum(static_cast<unsigned char>(expr[pos])) != 0) ||
                                     expr[pos] == '_')) {
                                    pos++;
                                }

                                std::string token = expr.substr(start_pos, pos - start_pos);
                                if (!token.empty() && is_valid_identifier(token)) {
                                    used_vars[token].push_back(
                                        adjust_display_line(original_line, display_line,
                                                            bounds.expr_start + start_pos));
                                }
                            } else {
                                pos++;
                            }
                        }

                        i = (bounds.closing_index == 0) ? i : bounds.closing_index - 1;
                        continue;
                    }
                }

                std::string var_name;
                size_t var_start = i + 1;
                size_t var_end = var_start;

                if (line_without_comments[var_start] == '{') {
                    var_start++;
                    var_end = line_without_comments.find('}', var_start);
                    if (var_end != std::string::npos) {
                        var_name = line_without_comments.substr(var_start, var_end - var_start);

                        size_t colon_pos = var_name.find(':');
                        if (colon_pos != std::string::npos) {
                            var_name = var_name.substr(0, colon_pos);
                        }
                    } else {
                        errors.push_back(SyntaxError({display_line, i, i + 2, 0},
                                                     ErrorSeverity::CRITICAL, ErrorCategory::SYNTAX,
                                                     "SYN008", "Unclosed variable expansion ${",
                                                     original_line, "Add closing brace '}'"));
                        continue;
                    }
                } else if ((std::isalpha(line_without_comments[var_start]) != 0) ||
                           line_without_comments[var_start] == '_') {
                    while (var_end < line_without_comments.length() &&
                           ((std::isalnum(line_without_comments[var_end]) != 0) ||
                            line_without_comments[var_end] == '_')) {
                        var_end++;
                    }
                    var_name = line_without_comments.substr(var_start, var_end - var_start);
                }

                if (!var_name.empty()) {
                    used_vars[var_name].push_back(
                        adjust_display_line(original_line, display_line, i));
                }
            }
        }
    }

    for (const auto& [var_name, usage_lines] : used_vars) {
        const bool defined_in_script = defined_vars.find(var_name) != defined_vars.end();
        const bool known_to_environment = variable_is_set(var_name);

        if (!defined_in_script && !known_to_environment) {
            if ((std::isdigit(static_cast<unsigned char>(var_name[0])) == 0)) {
                for (size_t line : usage_lines) {
                    errors.push_back(SyntaxError(
                        {line, 0, 0, 0}, ErrorSeverity::WARNING, ErrorCategory::VARIABLES, "VAR002",
                        "Variable '" + var_name + "' used but not defined in this script", "",
                        "Define the variable before use: " + var_name + "=value"));
                }
            }
        }
    }

    for (const auto& [var_name, def_lines] : defined_vars) {
        if (is_special_shell_variable(var_name)) {
            continue;
        }
        if (used_vars.find(var_name) == used_vars.end()) {
            for (size_t line : def_lines) {
                errors.push_back(SyntaxError({line, 0, 0, 0}, ErrorSeverity::INFO,
                                             ErrorCategory::VARIABLES, "VAR003",
                                             "Variable '" + var_name + "' defined but never used",
                                             "", "Remove unused variable or add usage"));
            }
        }
    }

    return errors;
}

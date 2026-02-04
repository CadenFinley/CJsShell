/*
  conditional_evaluator.cpp

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

#include "conditional_evaluator.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <optional>

#include "cjsh.h"
#include "interpreter_utils.h"
#include "parser.h"

using shell_script_interpreter::detail::process_line_for_validation;
using shell_script_interpreter::detail::strip_inline_comment;
using shell_script_interpreter::detail::trim;

namespace {

bool starts_with_keyword(const std::string& text, const std::string& keyword) {
    if (text.size() < keyword.size())
        return false;
    if (text.compare(0, keyword.size(), keyword) != 0)
        return false;
    if (text.size() == keyword.size())
        return true;
    char next = text[keyword.size()];
    return std::isspace(static_cast<unsigned char>(next)) != 0;
}

bool is_if_token(const std::string& token) {
    return token == "if" || token.rfind("if ", 0) == 0;
}

bool handle_quote_char(char c, bool& in_quotes, char& quote_char, std::string* output) {
    if (!in_quotes) {
        if (c == '"' || c == '\'' || c == '`') {
            in_quotes = true;
            quote_char = c;
            if (output)
                output->push_back(c);
            return true;
        }
        return false;
    }

    if (c == quote_char) {
        in_quotes = false;
        quote_char = '\0';
    }
    if (output)
        output->push_back(c);
    return true;
}

void update_group_depths(char c, int& bracket_depth, int& paren_depth) {
    if (c == '[') {
        bracket_depth++;
    } else if (c == ']') {
        bracket_depth--;
    } else if (c == '(') {
        paren_depth++;
    } else if (c == ')') {
        paren_depth--;
    }
}

bool is_fi_token_boundary(const std::string& text, size_t pos) {
    bool is_word_end =
        (pos + 2 >= text.length()) || (text[pos + 2] == ' ') || (text[pos + 2] == ';');
    bool is_word_start = (pos == 0) || (text[pos - 1] == ' ') || (text[pos - 1] == ';');
    return is_word_start && is_word_end;
}

std::vector<std::string> split_top_level_semicolons(const std::string& text) {
    std::vector<std::string> segments;
    segments.reserve(8);
    std::string current;
    current.reserve(text.size());

    bool in_single = false;
    bool in_double = false;
    bool in_backtick = false;
    bool escape_next = false;
    int paren_depth = 0;
    int brace_depth = 0;
    int bracket_depth = 0;

    auto flush_segment = [&]() {
        std::string trimmed = trim(current);
        if (!trimmed.empty())
            segments.push_back(trimmed);
        current.clear();
    };

    for (char c : text) {
        if (escape_next) {
            current.push_back(c);
            escape_next = false;
            continue;
        }

        if (!in_single && c == '\\') {
            escape_next = true;
            current.push_back(c);
            continue;
        }

        if (!in_double && !in_backtick && c == '\'') {
            in_single = !in_single;
            current.push_back(c);
            continue;
        }

        if (!in_single && !in_backtick && c == '"') {
            in_double = !in_double;
            current.push_back(c);
            continue;
        }

        if (!in_single && !in_double && c == '`') {
            in_backtick = !in_backtick;
            current.push_back(c);
            continue;
        }

        if (!in_single && !in_double && !in_backtick) {
            if (c == '(') {
                paren_depth++;
            } else if (c == ')') {
                if (paren_depth > 0)
                    paren_depth--;
            } else if (c == '{') {
                brace_depth++;
            } else if (c == '}') {
                if (brace_depth > 0)
                    brace_depth--;
            } else if (c == '[') {
                bracket_depth++;
            } else if (c == ']') {
                if (bracket_depth > 0)
                    bracket_depth--;
            } else if (c == ';' && paren_depth == 0 && brace_depth == 0 && bracket_depth == 0) {
                flush_segment();
                continue;
            }
        }

        current.push_back(c);
    }

    if (!current.empty())
        flush_segment();

    return segments;
}

void expand_segment(const std::string& segment, std::vector<std::string>& out) {
    std::string cleaned = trim(strip_inline_comment(segment));
    if (cleaned.empty())
        return;

    if (starts_with_keyword(cleaned, "then")) {
        out.push_back("then");
        std::string remainder = trim(cleaned.substr(4));
        if (!remainder.empty())
            expand_segment(remainder, out);
        return;
    }

    if (starts_with_keyword(cleaned, "else")) {
        out.push_back("else");
        std::string remainder = trim(cleaned.substr(4));
        if (!remainder.empty())
            expand_segment(remainder, out);
        return;
    }

    if (starts_with_keyword(cleaned, "elif")) {
        out.push_back(cleaned);
        return;
    }

    out.push_back(cleaned);
}

struct ExpandedSingleLineIf {
    std::vector<std::string> lines;
    std::string trailing_commands;
};

std::optional<ExpandedSingleLineIf> expand_single_line_if(const std::string& line) {
    std::string cleaned = trim(strip_inline_comment(line));
    if (cleaned.empty())
        return std::nullopt;

    if (!is_if_token(cleaned))
        return std::nullopt;

    if (cleaned.find("then") == std::string::npos)
        return std::nullopt;

    auto segments = split_top_level_semicolons(cleaned);
    if (segments.empty())
        return std::nullopt;

    std::vector<std::string> tokens;
    tokens.reserve(segments.size() * 2);
    for (const auto& seg : segments) {
        expand_segment(seg, tokens);
    }

    if (tokens.empty())
        return std::nullopt;

    std::vector<std::string> block_lines;
    std::vector<std::string> trailing_tokens;
    int depth = 0;
    bool started = false;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& tok = tokens[i];

        if (!started) {
            if (!is_if_token(tok))
                return std::nullopt;
            started = true;
        }

        block_lines.push_back(tok);

        if (is_if_token(tok)) {
            depth++;
        } else if (tok == "fi") {
            depth--;
            if (depth < 0)
                return std::nullopt;
        }

        if (depth == 0) {
            if (i + 1 < tokens.size()) {
                auto start = std::next(tokens.begin(), static_cast<std::ptrdiff_t>(i + 1));
                trailing_tokens.insert(trailing_tokens.end(), start, tokens.end());
            }
            break;
        }
    }

    if (!started || depth != 0)
        return std::nullopt;

    std::string trailing;
    for (const auto& t : trailing_tokens) {
        if (trailing.empty()) {
            trailing = t;
        } else {
            trailing.append("; ");
            trailing.append(t);
        }
    }

    return ExpandedSingleLineIf{std::move(block_lines), trailing};
}

}  // namespace

namespace conditional_evaluator {

int handle_if_block(const std::vector<std::string>& src_lines, size_t& idx,
                    const std::function<int(const std::vector<std::string>&)>& execute_block,
                    const std::function<int(const std::string&)>& execute_simple_or_pipeline,
                    const std::function<int(const std::string&)>& evaluate_logical_condition,
                    Parser* shell_parser) {
    if (src_lines.size() == 1 && shell_parser != nullptr) {
        const std::string& line = src_lines[idx];
        bool has_elif =
            (line.find(" elif ") != std::string::npos || line.find(";elif ") != std::string::npos ||
             line.find("; elif") != std::string::npos);

        bool has_trailing_commands = false;
        size_t fi_pos = line.find(" fi");
        if (fi_pos == std::string::npos)
            fi_pos = line.find(";fi");
        if (fi_pos != std::string::npos) {
            size_t after_fi = (line[fi_pos] == ' ') ? fi_pos + 3 : fi_pos + 3;
            while (after_fi < line.length() &&
                   std::isspace(static_cast<unsigned char>(line[after_fi])))
                after_fi++;
            if (after_fi < line.length() && line[after_fi] == ';')
                after_fi++;
            while (after_fi < line.length() &&
                   std::isspace(static_cast<unsigned char>(line[after_fi])))
                after_fi++;
            has_trailing_commands = (after_fi < line.length());
        }

        if (has_elif || has_trailing_commands) {
            if (auto expanded = expand_single_line_if(src_lines[idx])) {
                size_t local_idx = 0;
                int rc = handle_if_block(expanded->lines, local_idx, execute_block,
                                         execute_simple_or_pipeline, evaluate_logical_condition,
                                         shell_parser);

                if (!expanded->trailing_commands.empty() && rc != 253 && rc != 254 && rc != 255 &&
                    !g_exit_flag) {
                    auto trailing_cmds =
                        shell_parser->parse_semicolon_commands(expanded->trailing_commands);
                    for (const auto& cmd : trailing_cmds) {
                        int follow_rc = execute_simple_or_pipeline(cmd);
                        rc = follow_rc;
                        if (follow_rc != 0 || g_exit_flag)
                            break;
                    }
                }

                idx = 0;
                return rc;
            }
        }
    }

    std::string first = process_line_for_validation(src_lines[idx]);

    std::string cond_accum;
    if (first.rfind("if ", 0) == 0) {
        cond_accum = first.substr(3);
    } else if (first == "if") {
    } else {
        return 1;
    }

    size_t j = idx;
    bool then_found = false;

    auto pos = first.find("; then");
    if (pos == std::string::npos) {
        pos = first.find(";then");
    }
    if (pos != std::string::npos) {
        cond_accum = trim(first.substr(3, pos - 3));
        then_found = true;
    } else {
        while (!then_found && ++j < src_lines.size()) {
            std::string cur = trim(strip_inline_comment(src_lines[j]));
            if (cur == "then") {
                then_found = true;
                break;
            }
            auto p = cur.rfind("; then");
            if (p == std::string::npos) {
                p = cur.rfind(";then");
            }
            if (p != std::string::npos) {
                if (!cond_accum.empty())
                    cond_accum += " ";
                cond_accum += cur.substr(0, p);
                then_found = true;
                break;
            }
            if (!cur.empty()) {
                if (!cond_accum.empty())
                    cond_accum += " ";
                cond_accum += cur;
            }
        }
    }

    if (!then_found) {
        idx = j;
        return 1;
    }

    int cond_rc = 1;
    if (!cond_accum.empty()) {
        cond_rc = evaluate_logical_condition(cond_accum);
    }

    if (pos != std::string::npos) {
        std::string rem = trim(first.substr(pos + 6));

        if (!rem.empty()) {
            size_t fi_pos = std::string::npos;
            int if_depth = 1;
            size_t search_pos = 0;
            bool in_quotes = false;
            char quote_char = '\0';

            while (search_pos < rem.length() && if_depth > 0) {
                char c = rem[search_pos];

                if (handle_quote_char(c, in_quotes, quote_char, nullptr)) {
                    search_pos++;
                    continue;
                }

                if (!in_quotes) {
                    if (search_pos + 3 < rem.length() && rem.substr(search_pos, 3) == "if ") {
                        if_depth++;
                        search_pos += 2;
                    }

                    else if (search_pos + 2 <= rem.length() && rem.substr(search_pos, 2) == "fi") {
                        bool is_word_start =
                            (search_pos == 0 ||
                             std::isalnum(static_cast<unsigned char>(rem[search_pos - 1])) == 0);
                        bool is_word_end =
                            (search_pos + 2 >= rem.length() ||
                             std::isalnum(static_cast<unsigned char>(rem[search_pos + 2])) == 0);
                        if (is_word_start && is_word_end) {
                            if_depth--;
                            if (if_depth == 0) {
                                fi_pos = search_pos;
                                break;
                            }
                        }
                    }
                }
                search_pos++;
            }

            bool has_top_level_elif = false;
            if (fi_pos != std::string::npos) {
                size_t check_pos = 0;
                int nested_depth = 0;
                bool check_in_quotes = false;
                char check_quote = '\0';

                while (check_pos < fi_pos) {
                    char ch = rem[check_pos];

                    if (handle_quote_char(ch, check_in_quotes, check_quote, nullptr)) {
                        check_pos++;
                        continue;
                    }

                    if (!check_in_quotes) {
                        if (check_pos + 3 <= fi_pos && rem.substr(check_pos, 3) == "if ") {
                            nested_depth++;
                            check_pos += 2;
                        } else if (check_pos + 2 <= fi_pos && rem.substr(check_pos, 2) == "fi") {
                            bool is_word =
                                (check_pos == 0 || std::isalnum(static_cast<unsigned char>(
                                                       rem[check_pos - 1])) == 0) &&
                                (check_pos + 2 >= fi_pos ||
                                 std::isalnum(static_cast<unsigned char>(rem[check_pos + 2])) == 0);
                            if (is_word && nested_depth > 0) {
                                nested_depth--;
                            }
                        } else if (nested_depth == 0) {
                            bool matches_plain_elif =
                                (check_pos + 5 <= fi_pos && rem.substr(check_pos, 5) == "elif ");
                            bool matches_semicolon_elif =
                                (check_pos + 6 <= fi_pos && rem.substr(check_pos, 6) == "; elif");
                            if (matches_plain_elif || matches_semicolon_elif) {
                                has_top_level_elif = true;
                                break;
                            }
                        }
                    }
                    check_pos++;
                }
            }

            if (fi_pos != std::string::npos && !has_top_level_elif) {
                std::string body = trim(rem.substr(0, fi_pos));

                std::string then_body = body;
                std::string else_body;

                size_t else_pos = std::string::npos;
                int nested_if_depth = 0;
                size_t search_else = 0;
                in_quotes = false;
                quote_char = '\0';

                while (search_else < body.length()) {
                    char c = body[search_else];

                    if (handle_quote_char(c, in_quotes, quote_char, nullptr)) {
                        search_else++;
                        continue;
                    }

                    if (!in_quotes) {
                        if (search_else + 3 < body.length() &&
                            body.substr(search_else, 3) == "if ") {
                            nested_if_depth++;
                            search_else += 2;
                        }

                        else if (search_else + 2 <= body.length() &&
                                 body.substr(search_else, 2) == "fi") {
                            bool is_word_start =
                                (search_else == 0 || std::isalnum(static_cast<unsigned char>(
                                                         body[search_else - 1])) == 0);
                            bool is_word_end =
                                (search_else + 2 >= body.length() ||
                                 std::isalnum(static_cast<unsigned char>(body[search_else + 2])) ==
                                     0);
                            if (is_word_start && is_word_end && nested_if_depth > 0) {
                                nested_if_depth--;
                            }
                        }

                        else if (nested_if_depth == 0) {
                            if (search_else + 6 < body.length() &&
                                body.substr(search_else, 6) == "; else") {
                                else_pos = search_else;
                                break;
                            }
                            if (search_else + 5 < body.length() &&
                                body.substr(search_else, 5) == " else") {
                                else_pos = search_else;
                                break;
                            }
                        }
                    }
                    search_else++;
                }
                if (else_pos != std::string::npos) {
                    then_body = trim(body.substr(0, else_pos));
                    else_body = trim(body.substr(else_pos + 6));
                } else {
                }
                int body_rc = 0;
                if (cond_rc == 0) {
                    auto cmds = shell_parser->parse_semicolon_commands(then_body);
                    for (const auto& c : cmds) {
                        int rc2 = execute_simple_or_pipeline(c);
                        body_rc = rc2;
                        if (rc2 != 0)
                            break;
                    }
                } else if (!else_body.empty()) {
                    auto cmds = shell_parser->parse_semicolon_commands(else_body);
                    for (const auto& c : cmds) {
                        int rc2 = execute_simple_or_pipeline(c);
                        body_rc = rc2;
                        if (rc2 != 0)
                            break;
                    }
                }

                if (body_rc != 253 && body_rc != 254 && body_rc != 255 && !g_exit_flag) {
                    size_t after_fi_pos = fi_pos + 2;
                    while (after_fi_pos < rem.length() &&
                           std::isspace(static_cast<unsigned char>(rem[after_fi_pos])) != 0) {
                        after_fi_pos++;
                    }
                    if (after_fi_pos < rem.length() && rem[after_fi_pos] == ';') {
                        after_fi_pos++;
                        while (after_fi_pos < rem.length() &&
                               std::isspace(static_cast<unsigned char>(rem[after_fi_pos])) != 0) {
                            after_fi_pos++;
                        }
                    }
                    if (after_fi_pos < rem.length()) {
                        std::string after_commands = trim(rem.substr(after_fi_pos));
                        if (!after_commands.empty()) {
                            auto after_cmds =
                                shell_parser->parse_semicolon_commands(after_commands);
                            for (const auto& c : after_cmds) {
                                int rc3 = execute_simple_or_pipeline(c);
                                body_rc = rc3;
                                if (rc3 != 0 || g_exit_flag)
                                    break;
                            }
                        }
                    }
                }

                return body_rc;
            }
        }
    }

    size_t k = j + 1;
    int depth = 1;
    bool in_else = false;
    std::vector<std::string> then_lines;
    std::vector<std::string> else_lines;

    bool is_simple_single_line = false;

    if (src_lines.size() == 1 && src_lines[0].find("fi") != std::string::npos) {
        const std::string& line = src_lines[0];

        size_t if_count = 0;
        size_t fi_count = 0;
        size_t pos = 0;
        while ((pos = line.find(" if ", pos)) != std::string::npos) {
            if_count++;
            pos += 4;
        }
        if (line.rfind("if ", 0) == 0)
            if_count++;
        pos = 0;
        while ((pos = line.find("fi", pos)) != std::string::npos) {
            bool is_word =
                (pos == 0 || std::isalnum(static_cast<unsigned char>(line[pos - 1])) == 0) &&
                (pos + 2 >= line.length() ||
                 std::isalnum(static_cast<unsigned char>(line[pos + 2])) == 0);
            if (is_word)
                fi_count++;
            pos += 2;
        }
        is_simple_single_line = (if_count == 1 && fi_count == 1);
    }

    if (is_simple_single_line) {
        const std::string& full_line = src_lines[0];

        std::vector<std::string> parts;

        size_t if_pos = full_line.find("if ");
        size_t then_pos = full_line.find("; then");
        if (if_pos != std::string::npos && then_pos != std::string::npos) {
            std::string condition = trim(full_line.substr(if_pos + 3, then_pos - (if_pos + 3)));

            int cond_result = execute_simple_or_pipeline(condition);

            std::string remaining = trim(full_line.substr(then_pos + 6));

            std::vector<std::pair<std::string, std::string>> branches;

            size_t pos = 0;
            bool condition_met = false;

            while (pos < remaining.length()) {
                size_t elif_pos = remaining.find("; elif ", pos);
                if (elif_pos == std::string::npos) {
                    size_t elif_semi = remaining.find("; elif;", pos);
                    if (elif_semi != std::string::npos) {
                        elif_pos = elif_semi;
                    }
                }
                size_t else_pos = remaining.find("; else ", pos);

                size_t fi_pos = std::string::npos;
                size_t search_pos = pos;
                while (search_pos < remaining.length()) {
                    size_t candidate = remaining.find("fi", search_pos);
                    if (candidate == std::string::npos)
                        break;

                    if (is_fi_token_boundary(remaining, candidate)) {
                        fi_pos = candidate;
                        break;
                    }
                    search_pos = candidate + 1;
                }

                size_t next_pos = std::min({elif_pos, else_pos, fi_pos});
                if (next_pos == std::string::npos)
                    break;

                std::string commands = trim(remaining.substr(pos, next_pos - pos));

                if (elif_pos != std::string::npos && next_pos == elif_pos) {
                    if (pos == 0) {
                        if (cond_result == 0 && !condition_met) {
                            auto cmds = shell_parser->parse_semicolon_commands(commands);
                            for (const auto& c : cmds) {
                                execute_simple_or_pipeline(c);
                            }
                            idx = 0;
                            return 0;
                        }
                    }

                    size_t skip_len = 7;
                    if (elif_pos != std::string::npos && elif_pos + 6 < remaining.length() &&
                        remaining[elif_pos + 6] == ';') {
                        skip_len = 6;
                    }

                    pos = next_pos + skip_len;
                    size_t elif_then = remaining.find("; then", pos);
                    if (elif_then == std::string::npos) {
                        elif_then = remaining.find(";then", pos);
                    }
                    if (elif_then != std::string::npos) {
                        std::string elif_cond = trim(remaining.substr(pos, elif_then - pos));

                        if (elif_cond.empty()) {
                            idx = 0;
                            return 2;
                        }
                        int elif_result = evaluate_logical_condition(elif_cond);

                        if (elif_result == 0 && !condition_met) {
                            size_t elif_body_start = elif_then + 6;

                            size_t next_elif = remaining.find("; elif ", elif_body_start);
                            size_t next_else = remaining.find("; else ", elif_body_start);
                            size_t next_fi = std::string::npos;
                            size_t search_fi = elif_body_start;
                            while (search_fi < remaining.length()) {
                                size_t candidate = remaining.find("fi", search_fi);
                                if (candidate == std::string::npos)
                                    break;

                                if (is_fi_token_boundary(remaining, candidate)) {
                                    next_fi = candidate;
                                    break;
                                }
                                search_fi = candidate + 1;
                            }

                            size_t elif_body_end = std::min({next_elif, next_else, next_fi});
                            if (elif_body_end != std::string::npos) {
                                std::string elif_commands = trim(remaining.substr(
                                    elif_body_start, elif_body_end - elif_body_start));
                                auto cmds = shell_parser->parse_semicolon_commands(elif_commands);
                                for (const auto& c : cmds) {
                                    int rc = execute_simple_or_pipeline(c);

                                    if (rc == 253 || rc == 254 || rc == 255) {
                                        idx = 0;
                                        return rc;
                                    }
                                }
                                idx = 0;
                                return 0;
                            }
                        }
                        pos = elif_then + 6;
                    }
                } else if (else_pos != std::string::npos && next_pos == else_pos) {
                    if (!condition_met && cond_result != 0) {
                        pos = next_pos + 7;
                        size_t fi_end = remaining.find(" fi", pos);
                        if (fi_end != std::string::npos) {
                            std::string else_commands = trim(remaining.substr(pos, fi_end - pos));
                            auto cmds = shell_parser->parse_semicolon_commands(else_commands);
                            for (const auto& c : cmds) {
                                execute_simple_or_pipeline(c);
                            }
                            idx = 0;
                            return 0;
                        }
                    }
                    break;
                } else {
                    if (commands.length() > 0) {
                        auto cmds = shell_parser->parse_semicolon_commands(commands);
                        for (const auto& c : cmds) {
                            execute_simple_or_pipeline(c);
                        }
                    }
                    break;
                }
            }

            idx = 0;
            return 0;
        }
    }

    std::vector<std::pair<std::vector<std::string>, std::vector<std::string>>> elif_branches;
    std::vector<std::string> current_elif_cond;
    std::vector<std::string> current_elif_body;
    bool in_elif = false;
    bool in_elif_body = false;
    bool condition_met = (cond_rc == 0);

    while (k < src_lines.size() && depth > 0) {
        const std::string& cur_raw = src_lines[k];
        std::string cur = trim(strip_inline_comment(cur_raw));

        if (depth == 1 && (cur == "elif" || cur.rfind("elif ", 0) == 0)) {
            if (in_elif_body && !current_elif_cond.empty()) {
                elif_branches.push_back({current_elif_cond, current_elif_body});
            }

            in_elif = true;
            in_elif_body = false;
            in_else = false;
            current_elif_cond.clear();
            current_elif_body.clear();

            std::string elif_cond;
            if (cur.rfind("elif ", 0) == 0) {
                elif_cond = trim(cur.substr(5));
            }

            auto then_pos = elif_cond.find("; then");
            if (then_pos == std::string::npos) {
                then_pos = elif_cond.find(";then");
            }

            if (then_pos != std::string::npos) {
                current_elif_cond.push_back(trim(elif_cond.substr(0, then_pos)));
                in_elif = false;
                in_elif_body = true;
            } else if (!elif_cond.empty()) {
                current_elif_cond.push_back(elif_cond);
            }
            k++;
            continue;
        }

        if (cur == "if" || cur.rfind("if ", 0) == 0) {
            depth++;
        } else if (cur.find("; then") != std::string::npos ||
                   cur.find(";then") != std::string::npos) {
            if (cur.rfind("if ", 0) == 0 || cur == "if") {
                depth++;
            } else if (depth == 1 && in_elif) {
                in_elif = false;
                in_elif_body = true;

                auto then_pos = cur.find("; then");
                if (then_pos == std::string::npos) {
                    then_pos = cur.find(";then");
                }
                if (then_pos != std::string::npos) {
                    std::string cond_part = trim(cur.substr(0, then_pos));
                    if (!cond_part.empty()) {
                        current_elif_cond.push_back(cond_part);
                    }
                }
                k++;
                continue;
            }
        } else if (cur == "fi") {
            depth--;
            if (depth == 0)
                break;
        } else if (depth == 1 && cur == "else") {
            if (in_elif_body && !current_elif_cond.empty()) {
                elif_branches.push_back({current_elif_cond, current_elif_body});
                current_elif_cond.clear();
                current_elif_body.clear();
            }

            in_else = true;
            in_elif = false;
            in_elif_body = false;
            else_lines.clear();
            k++;
            continue;
        } else if (depth == 1 && cur == "then") {
            if (in_elif) {
                in_elif = false;
                in_elif_body = true;
                k++;
                continue;
            }
        }

        if (depth > 0) {
            if (in_elif) {
                current_elif_cond.push_back(cur_raw);
            } else if (in_elif_body) {
                current_elif_body.push_back(cur_raw);
            } else if (!in_else) {
                then_lines.push_back(cur_raw);
            } else {
                else_lines.push_back(cur_raw);
            }
        }
        k++;
    }

    if (in_elif_body && !current_elif_cond.empty()) {
        elif_branches.push_back({current_elif_cond, current_elif_body});
    }

    if (depth != 0) {
        idx = k;
        return 1;
    }

    int body_rc = 0;
    if (cond_rc == 0) {
        body_rc = execute_block(then_lines);
    } else {
        for (const auto& elif_branch : elif_branches) {
            std::string elif_cond_str;
            for (const auto& line : elif_branch.first) {
                if (!elif_cond_str.empty()) {
                    elif_cond_str += " ";
                }
                elif_cond_str += trim(strip_inline_comment(line));
            }

            if (elif_cond_str.empty()) {
                idx = k;
                return 2;
            }

            int elif_rc = evaluate_logical_condition(elif_cond_str);
            if (elif_rc == 0) {
                body_rc = execute_block(elif_branch.second);
                condition_met = true;
                break;
            }
        }

        if (!condition_met && !else_lines.empty()) {
            body_rc = execute_block(else_lines);
        }
    }

    idx = k;
    return body_rc;
}

std::string simplify_parentheses_in_condition(
    const std::string& condition, const std::function<int(const std::string&)>& evaluator) {
    std::string result = condition;

    while (true) {
        size_t start = std::string::npos;
        size_t end = std::string::npos;
        int depth = 0;
        bool in_quotes = false;
        char quote_char = '\0';
        bool escaped = false;

        for (size_t i = 0; i < result.length(); ++i) {
            char c = result[i];

            if (escaped) {
                escaped = false;
                continue;
            }

            if (c == '\\') {
                escaped = true;
                continue;
            }

            if (!in_quotes) {
                if (c == '$' && i + 1 < result.length() && result[i + 1] == '(') {
                    size_t j = i + 1;
                    int nested = 0;
                    bool sub_in_quotes = false;
                    char sub_quote = '\0';
                    bool sub_escaped = false;

                    for (; j < result.length(); ++j) {
                        char sc = result[j];

                        if (sub_escaped) {
                            sub_escaped = false;
                            continue;
                        }

                        if (sc == '\\') {
                            sub_escaped = true;
                            continue;
                        }

                        if (!sub_in_quotes) {
                            if (sc == '"' || sc == '\'' || sc == '`') {
                                sub_in_quotes = true;
                                sub_quote = sc;
                                continue;
                            }
                            if (sc == '(') {
                                nested++;
                                continue;
                            }
                            if (sc == ')') {
                                if (nested == 0) {
                                    i = j;
                                    break;
                                }
                                nested--;
                                continue;
                            }
                        } else {
                            if (sc == sub_quote) {
                                sub_in_quotes = false;
                                sub_quote = '\0';
                            }
                            continue;
                        }
                    }

                    if (j == result.length())
                        break;

                    continue;
                }

                if (c == '"' || c == '\'' || c == '`') {
                    in_quotes = true;
                    quote_char = c;
                    continue;
                }
                if (c == '(') {
                    if (depth == 0) {
                        start = i;
                    }
                    depth++;
                } else if (c == ')') {
                    if (depth > 0) {
                        depth--;
                        if (depth == 0 && start != std::string::npos) {
                            end = i;
                            break;
                        }
                    }
                }
            } else {
                if (c == quote_char) {
                    in_quotes = false;
                    quote_char = '\0';
                }
            }
        }

        if (start == std::string::npos || end == std::string::npos) {
            break;
        }

        std::string inner = result.substr(start + 1, end - start - 1);
        int inner_result = evaluator(inner);
        std::string replacement = (inner_result == 0) ? "true" : "false";

        std::string temp = result.substr(0, start);
        temp.append(replacement);
        temp.append(result.substr(end + 1));
        result = std::move(temp);
    }

    return result;
}

int evaluate_logical_condition(const std::string& condition,
                               const std::function<int(const std::string&)>& executor) {
    std::string cond = trim(condition);
    if (cond.empty())
        return 1;

    std::function<int(const std::string&)> self_eval = [&](const std::string& inner) -> int {
        return evaluate_logical_condition(inner, executor);
    };

    cond = simplify_parentheses_in_condition(cond, self_eval);

    bool has_logical_ops = false;
    bool in_quotes = false;
    char quote_char = '\0';
    bool escaped = false;
    int bracket_depth = 0;
    int paren_depth = 0;

    for (size_t i = 0; i + 1 < cond.length(); ++i) {
        char c = cond[i];

        if (escaped) {
            escaped = false;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            continue;
        }

        if (handle_quote_char(c, in_quotes, quote_char, nullptr)) {
            continue;
        }
        update_group_depths(c, bracket_depth, paren_depth);

        if (!in_quotes && bracket_depth == 0 && paren_depth == 0) {
            if ((cond[i] == '&' && cond[i + 1] == '&') || (cond[i] == '|' && cond[i + 1] == '|')) {
                has_logical_ops = true;
                break;
            }
        }
    }

    if (!has_logical_ops) {
        return executor(cond);
    }

    std::vector<std::pair<std::string, std::string>> parts;
    std::string current_part;
    in_quotes = false;
    quote_char = '\0';
    escaped = false;
    bracket_depth = 0;
    paren_depth = 0;

    for (size_t i = 0; i < cond.length(); ++i) {
        char c = cond[i];

        if (escaped) {
            current_part += c;
            escaped = false;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            current_part += c;
            continue;
        }

        if (handle_quote_char(c, in_quotes, quote_char, &current_part)) {
            continue;
        }
        update_group_depths(c, bracket_depth, paren_depth);

        if (!in_quotes && bracket_depth == 0 && paren_depth == 0 && i + 1 < cond.length()) {
            if (cond[i] == '&' && cond[i + 1] == '&') {
                parts.push_back({trim(current_part), "&&"});
                current_part.clear();
                ++i;
                continue;
            }
            if (cond[i] == '|' && cond[i + 1] == '|') {
                parts.push_back({trim(current_part), "||"});
                current_part.clear();
                ++i;
                continue;
            }
        }

        current_part += c;
    }

    if (!current_part.empty()) {
        parts.push_back({trim(current_part), ""});
    }

    if (parts.empty())
        return 1;

    int result = executor(parts[0].first);

    for (size_t i = 1; i < parts.size(); ++i) {
        const std::string& op = parts[i - 1].second;
        const std::string& cond_part = parts[i].first;

        if (op == "&&") {
            if (result != 0) {
                break;
            }
        } else if (op == "||") {
            if (result == 0) {
                break;
            }
        }

        result = executor(cond_part);
    }

    return result;
}

}  // namespace conditional_evaluator

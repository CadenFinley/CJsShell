/*
  command_analysis.cpp

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

#include "command_analysis.h"

#include <cctype>
#include <filesystem>
#include <system_error>

#include "cjsh_filesystem.h"
#include "quote_state.h"
#include "shell.h"
#include "shell_env.h"
#include "token_classifier.h"

namespace command_analysis {

bool extract_next_token(const std::string& cmd, size_t& cursor, size_t& token_start,
                        size_t& token_end) {
    const size_t len = cmd.length();

    while (cursor < len && (std::isspace(static_cast<unsigned char>(cmd[cursor])) != 0)) {
        ++cursor;
    }

    if (cursor >= len) {
        return false;
    }

    size_t start = cursor;
    utils::QuoteState quote_state;

    while (cursor < len) {
        char ch = cmd[cursor];
        auto action = quote_state.consume_forward(ch);
        if (action == utils::QuoteAdvanceResult::Process && !quote_state.inside_quotes() &&
            (std::isspace(static_cast<unsigned char>(ch)) != 0)) {
            break;
        }
        ++cursor;
    }

    token_start = start;
    token_end = cursor;
    return true;
}

bool token_has_explicit_path_hint(const std::string& token) {
    if (token.empty()) {
        return false;
    }

    if (token[0] == '/') {
        return true;
    }

    return token.rfind("./", 0) == 0 || token.rfind("../", 0) == 0 || token.rfind("~/", 0) == 0 ||
           token.rfind("-/", 0) == 0 || token.find('/') != std::string::npos;
}

std::string resolve_token_path(const std::string& token, const Shell* shell) {
    std::string path_to_check = token;

    if (token.rfind("~/", 0) == 0) {
        path_to_check = cjsh_filesystem::g_user_home_path().string() + token.substr(1);
    } else if (token.rfind("-/", 0) == 0) {
        if (shell != nullptr) {
            std::string prev_dir = shell->get_previous_directory();
            if (!prev_dir.empty()) {
                path_to_check = prev_dir + token.substr(1);
            }
        }
    } else if (token[0] != '/' && token.rfind("./", 0) != 0 && token.rfind("../", 0) != 0 &&
               token.rfind("~/", 0) != 0 && token.rfind("-/", 0) != 0) {
        path_to_check = cjsh_filesystem::safe_current_directory() + "/" + token;
    }

    return path_to_check;
}

bool token_is_history_expansion(const std::string& token, size_t absolute_cmd_start) {
    if (!config::history_expansion_enabled || token.empty()) {
        return false;
    }

    if (token[0] == '!') {
        return true;
    }

    if (token[0] == '^' && absolute_cmd_start == 0) {
        return true;
    }

    return false;
}

bool is_known_command_token(const std::string& token, size_t absolute_cmd_start, Shell* shell,
                            const std::unordered_set<std::string>& available_commands) {
    using namespace token_classifier;

    if (token.empty()) {
        return true;
    }

    if (is_variable_reference(token)) {
        return true;
    }

    if (token_is_history_expansion(token, absolute_cmd_start)) {
        return true;
    }

    if (token_has_explicit_path_hint(token)) {
        std::string path_to_check = resolve_token_path(token, shell);
        std::error_code ec;
        return std::filesystem::exists(path_to_check, ec);
    }

    if (shell != nullptr && shell->get_interactive_mode()) {
        const auto& abbreviations = shell->get_abbreviations();
        if (abbreviations.find(token) != abbreviations.end()) {
            return true;
        }
    }

    if (is_shell_keyword(token) || is_shell_builtin(token)) {
        return true;
    }

    if (available_commands.find(token) != available_commands.end()) {
        return true;
    }

    if (is_external_command(token)) {
        return true;
    }

    return false;
}

std::string sanitize_input_for_analysis(const std::string& input,
                                        std::vector<CommentRange>* comment_ranges) {
    std::string sanitized = input;
    size_t len = input.size();
    bool in_quotes = false;
    char quote_char = '\0';
    bool escaped = false;

    size_t i = 0;
    while (i < len) {
        char c = input[i];

        if (escaped) {
            escaped = false;
            ++i;
            continue;
        }

        if (c == '\\' && (!in_quotes || quote_char != '\'')) {
            escaped = true;
            ++i;
            continue;
        }

        if ((c == '"' || c == '\'') && !in_quotes) {
            in_quotes = true;
            quote_char = c;
            ++i;
            continue;
        }

        if (c == quote_char && in_quotes) {
            in_quotes = false;
            quote_char = '\0';
            ++i;
            continue;
        }

        if (!in_quotes && c == '#') {
            size_t comment_end = i;
            while (comment_end < len && input[comment_end] != '\n' && input[comment_end] != '\r') {
                sanitized[comment_end] = ' ';
                comment_end++;
            }
            if (comment_ranges != nullptr && comment_end > i) {
                comment_ranges->push_back({i, comment_end});
            }
            i = comment_end;
            continue;
        }

        ++i;
    }

    return sanitized;
}

CommandSeparator scan_command_separator(const std::string& analysis, size_t index) {
    CommandSeparator match;
    const size_t len = analysis.size();
    if (index >= len) {
        return match;
    }

    char current = analysis[index];
    if (index + 1 < len) {
        char next = analysis[index + 1];
        if ((current == '&' && next == '&') || (current == '&' && next == '^') ||
            (current == '|' && next == '|') || (current == '>' && next == '>') ||
            (current == '<' && next == '<') || (current == '&' && next == '>')) {
            match.length = 2;
            match.is_operator = true;
            return match;
        }
        if (current == '\r' && next == '\n') {
            match.length = 2;
            match.is_operator = false;
            return match;
        }
    }

    if (current == '|' || current == ';' || current == '>' || current == '<') {
        match.length = 1;
        match.is_operator = true;
        return match;
    }

    if (current == '&' && (index == len - 1 || analysis[index + 1] != '&')) {
        match.length = 1;
        match.is_operator = true;
        return match;
    }

    if (current == '\n' || current == '\r') {
        match.length = 1;
        match.is_operator = false;
        return match;
    }

    return match;
}

size_t find_command_end(const std::string& analysis, size_t start) {
    const size_t len = analysis.size();
    size_t cmd_end = start;
    utils::QuoteState cmd_quote_state;
    while (cmd_end < len) {
        char current = analysis[cmd_end];
        auto action = cmd_quote_state.consume_forward(current);
        if (action == utils::QuoteAdvanceResult::Process && !cmd_quote_state.inside_quotes()) {
            auto separator = scan_command_separator(analysis, cmd_end);
            if (separator.length > 0) {
                break;
            }
        }
        cmd_end++;
    }

    return cmd_end;
}

}  // namespace command_analysis

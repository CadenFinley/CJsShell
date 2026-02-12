/*
  validation.cpp

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
#include "shell_env.h"

#include "validation_common.h"

#include <cctype>
#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "error_out.h"

using shell_script_interpreter::detail::process_line_for_validation;
using shell_script_interpreter::detail::should_skip_line;
using shell_script_interpreter::detail::strip_inline_comment;
using shell_script_interpreter::detail::trim;
namespace validation_internal = shell_validation::internal;

using validation_internal::analyze_case_syntax;
using validation_internal::analyze_for_loop_syntax;
using validation_internal::analyze_if_syntax;
using validation_internal::analyze_while_until_syntax;
using validation_internal::extract_trimmed_line;
using validation_internal::for_each_effective_char;
using validation_internal::IterationAction;
using validation_internal::next_effective_line_starts_with_keyword;
using validation_internal::QuoteState;
using validation_internal::sanitize_command_substitutions_for_validation;
using validation_internal::sanitize_lines_for_validation;
using validation_internal::should_process_char;
using validation_internal::starts_with_keyword_token;
using validation_internal::tokenize_and_get_first;
using validation_internal::tokenize_whitespace;

namespace {

enum class ControlToken : std::uint8_t {
    If,
    Then,
    Elif,
    Else,
    Fi,
    While,
    Until,
    For,
    Do,
    Done,
    Case,
    CaseHeader,
    In,
    Esac,
    Function,
    BraceOpen,
    BraceClose
};

const char* control_token_name(ControlToken token) {
    switch (token) {
        case ControlToken::If:
            return "if";
        case ControlToken::Then:
            return "then";
        case ControlToken::Elif:
            return "elif";
        case ControlToken::Else:
            return "else";
        case ControlToken::Fi:
            return "fi";
        case ControlToken::While:
            return "while";
        case ControlToken::Until:
            return "until";
        case ControlToken::For:
            return "for";
        case ControlToken::Do:
            return "do";
        case ControlToken::Done:
            return "done";
        case ControlToken::Case:
            return "case";
        case ControlToken::CaseHeader:
            return "case-header";
        case ControlToken::In:
            return "in";
        case ControlToken::Esac:
            return "esac";
        case ControlToken::Function:
            return "function";
        case ControlToken::BraceOpen:
            return "{";
        case ControlToken::BraceClose:
            return "}";
    }
    return "";
}

std::optional<ControlToken> parse_control_token(std::string_view token) {
    if (token == "if") {
        return ControlToken::If;
    }
    if (token == "then") {
        return ControlToken::Then;
    }
    if (token == "elif") {
        return ControlToken::Elif;
    }
    if (token == "else") {
        return ControlToken::Else;
    }
    if (token == "fi") {
        return ControlToken::Fi;
    }
    if (token == "while") {
        return ControlToken::While;
    }
    if (token == "until") {
        return ControlToken::Until;
    }
    if (token == "for") {
        return ControlToken::For;
    }
    if (token == "do") {
        return ControlToken::Do;
    }
    if (token == "done") {
        return ControlToken::Done;
    }
    if (token == "case") {
        return ControlToken::Case;
    }
    if (token == "in") {
        return ControlToken::In;
    }
    if (token == "esac") {
        return ControlToken::Esac;
    }
    if (token == "function") {
        return ControlToken::Function;
    }
    if (token == "{") {
        return ControlToken::BraceOpen;
    }
    if (token == "}") {
        return ControlToken::BraceClose;
    }
    return std::nullopt;
}

bool starts_with_token_keyword(const std::string& text, ControlToken token) {
    const char* keyword = control_token_name(token);
    if (keyword == nullptr || *keyword == '\0') {
        return false;
    }
    const size_t len = std::strlen(keyword);
    if (text == keyword) {
        return true;
    }
    if (text.size() <= len) {
        return false;
    }
    if (text.compare(0, len, keyword) != 0) {
        return false;
    }
    return std::isspace(static_cast<unsigned char>(text[len])) != 0;
}

using SyntaxError = ShellScriptInterpreter::SyntaxError;
using ErrorCategory = ShellScriptInterpreter::ErrorCategory;

ErrorType map_category_to_error_type(ErrorCategory category) {
    switch (category) {
        case ErrorCategory::SYNTAX:
            return ErrorType::SYNTAX_ERROR;
        case ErrorCategory::CONTROL_FLOW:
        case ErrorCategory::COMMANDS:
        case ErrorCategory::SEMANTICS:
            return ErrorType::RUNTIME_ERROR;
        case ErrorCategory::REDIRECTION:
            return ErrorType::FILE_NOT_FOUND;
        case ErrorCategory::VARIABLES:
            return ErrorType::INVALID_ARGUMENT;
        case ErrorCategory::STYLE:
            return ErrorType::INVALID_ARGUMENT;
        case ErrorCategory::PERFORMANCE:
            return ErrorType::INVALID_ARGUMENT;
        default:
            return ErrorType::UNKNOWN_ERROR;
    }
}

std::string build_error_message(const SyntaxError& error) {
    std::ostringstream builder;
    if (!error.error_code.empty()) {
        builder << "[" << error.error_code << "] ";
    }
    builder << error.message;
    if (error.position.line_number > 0) {
        builder << " (line " << error.position.line_number;
        if (error.position.column_start > 0) {
            builder << ", column " << error.position.column_start;
        }
        builder << ")";
    }
    return builder.str();
}

void emit_validation_errors(const std::vector<SyntaxError>& errors) {
    for (const auto& error : errors) {
        std::vector<std::string> suggestions;
        if (config::error_suggestions_enabled) {
            if (!error.suggestion.empty()) {
                suggestions.push_back(error.suggestion);
            }
            for (const auto& info : error.related_info) {
                if (!info.empty()) {
                    suggestions.push_back(info);
                }
            }
            if (!error.documentation_url.empty()) {
                suggestions.push_back("More info: " + error.documentation_url);
            }
        }

        print_error({map_category_to_error_type(error.category), error.severity, "",
                     build_error_message(error), suggestions});
    }
}

bool message_contains_any(const std::string& haystack, std::initializer_list<const char*> needles) {
    for (const char* needle : needles) {
        if (needle != nullptr && *needle != '\0' && haystack.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool syntax_error_indicates_incomplete(const SyntaxError& error) {
    const std::string& code = error.error_code;
    const std::string& message = error.message;

    if (code == "SYN001" || code == "SYN007") {
        return message_contains_any(message, {"Unclosed", "Unmatched opening", "missing '"});
    }

    if (code == "SYN002") {
        return message_contains_any(message, {"incomplete", "missing", "without done"});
    }

    if (code == "SYN003" || code == "SYN004") {
        return message_contains_any(message, {"missing", "Unclosed"});
    }

    if (code == "SYN008") {
        return message_contains_any(message, {"missing", "incomplete"});
    }

    if (code == "SYN012") {
        return message_contains_any(message, {"without condition"});
    }

    return false;
}

bool has_incomplete_construct_errors(const std::vector<SyntaxError>& errors) {
    for (const auto& error : errors) {
        if (syntax_error_indicates_incomplete(error)) {
            return true;
        }
    }
    return false;
}

bool has_inline_terminator(const std::string& text, const std::string& terminator) {
    auto is_boundary = [](char ch) {
        if (ch == '\0' || ch == ';' || ch == '(' || ch == ')' || ch == '{' || ch == '}' ||
            ch == '&' || ch == '|') {
            return true;
        }
        return std::isspace(static_cast<unsigned char>(ch)) != 0;
    };

    size_t pos = 0;
    while ((pos = text.find(terminator, pos)) != std::string::npos) {
        bool valid_start = (pos == 0) || is_boundary(text[pos - 1]);
        size_t end_pos = pos + terminator.length();
        bool valid_end = (end_pos >= text.length()) || is_boundary(text[end_pos]);

        if (valid_start && valid_end) {
            return true;
        }
        pos++;
    }
    return false;
}

bool handle_inline_loop_header(
    const std::string& line, ControlToken keyword, size_t display_line,
    std::vector<std::tuple<ControlToken, ControlToken, size_t>>& control_stack) {
    if (!starts_with_keyword_token(line, control_token_name(keyword)))
        return false;

    size_t search_pos = 0;
    while ((search_pos = line.find(';', search_pos)) != std::string::npos) {
        size_t do_pos = search_pos + 1;
        while (do_pos < line.size() &&
               std::isspace(static_cast<unsigned char>(line[do_pos])) != 0) {
            ++do_pos;
        }

        if (do_pos < line.size() && line.compare(do_pos, 2, "do") == 0) {
            size_t after_do = do_pos + 2;
            if (after_do == line.size() || line[after_do] == ';' || line[after_do] == '&' ||
                line[after_do] == '|' || line[after_do] == '{' || line[after_do] == '(' ||
                line[after_do] == '#' ||
                std::isspace(static_cast<unsigned char>(line[after_do])) != 0) {
                if (!has_inline_terminator(line, "done")) {
                    control_stack.push_back({ControlToken::Do, keyword, display_line});
                }
                return true;
            }
        }

        ++search_pos;
    }

    return false;
}

void push_function_context(
    const std::string& trimmed_line, size_t display_line,
    std::vector<std::tuple<ControlToken, ControlToken, size_t>>& control_stack) {
    if (!trimmed_line.empty() && trimmed_line.back() == '{') {
        size_t open_brace = trimmed_line.find('{');
        if (open_brace != std::string::npos) {
            std::string after_brace = trimmed_line.substr(open_brace + 1);

            int brace_count = 1;
            for (char c : after_brace) {
                if (c == '{') {
                    brace_count++;
                } else if (c == '}') {
                    brace_count--;
                }
            }

            if (brace_count > 0) {
                control_stack.push_back(
                    {ControlToken::BraceOpen, ControlToken::BraceOpen, display_line});
            }
        } else {
            control_stack.push_back(
                {ControlToken::BraceOpen, ControlToken::BraceOpen, display_line});
        }
    } else {
        control_stack.push_back({ControlToken::Function, ControlToken::Function, display_line});
    }
}

bool find_embedded_loop_keyword(const std::string& line, const std::string& keyword,
                                size_t& position_out) {
    bool found = false;
    for_each_effective_char(
        line, false, false,
        [&](size_t index, char c, QuoteState&, size_t& next_index) -> IterationAction {
            if (index == 0 || c != keyword[0]) {
                return IterationAction::Continue;
            }
            if (index + keyword.size() > line.size()) {
                return IterationAction::Continue;
            }
            if (line.compare(index, keyword.size(), keyword) != 0) {
                return IterationAction::Continue;
            }

            char previous = line[index - 1];
            bool prefix_ok = (std::isspace(static_cast<unsigned char>(previous)) != 0) ||
                             previous == '|' || previous == ';' || previous == '&' ||
                             previous == '(' || previous == '{';
            if (!prefix_ok) {
                return IterationAction::Continue;
            }

            size_t after = index + keyword.size();
            if (after < line.size()) {
                char next_char = line[after];
                if (std::isspace(static_cast<unsigned char>(next_char)) == 0 && next_char != '(') {
                    return IterationAction::Continue;
                }
            }

            position_out = index;
            next_index = index + keyword.size() - 1;
            found = true;
            return IterationAction::Break;
        },
        true);

    return found;
}

bool handle_embedded_loop_header(
    const std::string& trimmed_line, size_t display_line,
    std::vector<std::tuple<ControlToken, ControlToken, size_t>>& control_stack) {
    auto try_keyword = [&](ControlToken keyword) -> bool {
        size_t position = 0;
        const std::string keyword_text = control_token_name(keyword);
        if (!find_embedded_loop_keyword(trimmed_line, keyword_text, position)) {
            return false;
        }

        std::string remainder = trim(trimmed_line.substr(position));
        auto [tokens, first_token] = tokenize_and_get_first(remainder);
        if (first_token != keyword_text) {
            return false;
        }

        if (has_inline_terminator(remainder, "done")) {
            return false;
        }

        if (keyword == ControlToken::For) {
            auto for_check = analyze_for_loop_syntax(tokens, remainder);
            control_stack.push_back({ControlToken::For, ControlToken::For, display_line});
            if (for_check.has_inline_do) {
                std::get<0>(control_stack.back()) = ControlToken::Do;
            }
        } else {
            auto loop_check = analyze_while_until_syntax(keyword_text, remainder, tokens);
            control_stack.push_back({keyword, keyword, display_line});
            if (loop_check.has_inline_do) {
                std::get<0>(control_stack.back()) = ControlToken::Do;
            }
        }
        return true;
    };

    if (try_keyword(ControlToken::While)) {
        return true;
    }
    if (try_keyword(ControlToken::Until)) {
        return true;
    }
    if (try_keyword(ControlToken::For)) {
        return true;
    }
    return false;
}

}  // namespace

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::validate_script_syntax(
    const std::vector<std::string>& lines) {
    std::vector<SyntaxError> errors;
    std::vector<std::string> sanitized_lines = sanitize_lines_for_validation(lines);

    std::vector<std::tuple<ControlToken, ControlToken, size_t>> control_stack;
    bool encountered_unclosed_quote = false;

    auto expected_close_for_entry =
        [](const std::tuple<ControlToken, ControlToken, size_t>& entry) -> ControlToken {
        const ControlToken current_state = std::get<0>(entry);
        const ControlToken opening_statement = std::get<1>(entry);

        if (opening_statement == ControlToken::If || current_state == ControlToken::Then ||
            current_state == ControlToken::Elif || current_state == ControlToken::Else) {
            return ControlToken::Fi;
        }
        if (opening_statement == ControlToken::While || opening_statement == ControlToken::Until ||
            opening_statement == ControlToken::For || current_state == ControlToken::Do) {
            return ControlToken::Done;
        }
        if (opening_statement == ControlToken::Case) {
            return ControlToken::Esac;
        }
        if (opening_statement == ControlToken::BraceOpen ||
            opening_statement == ControlToken::Function) {
            return ControlToken::BraceClose;
        }
        return ControlToken::BraceClose;
    };

    auto report_unclosed_entry = [&](const std::tuple<ControlToken, ControlToken, size_t>& entry) {
        const ControlToken current_state = std::get<0>(entry);
        const ControlToken opening_statement = std::get<1>(entry);
        size_t opening_line = std::get<2>(entry);
        ControlToken expected_close = expected_close_for_entry(entry);

        if (current_state == ControlToken::CaseHeader) {
            return;
        }

        if (opening_statement == current_state &&
            (opening_statement == ControlToken::For || opening_statement == ControlToken::While ||
             opening_statement == ControlToken::Until || opening_statement == ControlToken::If)) {
            return;
        }

        std::string msg = "Unclosed '";
        msg += control_token_name(opening_statement);
        msg += "' from line ";
        msg += std::to_string(opening_line);
        msg += " - missing ";
        if (expected_close == ControlToken::Done) {
            msg += "closing '";
            msg += control_token_name(expected_close);
            msg += "'";
        } else {
            msg += "'";
            msg += control_token_name(expected_close);
            msg += "'";
        }
        SyntaxError syn_err(opening_line, msg, "");

        if (opening_statement == ControlToken::BraceOpen ||
            opening_statement == ControlToken::Function) {
            syn_err.error_code = "SYN007";
            syn_err.suggestion =
                "Add closing '}' to match the opening on line " + std::to_string(opening_line);
        } else {
            syn_err.error_code = "SYN001";
            std::string close_name = control_token_name(expected_close);
            syn_err.suggestion = "Add '" + close_name + "' to close the '" +
                                 std::string(control_token_name(opening_statement)) +
                                 "' that started on line " + std::to_string(opening_line);
            // include plain "missing 'done'" phrasing to satisfy tests
            if (close_name == "done") {
                syn_err.message =
                    "Unclosed '" + std::string(control_token_name(opening_statement)) +
                    "' from line " + std::to_string(opening_line) + " - missing 'done'";
            }
            if (encountered_unclosed_quote && syn_err.error_code != "SYN007") {
                syn_err.related_info.push_back(
                    "An earlier unclosed quote may prevent detecting the matching closure "
                    "correctly.");
            }
        }

        syn_err.category = ErrorCategory::CONTROL_FLOW;
        syn_err.severity = ErrorSeverity::CRITICAL;
        errors.push_back(std::move(syn_err));
    };

    auto unwind_until_allowed = [&](std::initializer_list<ControlToken> allowed_states,
                                    ControlToken closing_keyword) {
        while (!control_stack.empty()) {
            const auto& top = control_stack.back();
            ControlToken current_state = std::get<0>(top);

            for (const auto allowed : allowed_states) {
                if (current_state == allowed) {
                    return true;
                }
            }

            ControlToken expected_close = expected_close_for_entry(top);
            if (expected_close == closing_keyword) {
                break;
            }

            report_unclosed_entry(top);
            control_stack.pop_back();
        }
        return false;
    };

    for (size_t line_num = 0; line_num < lines.size(); ++line_num) {
        const std::string& line = sanitized_lines[line_num];
        size_t display_line = line_num + 1;

        std::string trimmed;
        size_t first_non_space = 0;
        if (!extract_trimmed_line(line, trimmed, first_non_space)) {
            continue;
        }

        std::string line_without_comments = strip_inline_comment(line);
        std::string sanitized_line_without_comments =
            sanitize_command_substitutions_for_validation(line_without_comments);
        QuoteState quote_state;

        for (char c : sanitized_line_without_comments) {
            should_process_char(quote_state, c, false, false);
        }

        if (quote_state.in_quotes) {
            char missing = quote_state.quote_char == '\0' ? '"' : quote_state.quote_char;
            std::string message = "Unclosed quote: missing closing ";
            message.push_back(missing);
            std::string suggestion = "Close the opening ";
            suggestion.push_back(missing);
            suggestion += " or remove the stray quote";

            SyntaxError quote_error({display_line, 0, 0, 0}, ErrorSeverity::CRITICAL,
                                    ErrorCategory::SYNTAX, "SYN001", message, line, suggestion);
            errors.push_back(std::move(quote_error));
            encountered_unclosed_quote = true;
            control_stack.clear();
            break;
        }

        int paren_balance = 0;
        quote_state = QuoteState{};
        bool in_case_block = false;

        for (const auto& stack_item : control_stack) {
            if (std::get<0>(stack_item) == ControlToken::Case) {
                in_case_block = true;
                break;
            }
        }

        bool line_has_case =
            trimmed.find("case ") != std::string::npos && trimmed.find(" in ") != std::string::npos;

        bool looks_like_case_pattern =
            (in_case_block || line_has_case) && (trimmed.find(')') != std::string::npos);

        if (in_case_block && trimmed.find(')') != std::string::npos) {
            size_t paren_pos = trimmed.find(')');
            if (paren_pos != std::string::npos) {
                std::string before_paren = trimmed.substr(0, paren_pos);

                before_paren = trim(before_paren);
                if (!before_paren.empty() &&
                    (before_paren.front() == '"' || before_paren.front() == '\'' ||
                     before_paren == "*" || (isalnum(before_paren.front()) != 0))) {
                    looks_like_case_pattern = true;
                }
            }
        }

        if (!looks_like_case_pattern) {
            for (size_t i = 0; i < sanitized_line_without_comments.length(); ++i) {
                char c = sanitized_line_without_comments[i];

                if (!should_process_char(quote_state, c, false, false)) {
                    continue;
                }

                if (!quote_state.in_quotes) {
                    if (c == '(') {
                        paren_balance++;
                    } else if (c == ')') {
                        paren_balance--;
                    }
                }
            }

            if (paren_balance != 0) {
                if (paren_balance > 0) {
                    errors.push_back({display_line, "Unmatched opening parenthesis", line});
                } else {
                    errors.push_back({display_line, "Unmatched closing parenthesis", line});
                }
            }
        }

        std::string trimmed_for_parsing = process_line_for_validation(trimmed);

        if (!trimmed_for_parsing.empty() && trimmed_for_parsing.back() == ';') {
            trimmed_for_parsing.pop_back();
            trimmed_for_parsing = trim(trimmed_for_parsing);
        }

        if (!trimmed_for_parsing.empty() && trimmed_for_parsing.front() == ';') {
            std::string after_semicolon = trim(trimmed_for_parsing.substr(1));
            if (!after_semicolon.empty() &&
                (starts_with_token_keyword(after_semicolon, ControlToken::Then) ||
                 starts_with_token_keyword(after_semicolon, ControlToken::Elif) ||
                 starts_with_token_keyword(after_semicolon, ControlToken::Else) ||
                 starts_with_token_keyword(after_semicolon, ControlToken::Fi))) {
                trimmed_for_parsing = std::move(after_semicolon);
            }
        }

        if (config::posix_mode) {
            auto add_posix_error = [&](const std::string& code, size_t start, size_t end,
                                       const std::string& message, const std::string& suggestion) {
                errors.push_back(SyntaxError({display_line, start, end, 0}, ErrorSeverity::ERROR,
                                             ErrorCategory::SYNTAX, code, message, line,
                                             suggestion));
            };

            QuoteState posix_state;
            bool reported_double_bracket = false;
            bool reported_plus_equal = false;
            bool reported_pipe_amp = false;
            bool reported_amp_gt = false;

            for (size_t i = 0; i + 1 < sanitized_line_without_comments.size(); ++i) {
                char c = sanitized_line_without_comments[i];

                if (!should_process_char(posix_state, c, false, false)) {
                    continue;
                }

                if (posix_state.in_quotes) {
                    continue;
                }

                if (!reported_double_bracket &&
                    sanitized_line_without_comments.compare(i, 2, "[[") == 0) {
                    add_posix_error("POSIX001", i, i + 2,
                                    "'[[' conditionals are disabled in POSIX mode",
                                    "Use '[' or 'test' instead");
                    reported_double_bracket = true;
                }

                if (!reported_plus_equal &&
                    sanitized_line_without_comments.compare(i, 2, "+=") == 0) {
                    add_posix_error("POSIX006", i, i + 2,
                                    "+= assignments are disabled in POSIX mode",
                                    "Use explicit value with '=' instead");
                    reported_plus_equal = true;
                }

                if (!reported_pipe_amp &&
                    sanitized_line_without_comments.compare(i, 2, "|&") == 0) {
                    add_posix_error("POSIX007", i, i + 2,
                                    "'|&' pipelines are disabled in POSIX mode",
                                    "Redirect stderr explicitly then pipe: 2>&1 | cmd");
                    reported_pipe_amp = true;
                }

                if (!reported_amp_gt && sanitized_line_without_comments[i] == '&' &&
                    sanitized_line_without_comments[i + 1] == '>') {
                    size_t end_pos = i + 2;
                    if (end_pos < sanitized_line_without_comments.size() &&
                        sanitized_line_without_comments[end_pos] == '>') {
                        ++end_pos;
                    }
                    add_posix_error("POSIX008", i, end_pos,
                                    "'&>' redirections are disabled in POSIX mode",
                                    "Redirect stdout and stderr separately (e.g., '>file 2>&1')");
                    reported_amp_gt = true;
                }
            }

            if (starts_with_token_keyword(trimmed_for_parsing, ControlToken::Function)) {
                add_posix_error("POSIX002", first_non_space,
                                first_non_space + std::strlen("function"),
                                "The 'function' keyword is disabled in POSIX mode",
                                "Define functions as 'name() { ... }'");
            }

            auto posix_tokens = tokenize_whitespace(trimmed_for_parsing);
            if (!posix_tokens.empty()) {
                const std::string& first_tok = posix_tokens.front();
                if (first_tok == "source") {
                    add_posix_error("POSIX009", first_non_space, first_non_space + first_tok.size(),
                                    "'source' is disabled in POSIX mode",
                                    "Use '.' to read a file in the current shell");
                } else if (first_tok == "local") {
                    add_posix_error("POSIX010", first_non_space, first_non_space + first_tok.size(),
                                    "'local' is disabled in POSIX mode",
                                    "Use assignment without 'local' or redesign scope");
                }
            }
        }

        if (starts_with_token_keyword(trimmed_for_parsing, ControlToken::If) &&
            (trimmed_for_parsing.find("; then") != std::string::npos ||
             trimmed_for_parsing.find(";then") != std::string::npos)) {
            if (!has_inline_terminator(trimmed_for_parsing, "fi")) {
                control_stack.push_back({ControlToken::If, ControlToken::If, display_line});

                std::get<0>(control_stack.back()) = ControlToken::Then;
            }

            size_t elif_pos = 0;
            while ((elif_pos = trimmed_for_parsing.find("; elif", elif_pos)) != std::string::npos) {
                size_t after_elif = elif_pos + 6;

                while (after_elif < trimmed_for_parsing.length() &&
                       std::isspace(static_cast<unsigned char>(trimmed_for_parsing[after_elif]))) {
                    after_elif++;
                }

                if (after_elif >= trimmed_for_parsing.length() ||
                    trimmed_for_parsing[after_elif] == ';' ||
                    (after_elif + 4 <= trimmed_for_parsing.length() &&
                     trimmed_for_parsing.substr(after_elif, 4) == "then")) {
                    errors.push_back({{display_line, 0, 0, 0},
                                      ErrorSeverity::CRITICAL,
                                      ErrorCategory::SYNTAX,
                                      "SYN012",
                                      "'elif' without condition",
                                      line,
                                      "Add a condition after 'elif'"});
                }

                elif_pos = after_elif;
            }
        } else if (handle_inline_loop_header(trimmed_for_parsing, ControlToken::While, display_line,
                                             control_stack) ||
                   handle_inline_loop_header(trimmed_for_parsing, ControlToken::Until, display_line,
                                             control_stack) ||
                   handle_inline_loop_header(trimmed_for_parsing, ControlToken::For, display_line,
                                             control_stack)) {
        } else {
            handle_embedded_loop_header(trimmed_for_parsing, display_line, control_stack);

            auto tokens = tokenize_whitespace(trimmed_for_parsing);

            if (!tokens.empty()) {
                const std::string& first_token = tokens[0];
                auto first_control = parse_control_token(first_token);

                auto require_top = [&](std::initializer_list<ControlToken> allowed,
                                       const std::string& message) {
                    if (control_stack.empty()) {
                        errors.push_back({display_line, message, line});
                        return false;
                    }
                    const ControlToken top = std::get<0>(control_stack.back());
                    for (const auto value : allowed) {
                        if (top == value) {
                            return true;
                        }
                    }
                    errors.push_back({display_line, message, line});
                    return false;
                };

                if (first_control == ControlToken::If) {
                    control_stack.push_back({ControlToken::If, ControlToken::If, display_line});
                } else if (first_control == ControlToken::Then) {
                    if (require_top({ControlToken::If}, "'then' without matching 'if'")) {
                        std::get<0>(control_stack.back()) = ControlToken::Then;
                    }
                } else if (first_control == ControlToken::Elif) {
                    if (require_top({ControlToken::Then, ControlToken::Elif},
                                    "'elif' without matching 'if...then'")) {
                        std::get<0>(control_stack.back()) = ControlToken::Elif;

                        if (tokens.size() == 1) {
                            errors.push_back({{display_line, 0, 0, 0},
                                              ErrorSeverity::CRITICAL,
                                              ErrorCategory::SYNTAX,
                                              "SYN012",
                                              "'elif' without condition",
                                              line,
                                              "Add a condition after 'elif'"});
                        }
                    }
                } else if (first_control == ControlToken::Else) {
                    if (require_top({ControlToken::Then, ControlToken::Elif},
                                    "'else' without matching 'if...then'")) {
                        std::get<0>(control_stack.back()) = ControlToken::Else;
                    }
                } else if (first_control == ControlToken::Fi) {
                    if (require_top({ControlToken::Then, ControlToken::Elif, ControlToken::Else},
                                    "'fi' without matching 'if'")) {
                        control_stack.pop_back();
                    }
                }

                else if (first_control == ControlToken::While ||
                         first_control == ControlToken::Until) {
                    auto loop_check =
                        analyze_while_until_syntax(first_token, trimmed_for_parsing, tokens);
                    control_stack.push_back({*first_control, *first_control, display_line});
                    if (loop_check.has_inline_do) {
                        std::get<0>(control_stack.back()) = ControlToken::Do;
                    }
                } else if (first_control == ControlToken::Do) {
                    if (require_top({ControlToken::While, ControlToken::Until, ControlToken::For},
                                    "'do' without matching 'while', 'until', or 'for'")) {
                        std::get<0>(control_stack.back()) = ControlToken::Do;
                    }
                } else if (first_control == ControlToken::Done) {
                    if (unwind_until_allowed({ControlToken::Do}, ControlToken::Done)) {
                        control_stack.pop_back();
                    } else if (require_top({ControlToken::Do}, "'done' without matching 'do'")) {
                        control_stack.pop_back();
                    }
                }

                else if (first_control == ControlToken::For) {
                    auto for_check = analyze_for_loop_syntax(tokens, trimmed_for_parsing);
                    if (for_check.missing_in_keyword) {
                        errors.push_back(
                            {display_line, "'for' statement missing 'in' clause", line});
                    }
                    control_stack.push_back({ControlToken::For, ControlToken::For, display_line});
                    if (for_check.has_inline_do) {
                        std::get<0>(control_stack.back()) = ControlToken::Do;
                    }
                }

                else if (first_control == ControlToken::Case) {
                    auto case_check = analyze_case_syntax(tokens);
                    if (case_check.missing_in_keyword) {
                        errors.push_back(
                            {display_line, "'case' statement missing 'in' clause", line});
                    }

                    bool header_complete = !case_check.incomplete && !case_check.missing_in_keyword;
                    if (!has_inline_terminator(trimmed_for_parsing, "esac")) {
                        control_stack.push_back(
                            {ControlToken::CaseHeader, ControlToken::Case, display_line});
                        if (header_complete) {
                            std::get<0>(control_stack.back()) = ControlToken::Case;
                        }
                    }
                }

                else if (first_control == ControlToken::In) {
                    if (!control_stack.empty()) {
                        auto& top = control_stack.back();
                        if (std::get<1>(top) == ControlToken::Case &&
                            std::get<0>(top) == ControlToken::CaseHeader) {
                            std::get<0>(top) = ControlToken::Case;
                        }
                    }
                } else if (first_control == ControlToken::Esac) {
                    if (require_top({ControlToken::Case}, "'esac' without matching 'case'")) {
                        control_stack.pop_back();
                    }
                }

                else if (first_control == ControlToken::Function) {
                    if (tokens.size() < 2) {
                        errors.push_back({display_line, "'function' missing function name", line});
                    }
                    push_function_context(trimmed, display_line, control_stack);
                } else if (tokens.size() >= 2 && tokens[1] == "()") {
                    push_function_context(trimmed, display_line, control_stack);
                }

                else if (!trimmed.empty() && trimmed.back() == '{') {
                    if (trimmed == "{" && !control_stack.empty() &&
                        std::get<0>(control_stack.back()) == ControlToken::Function) {
                        continue;
                    }
                    control_stack.push_back(
                        {ControlToken::BraceOpen, ControlToken::BraceOpen, display_line});
                } else if (first_control == ControlToken::BraceClose) {
                    if (require_top({ControlToken::BraceOpen, ControlToken::Function},
                                    "Unmatched closing brace '}'")) {
                        control_stack.pop_back();
                    }
                }
            }
        }
    }

    if (encountered_unclosed_quote) {
        return errors;
    }

    while (!control_stack.empty()) {
        report_unclosed_entry(control_stack.back());
        control_stack.pop_back();
    }

    return errors;
}

bool ShellScriptInterpreter::has_syntax_errors(const std::vector<std::string>& lines,
                                               bool print_errors) {
    std::vector<SyntaxError> errors;

    if (config::posix_mode) {
        errors = validate_comprehensive_syntax(lines, false, false);
    } else {
        errors = validate_script_syntax(lines);

        auto append_errors = [&errors](const std::vector<SyntaxError>& source) {
            errors.insert(errors.end(), source.begin(), source.end());
        };

        append_errors(validate_variable_usage(lines));

        const bool enforce_inline_completion = [&]() {
            size_t non_empty = 0;
            for (const auto& line : lines) {
                if (!trim(line).empty()) {
                    ++non_empty;
                    if (non_empty > 1) {
                        return false;
                    }
                }
            }
            return non_empty == 1;
        }();

        auto append_control_flow_errors = [&errors](const std::vector<SyntaxError>& source) {
            for (const auto& err : source) {
                if (err.error_code == "SYN002" || err.error_code == "SYN003" ||
                    err.error_code == "SYN004" || err.error_code == "SYN008") {
                    errors.push_back(err);
                }
            }
        };

        const std::vector<SyntaxError> loop_errors = validate_loop_syntax(lines);
        const std::vector<SyntaxError> conditional_errors = validate_conditional_syntax(lines);

        auto append_filtered_errors = [&errors](
                                          const std::vector<SyntaxError>& source,
                                          const std::function<bool(const SyntaxError&)>& filter) {
            for (const auto& err : source) {
                if (filter(err)) {
                    errors.push_back(err);
                }
            }
        };

        if (enforce_inline_completion) {
            append_control_flow_errors(loop_errors);
            append_control_flow_errors(conditional_errors);
        } else {
            append_filtered_errors(loop_errors, [](const SyntaxError& err) {
                return err.error_code == "SYN002" &&
                       err.message.find("'do' keyword") != std::string::npos;
            });
            append_filtered_errors(conditional_errors, [](const SyntaxError& err) {
                if (err.error_code == "SYN004") {
                    return err.message.find("'then' keyword") != std::string::npos;
                }
                if (err.error_code == "SYN008") {
                    return err.message.find("'in' keyword") != std::string::npos;
                }
                return false;
            });
        }
    }

    auto is_blocking_error = [&](const SyntaxError& error) {
        if (config::posix_mode && error.error_code.rfind("POSIX", 0) == 0) {
            return true;
        }
        if (error.error_code == "SYN002" || error.error_code == "SYN003" ||
            error.error_code == "SYN004" || error.error_code == "SYN008") {
            return true;
        }
        return error.severity == ErrorSeverity::CRITICAL && error.error_code != "SYN007";
    };

    bool has_blocking_errors = false;
    for (const auto& error : errors) {
        if (is_blocking_error(error)) {
            has_blocking_errors = true;
            break;
        }
    }

    if (has_blocking_errors && print_errors) {
        std::vector<SyntaxError> blocking_errors;
        for (const auto& error : errors) {
            if (is_blocking_error(error)) {
                blocking_errors.push_back(error);
            }
        }
        if (!blocking_errors.empty()) {
            emit_validation_errors(blocking_errors);
        }
    }

    return has_blocking_errors;
}

bool ShellScriptInterpreter::needs_additional_input(const std::vector<std::string>& lines) {
    if (lines.empty()) {
        return false;
    }

    if (has_incomplete_construct_errors(validate_script_syntax(lines))) {
        return true;
    }

    if (has_incomplete_construct_errors(validate_loop_syntax(lines))) {
        return true;
    }

    if (has_incomplete_construct_errors(validate_conditional_syntax(lines))) {
        return true;
    }

    return false;
}

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::validate_comprehensive_syntax(const std::vector<std::string>& lines,
                                                      bool check_semantics, bool check_style) {
    std::vector<SyntaxError> all_errors;

    auto add_errors = [&all_errors](const std::vector<SyntaxError>& new_errors) {
        all_errors.insert(all_errors.end(), new_errors.begin(), new_errors.end());
    };

    add_errors(validate_script_syntax(lines));
    add_errors(validate_variable_usage(lines));
    add_errors(validate_redirection_syntax(lines));
    add_errors(validate_arithmetic_expressions(lines));
    add_errors(validate_parameter_expansions(lines));
    add_errors(analyze_control_flow());
    add_errors(validate_pipeline_syntax(lines));
    add_errors(validate_function_syntax(lines));
    add_errors(validate_loop_syntax(lines));
    add_errors(validate_conditional_syntax(lines));
    add_errors(validate_array_syntax(lines));
    add_errors(validate_heredoc_syntax(lines));

    if (check_semantics) {
        add_errors(validate_command_existence());
    }

    if (check_style) {
        add_errors(check_style_guidelines(lines));
    }

    return all_errors;
}

#include "interpreter.h"
#include "interpreter_utils.h"

#include "parser_utils.h"
#include "validation_common.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <initializer_list>
#include <sstream>
#include <string>
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

        print_error({map_category_to_error_type(error.category), error.severity, "",
                     build_error_message(error), suggestions});
    }
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
    const std::string& line, const std::string& keyword, size_t display_line,
    std::vector<std::tuple<std::string, std::string, size_t>>& control_stack) {
    if (!starts_with_keyword_token(line, keyword))
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
                    control_stack.push_back({"do", keyword, display_line});
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
    std::vector<std::tuple<std::string, std::string, size_t>>& control_stack) {
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
                control_stack.push_back({"{", "{", display_line});
            }
        } else {
            control_stack.push_back({"{", "{", display_line});
        }
    } else {
        control_stack.push_back({"function", "function", display_line});
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
    std::vector<std::tuple<std::string, std::string, size_t>>& control_stack) {
    auto try_keyword = [&](const std::string& keyword) -> bool {
        size_t position = 0;
        if (!find_embedded_loop_keyword(trimmed_line, keyword, position)) {
            return false;
        }

        std::string remainder = trim(trimmed_line.substr(position));
        auto [tokens, first_token] = tokenize_and_get_first(remainder);
        if (first_token != keyword) {
            return false;
        }

        if (has_inline_terminator(remainder, "done")) {
            return false;
        }

        if (keyword == "for") {
            auto for_check = analyze_for_loop_syntax(tokens, remainder);
            control_stack.push_back({"for", "for", display_line});
            if (for_check.has_inline_do) {
                std::get<0>(control_stack.back()) = "do";
            }
        } else {
            auto loop_check = analyze_while_until_syntax(keyword, remainder, tokens);
            control_stack.push_back({keyword, keyword, display_line});
            if (loop_check.has_inline_do) {
                std::get<0>(control_stack.back()) = "do";
            }
        }
        return true;
    };

    if (try_keyword("while")) {
        return true;
    }
    if (try_keyword("until")) {
        return true;
    }
    if (try_keyword("for")) {
        return true;
    }
    return false;
}

}  // namespace

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::validate_script_syntax(
    const std::vector<std::string>& lines) {
    std::vector<SyntaxError> errors;
    std::vector<std::string> sanitized_lines = sanitize_lines_for_validation(lines);

    std::vector<std::tuple<std::string, std::string, size_t>> control_stack;
    bool encountered_unclosed_quote = false;

    auto expected_close_for_entry = [](const std::tuple<std::string, std::string, size_t>& entry) {
        const std::string& current_state = std::get<0>(entry);
        const std::string& opening_statement = std::get<1>(entry);

        if (opening_statement == "if" || current_state == "then" || current_state == "elif" ||
            current_state == "else") {
            return std::string("fi");
        }
        if (opening_statement == "while" || opening_statement == "until" ||
            opening_statement == "for" || current_state == "do") {
            return std::string("done");
        }
        if (opening_statement == "case") {
            return std::string("esac");
        }
        if (opening_statement == "{" || opening_statement == "function") {
            return std::string("}");
        }
        return std::string();
    };

    auto report_unclosed_entry = [&](const std::tuple<std::string, std::string, size_t>& entry) {
        const std::string& current_state = std::get<0>(entry);
        const std::string& opening_statement = std::get<1>(entry);
        size_t opening_line = std::get<2>(entry);
        std::string expected_close = expected_close_for_entry(entry);
        if (expected_close.empty()) {
            return;
        }

        if (current_state == "case-header") {
            return;
        }

        if (opening_statement == current_state &&
            (opening_statement == "for" || opening_statement == "while" ||
             opening_statement == "until" || opening_statement == "if")) {
            return;
        }

        std::string msg = "Unclosed '";
        msg += opening_statement;
        msg += "' from line ";
        msg += std::to_string(opening_line);
        msg += " - missing '";
        msg += expected_close;
        msg += "'";
        SyntaxError syn_err(opening_line, msg, "");

        if (opening_statement == "{" || opening_statement == "function") {
            syn_err.error_code = "SYN007";
            syn_err.suggestion =
                "Add closing '}' to match the opening on line " + std::to_string(opening_line);
        } else {
            syn_err.error_code = "SYN001";
            syn_err.suggestion = "Add '" + expected_close + "' to close the '" + opening_statement +
                                 "' that started on line " + std::to_string(opening_line);
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

    auto unwind_until_allowed = [&](std::initializer_list<const char*> allowed_states,
                                    const char* closing_keyword) {
        while (!control_stack.empty()) {
            const auto& top = control_stack.back();
            const std::string& current_state = std::get<0>(top);

            for (const char* allowed : allowed_states) {
                if (current_state == allowed) {
                    return true;
                }
            }

            std::string expected_close = expected_close_for_entry(top);
            if (expected_close.empty() ||
                (closing_keyword != nullptr && expected_close == closing_keyword)) {
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
            if (std::get<0>(stack_item) == "case") {
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
                (after_semicolon.rfind("then", 0) == 0 || after_semicolon.rfind("elif", 0) == 0 ||
                 after_semicolon.rfind("else", 0) == 0 || after_semicolon.rfind("fi", 0) == 0)) {
                trimmed_for_parsing = std::move(after_semicolon);
            }
        }

        if (trimmed_for_parsing.rfind("if ", 0) == 0 &&
            (trimmed_for_parsing.find("; then") != std::string::npos ||
             trimmed_for_parsing.find(";then") != std::string::npos)) {
            if (!has_inline_terminator(trimmed_for_parsing, "fi")) {
                control_stack.push_back({"if", "if", display_line});

                std::get<0>(control_stack.back()) = "then";
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
        } else if (handle_inline_loop_header(trimmed_for_parsing, "while", display_line,
                                             control_stack) ||
                   handle_inline_loop_header(trimmed_for_parsing, "until", display_line,
                                             control_stack) ||
                   handle_inline_loop_header(trimmed_for_parsing, "for", display_line,
                                             control_stack)) {
        } else {
            handle_embedded_loop_header(trimmed_for_parsing, display_line, control_stack);

            auto tokens = tokenize_whitespace(trimmed_for_parsing);

            if (!tokens.empty()) {
                const std::string& first_token = tokens[0];

                auto require_top = [&](std::initializer_list<const char*> allowed,
                                       const std::string& message) {
                    if (control_stack.empty()) {
                        errors.push_back({display_line, message, line});
                        return false;
                    }
                    const std::string& top = std::get<0>(control_stack.back());
                    for (const char* value : allowed) {
                        if (top == value) {
                            return true;
                        }
                    }
                    errors.push_back({display_line, message, line});
                    return false;
                };

                if (first_token == "if") {
                    control_stack.push_back({"if", "if", display_line});
                } else if (first_token == "then") {
                    if (require_top({"if"}, "'then' without matching 'if'")) {
                        std::get<0>(control_stack.back()) = "then";
                    }
                } else if (first_token == "elif") {
                    if (require_top({"then", "elif"}, "'elif' without matching 'if...then'")) {
                        std::get<0>(control_stack.back()) = "elif";

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
                } else if (first_token == "else") {
                    if (require_top({"then", "elif"}, "'else' without matching 'if...then'")) {
                        std::get<0>(control_stack.back()) = "else";
                    }
                } else if (first_token == "fi") {
                    if (require_top({"then", "elif", "else"}, "'fi' without matching 'if'")) {
                        control_stack.pop_back();
                    }
                }

                else if (first_token == "while" || first_token == "until") {
                    auto loop_check =
                        analyze_while_until_syntax(first_token, trimmed_for_parsing, tokens);
                    control_stack.push_back({first_token, first_token, display_line});
                    if (loop_check.has_inline_do) {
                        std::get<0>(control_stack.back()) = "do";
                    }
                } else if (first_token == "do") {
                    if (require_top({"while", "until", "for"},
                                    "'do' without matching 'while', 'until', or 'for'")) {
                        std::get<0>(control_stack.back()) = "do";
                    }
                } else if (first_token == "done") {
                    if (unwind_until_allowed({"do"}, "done")) {
                        control_stack.pop_back();
                    } else if (require_top({"do"}, "'done' without matching 'do'")) {
                        control_stack.pop_back();
                    }
                }

                else if (first_token == "for") {
                    auto for_check = analyze_for_loop_syntax(tokens, trimmed_for_parsing);
                    if (for_check.missing_in_keyword) {
                        errors.push_back(
                            {display_line, "'for' statement missing 'in' clause", line});
                    }
                    control_stack.push_back({"for", "for", display_line});
                    if (for_check.has_inline_do) {
                        std::get<0>(control_stack.back()) = "do";
                    }
                }

                else if (first_token == "case") {
                    auto case_check = analyze_case_syntax(tokens);
                    if (case_check.missing_in_keyword) {
                        errors.push_back(
                            {display_line, "'case' statement missing 'in' clause", line});
                    }

                    bool header_complete = !case_check.incomplete && !case_check.missing_in_keyword;
                    if (!has_inline_terminator(trimmed_for_parsing, "esac")) {
                        control_stack.push_back({"case-header", "case", display_line});
                        if (header_complete) {
                            std::get<0>(control_stack.back()) = "case";
                        }
                    }
                }

                else if (first_token == "in") {
                    if (!control_stack.empty()) {
                        auto& top = control_stack.back();
                        if (std::get<1>(top) == "case" && std::get<0>(top) == "case-header") {
                            std::get<0>(top) = "case";
                        }
                    }
                } else if (first_token == "esac") {
                    if (require_top({"case"}, "'esac' without matching 'case'")) {
                        control_stack.pop_back();
                    }
                }

                else if (first_token == "function") {
                    if (tokens.size() < 2) {
                        errors.push_back({display_line, "'function' missing function name", line});
                    }
                    push_function_context(trimmed, display_line, control_stack);
                } else if (tokens.size() >= 2 && tokens[1] == "()") {
                    push_function_context(trimmed, display_line, control_stack);
                }

                else if (!trimmed.empty() && trimmed.back() == '{') {
                    control_stack.push_back({"{", "{", display_line});
                } else if (first_token == "}") {
                    if (require_top({"{", "function"}, "Unmatched closing brace '}'")) {
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
    std::vector<SyntaxError> errors = validate_script_syntax(lines);

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

    auto append_filtered_errors = [&errors](const std::vector<SyntaxError>& source,
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

    auto is_blocking_error = [&](const SyntaxError& error) {
        if (error.error_code == "SYN002") {
            if (enforce_inline_completion) {
                return true;
            }
            return error.message.find("'do' keyword") != std::string::npos;
        }
        if (error.error_code == "SYN004") {
            if (enforce_inline_completion) {
                return true;
            }
            return error.message.find("'then' keyword") != std::string::npos;
        }
        if (error.error_code == "SYN008") {
            if (enforce_inline_completion) {
                return true;
            }
            return error.message.find("'in' keyword") != std::string::npos;
        }
        if (error.error_code == "SYN003") {
            return enforce_inline_completion;
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

std::vector<ShellScriptInterpreter::SyntaxError>
ShellScriptInterpreter::validate_comprehensive_syntax(const std::vector<std::string>& lines,
                                                      bool check_semantics, bool check_style,
                                                      bool check_performance) {
    (void)check_performance;

    std::vector<SyntaxError> all_errors;

    auto add_errors = [&all_errors](const std::vector<SyntaxError>& new_errors) {
        all_errors.insert(all_errors.end(), new_errors.begin(), new_errors.end());
    };

    add_errors(validate_script_syntax(lines));
    add_errors(validate_variable_usage(lines));
    add_errors(validate_redirection_syntax(lines));
    add_errors(validate_arithmetic_expressions(lines));
    add_errors(validate_parameter_expansions(lines));
    add_errors(analyze_control_flow(lines));
    add_errors(validate_pipeline_syntax(lines));
    add_errors(validate_function_syntax(lines));
    add_errors(validate_loop_syntax(lines));
    add_errors(validate_conditional_syntax(lines));
    add_errors(validate_array_syntax(lines));
    add_errors(validate_heredoc_syntax(lines));

    if (check_semantics) {
        add_errors(validate_command_existence(lines));
    }

    if (check_style) {
        add_errors(check_style_guidelines(lines));
    }

    return all_errors;
}

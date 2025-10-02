#include "shell_script_interpreter.h"
#include "shell_script_interpreter_error_reporter.h"
#include "shell_script_interpreter_utils.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using shell_script_interpreter::detail::process_line_for_validation;
using shell_script_interpreter::detail::strip_inline_comment;
using shell_script_interpreter::detail::trim;

namespace {

using SyntaxError = ShellScriptInterpreter::SyntaxError;
using ErrorSeverity = ShellScriptInterpreter::ErrorSeverity;
using ErrorCategory = ShellScriptInterpreter::ErrorCategory;

bool has_inline_terminator(const std::string& text, const std::string& terminator) {
    size_t pos = 0;
    while ((pos = text.find(terminator, pos)) != std::string::npos) {
        bool valid_start = (pos == 0 || text[pos - 1] == ' ' || text[pos - 1] == '\t' || text[pos - 1] == ';');
        bool valid_end = (pos + terminator.length() >= text.length() || text[pos + terminator.length()] == ' ' ||
                          text[pos + terminator.length()] == '\t' || text[pos + terminator.length()] == ';');

        if (valid_start && valid_end) {
            return true;
        }
        pos++;
    }
    return false;
}

bool handle_inline_loop_header(const std::string& line, const std::string& keyword, size_t display_line,
                               std::vector<std::tuple<std::string, std::string, size_t>>& control_stack) {
    if (line.rfind(keyword + " ", 0) == 0 && line.find("; do") != std::string::npos) {
        if (!has_inline_terminator(line, "done")) {
            control_stack.push_back({"do", keyword, display_line});
        }
        return true;
    }
    return false;
}

struct QuoteState {
    bool in_quotes = false;
    char quote_char = '\0';
    bool escaped = false;
};

bool should_skip_line(const std::string& line) {
    size_t first_non_space = line.find_first_not_of(" \t");
    return first_non_space == std::string::npos || line[first_non_space] == '#';
}

bool should_process_char(QuoteState& state, char c, bool ignore_single_quotes, bool process_escaped_chars = true) {
    if (state.escaped) {
        state.escaped = false;
        return process_escaped_chars;
    }

    if (c == '\\' && (!state.in_quotes || state.quote_char != '\'')) {
        state.escaped = true;
        return false;
    }

    if (!state.in_quotes && (c == '"' || c == '\'')) {
        state.in_quotes = true;
        state.quote_char = c;
        return false;
    }

    if (state.in_quotes && c == state.quote_char) {
        state.in_quotes = false;
        state.quote_char = '\0';
        return false;
    }

    if (state.in_quotes && state.quote_char == '\'' && ignore_single_quotes)
        return false;

    return true;
}

enum class IterationAction {
    Continue,
    Break
};

template <typename Callback>
void for_each_effective_char(const std::string& line, bool ignore_single_quotes, bool process_escaped_chars, Callback&& callback) {
    QuoteState state;
    size_t index = 0;
    while (index < line.size()) {
        char c = line[index];
        if (!should_process_char(state, c, ignore_single_quotes, process_escaped_chars)) {
            ++index;
            continue;
        }
        size_t next_index = index;
        if (callback(index, c, state, next_index) == IterationAction::Break) {
            break;
        }
        index = next_index < index ? index + 1 : next_index + 1;
    }
}

bool extract_trimmed_line(const std::string& line, std::string& trimmed_line, size_t& first_non_space) {
    first_non_space = line.find_first_not_of(" \t");
    if (first_non_space == std::string::npos)
        return false;

    if (line[first_non_space] == '#')
        return false;

    trimmed_line = line.substr(first_non_space);
    return true;
}

template <typename ProcessFunc>
std::vector<SyntaxError> process_lines_for_validation(const std::vector<std::string>& lines, ProcessFunc process_line_func) {
    std::vector<SyntaxError> errors;

    for (size_t line_num = 0; line_num < lines.size(); ++line_num) {
        const std::string& line = lines[line_num];
        size_t display_line = line_num + 1;

        std::string trimmed_line;
        size_t first_non_space = 0;
        if (!extract_trimmed_line(line, trimmed_line, first_non_space)) {
            continue;
        }

        auto line_errors = process_line_func(line, trimmed_line, display_line, first_non_space);
        errors.insert(errors.end(), line_errors.begin(), line_errors.end());
    }

    return errors;
}

std::vector<std::string> tokenize_whitespace(const std::string& input) {
    std::vector<std::string> tokens;
    std::stringstream ss(input);
    std::string token;
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

void push_function_context(const std::string& trimmed_line, size_t display_line,
                           std::vector<std::tuple<std::string, std::string, size_t>>& control_stack) {
    if (!trimmed_line.empty() && trimmed_line.back() == '{') {
        size_t open_brace = trimmed_line.find('{');
        if (open_brace != std::string::npos) {
            std::string after_brace = trimmed_line.substr(open_brace + 1);

            int brace_count = 1;
            for (char c : after_brace) {
                if (c == '{')
                    brace_count++;
                else if (c == '}')
                    brace_count--;
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

bool is_valid_identifier_start(char c);
bool is_valid_identifier_char(char c);

void append_function_name_errors(std::vector<SyntaxError>& errors, size_t display_line, const std::string& line,
                                 const std::string& func_name, const std::string& missing_name_suggestion) {
    if (func_name.empty() || func_name == "()") {
        errors.push_back(SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR, ErrorCategory::SYNTAX, "FUNC001",
                                     "Function declaration missing name", line, missing_name_suggestion));
        return;
    }

    if (!is_valid_identifier_start(func_name[0])) {
        errors.push_back(SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR, ErrorCategory::SYNTAX, "FUNC002",
                                     "Invalid function name '" + func_name + "' - must start with letter or underscore", line,
                                     "Use valid function name starting with letter or underscore"));
        return;
    }

    for (char c : func_name) {
        if (!is_valid_identifier_char(c)) {
            errors.push_back(
                SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR, ErrorCategory::SYNTAX, "FUNC002",
                            "Invalid function name '" + func_name + "' - contains invalid character '" + std::string(1, c) + "'", line,
                            "Use only letters, numbers, and underscores in "
                            "function names"));
            return;
        }
    }
}

size_t adjust_display_line(const std::string& text, size_t base_line, size_t offset) {
    size_t limit = std::min(offset, text.size());
    return base_line + static_cast<size_t>(std::count(text.begin(), text.begin() + limit, '\n'));
}

struct ForLoopCheckResult {
    bool incomplete = false;
    bool missing_in_keyword = false;
    bool missing_do_keyword = false;
};

ForLoopCheckResult analyze_for_loop_syntax(const std::vector<std::string>& tokens, const std::string& trimmed_line) {
    ForLoopCheckResult result;

    if (tokens.size() < 3) {
        result.incomplete = true;
        return result;
    }

    bool has_in_clause = std::find(tokens.begin(), tokens.end(), "in") != tokens.end();
    if (!has_in_clause) {
        result.missing_in_keyword = true;
        return result;
    }

    bool has_do = std::find(tokens.begin(), tokens.end(), "do") != tokens.end();
    bool has_semicolon = trimmed_line.find(';') != std::string::npos;
    if (!has_do && !has_semicolon) {
        result.missing_do_keyword = true;
    }

    return result;
}

struct WhileUntilCheckResult {
    bool missing_do_keyword = false;
    bool missing_condition = false;
    bool unclosed_test = false;
};

WhileUntilCheckResult analyze_while_until_syntax(const std::string& first_token, const std::string& trimmed_line,
                                                 const std::vector<std::string>& tokens) {
    WhileUntilCheckResult result;

    bool has_do = std::find(tokens.begin(), tokens.end(), "do") != tokens.end();
    bool has_semicolon = trimmed_line.find(';') != std::string::npos;
    if (!has_do && !has_semicolon) {
        result.missing_do_keyword = true;
    }

    size_t kw_pos = trimmed_line.find(first_token);
    std::string after_kw = kw_pos != std::string::npos ? trimmed_line.substr(kw_pos + first_token.size()) : "";
    size_t non = after_kw.find_first_not_of(" \t");
    if (non != std::string::npos)
        after_kw = after_kw.substr(non);
    else
        after_kw.clear();

    bool immediate_do = (after_kw == "do" || after_kw.find("do ") == 0 || after_kw.find("do\t") == 0);

    size_t semi = after_kw.find(';');
    if (semi != std::string::npos)
        after_kw = after_kw.substr(0, semi);

    size_t do_pos = after_kw.rfind(" do");
    if (do_pos != std::string::npos && do_pos == after_kw.size() - 3)
        after_kw = after_kw.substr(0, do_pos);
    do_pos = after_kw.rfind("\tdo");
    if (do_pos != std::string::npos && do_pos == after_kw.size() - 3)
        after_kw = after_kw.substr(0, do_pos);

    std::string cond = after_kw;
    while (!cond.empty() && isspace(static_cast<unsigned char>(cond.back())))
        cond.pop_back();

    if (cond.empty() || immediate_do) {
        result.missing_condition = true;
    } else {
        if ((cond.find('[') != std::string::npos && cond.find(']') == std::string::npos) ||
            (cond.find("[[") != std::string::npos && cond.find("]]") == std::string::npos)) {
            result.unclosed_test = true;
        }
    }

    return result;
}

struct IfCheckResult {
    bool missing_then_keyword = false;
    bool missing_condition = false;
};

IfCheckResult analyze_if_syntax(const std::vector<std::string>& tokens, const std::string& trimmed_line) {
    IfCheckResult result;

    bool has_then_on_line = std::find(tokens.begin(), tokens.end(), "then") != tokens.end();
    bool has_semicolon = trimmed_line.find(';') != std::string::npos;

    if (!has_then_on_line && !has_semicolon) {
        result.missing_then_keyword = true;
    }

    if (tokens.size() == 1 || (tokens.size() == 2 && tokens[1] == "then")) {
        result.missing_condition = true;
    }

    return result;
}

struct CaseCheckResult {
    bool incomplete = false;
    bool missing_in_keyword = false;
};

CaseCheckResult analyze_case_syntax(const std::vector<std::string>& tokens) {
    CaseCheckResult result;

    if (tokens.size() < 3) {
        result.incomplete = true;
        return result;
    }

    bool has_in_keyword = std::find(tokens.begin(), tokens.end(), "in") != tokens.end();
    if (!has_in_keyword) {
        result.missing_in_keyword = true;
    }

    return result;
}

bool is_valid_identifier_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool is_valid_identifier_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool is_valid_identifier(const std::string& text) {
    if (text.empty() || !is_valid_identifier_start(text.front()))
        return false;
    for (size_t i = 1; i < text.size(); ++i) {
        if (!is_valid_identifier_char(text[i]))
            return false;
    }
    return true;
}

bool is_allowed_array_index_char(char c) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
        return true;
    switch (c) {
        case '+':
        case '-':
        case '*':
        case '/':
        case '%':
        case '(':
        case ')':
            return true;
        default:
            return false;
    }
}

bool validate_array_index_expression(const std::string& index_text, std::string& issue) {
    if (index_text.empty()) {
        issue = "Empty array index";
        return false;
    }
    if (index_text.find_first_of(" \t") != std::string::npos) {
        issue = "Array index cannot contain whitespace";
        return false;
    }
    for (char c : index_text) {
        if (!is_allowed_array_index_char(c)) {
            issue = "Invalid characters in array index";
            return false;
        }
    }
    return true;
}

}  // namespace

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::validate_script_syntax(const std::vector<std::string>& lines) {
    std::vector<SyntaxError> errors;

    std::vector<std::tuple<std::string, std::string, size_t>> control_stack;

    for (size_t line_num = 0; line_num < lines.size(); ++line_num) {
        const std::string& line = lines[line_num];
        size_t display_line = line_num + 1;

        std::string trimmed;
        size_t first_non_space = 0;
        if (!extract_trimmed_line(line, trimmed, first_non_space)) {
            continue;
        }

        std::string line_without_comments = strip_inline_comment(line);
        QuoteState quote_state;

        for (char c : line_without_comments) {
            should_process_char(quote_state, c, false, false);
        }

        if (quote_state.in_quotes) {
            errors.push_back({display_line, "Unclosed quote: missing closing " + std::string(1, quote_state.quote_char), line});
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

        bool line_has_case = trimmed.find("case ") != std::string::npos && trimmed.find(" in ") != std::string::npos;

        bool looks_like_case_pattern = (in_case_block || line_has_case) && (trimmed.find(")") != std::string::npos);

        if (in_case_block && trimmed.find(")") != std::string::npos) {
            size_t paren_pos = trimmed.find(")");
            if (paren_pos != std::string::npos) {
                std::string before_paren = trimmed.substr(0, paren_pos);

                before_paren = trim(before_paren);
                if (!before_paren.empty() &&
                    (before_paren.front() == '"' || before_paren.front() == '\'' || before_paren == "*" || isalnum(before_paren.front()))) {
                    looks_like_case_pattern = true;
                }
            }
        }

        if (!looks_like_case_pattern) {
            for (size_t i = 0; i < line_without_comments.length(); ++i) {
                char c = line_without_comments[i];

                if (!should_process_char(quote_state, c, false, false)) {
                    continue;
                }

                if (!quote_state.in_quotes) {
                    if (c == '(')
                        paren_balance++;
                    else if (c == ')')
                        paren_balance--;
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

        if (trimmed_for_parsing.rfind("if ", 0) == 0 &&
            (trimmed_for_parsing.find("; then") != std::string::npos || trimmed_for_parsing.find(";then") != std::string::npos)) {
            if (!has_inline_terminator(trimmed_for_parsing, "fi")) {
                control_stack.push_back({"if", "if", display_line});

                std::get<0>(control_stack.back()) = "then";
            }
        } else if (handle_inline_loop_header(trimmed_for_parsing, "while", display_line, control_stack) ||
                   handle_inline_loop_header(trimmed_for_parsing, "until", display_line, control_stack) ||
                   handle_inline_loop_header(trimmed_for_parsing, "for", display_line, control_stack)) {
        } else {
            auto tokens = tokenize_whitespace(trimmed_for_parsing);

            if (!tokens.empty()) {
                const std::string& first_token = tokens[0];

                auto require_top = [&](std::initializer_list<const char*> allowed, const std::string& message) {
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
                    control_stack.push_back({first_token, first_token, display_line});
                } else if (first_token == "do") {
                    if (require_top({"while", "until", "for"},
                                    "'do' without matching 'while', 'until', "
                                    "or 'for'")) {
                        std::get<0>(control_stack.back()) = "do";
                    }
                } else if (first_token == "done") {
                    if (require_top({"do"}, "'done' without matching 'do'")) {
                        control_stack.pop_back();
                    }
                }

                else if (first_token == "for") {
                    auto for_check = analyze_for_loop_syntax(tokens, trimmed_for_parsing);
                    if (for_check.missing_in_keyword) {
                        errors.push_back({display_line, "'for' statement missing 'in' clause", line});
                    }
                    control_stack.push_back({"for", "for", display_line});
                }

                else if (first_token == "case") {
                    auto case_check = analyze_case_syntax(tokens);
                    if (case_check.missing_in_keyword) {
                        errors.push_back({display_line, "'case' statement missing 'in' clause", line});
                    }

                    if (!has_inline_terminator(trimmed_for_parsing, "esac")) {
                        control_stack.push_back({"case", "case", display_line});
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

    while (!control_stack.empty()) {
        auto& unclosed = control_stack.back();
        const std::string& current_state = std::get<0>(unclosed);
        const std::string& opening_statement = std::get<1>(unclosed);
        size_t opening_line = std::get<2>(unclosed);
        std::string expected_close;

        if (opening_statement == "if" || current_state == "then" || current_state == "elif" || current_state == "else") {
            expected_close = "fi";
        } else if (opening_statement == "while" || opening_statement == "for" || current_state == "do") {
            expected_close = "done";
        } else if (opening_statement == "case") {
            expected_close = "esac";
        } else if (opening_statement == "{" || opening_statement == "function") {
            expected_close = "}";
        }

        {
            std::string msg =
                "Unclosed '" + opening_statement + "' from line " + std::to_string(opening_line) + " - missing '" + expected_close + "'";
            SyntaxError syn_err(opening_line, msg, "");
            if (opening_statement == "{" || opening_statement == "function") {
                syn_err.error_code = "SYN007";
                syn_err.suggestion = "Add closing '}' to match the opening on line " + std::to_string(opening_line);
                syn_err.severity = ErrorSeverity::CRITICAL;
            } else {
                syn_err.error_code = "SYN001";
                syn_err.suggestion = "Add '" + expected_close + "' to close the '" + opening_statement + "' that started on line " +
                                     std::to_string(opening_line);
            }
            syn_err.category = ErrorCategory::CONTROL_FLOW;
            syn_err.severity = ErrorSeverity::CRITICAL;
            errors.push_back(syn_err);
        }
        control_stack.pop_back();
    }

    return errors;
}

bool ShellScriptInterpreter::has_syntax_errors(const std::vector<std::string>& lines, bool print_errors) {
    std::vector<SyntaxError> errors = validate_script_syntax(lines);

    bool has_blocking_errors = false;
    for (const auto& error : errors) {
        if (error.severity == ErrorSeverity::CRITICAL && error.error_code != "SYN007") {
            has_blocking_errors = true;
            break;
        }
    }

    if (has_blocking_errors && print_errors) {
        std::vector<SyntaxError> blocking_errors;
        for (const auto& error : errors) {
            if (error.severity == ErrorSeverity::CRITICAL && error.error_code != "SYN007") {
                blocking_errors.push_back(error);
            }
        }
        if (!blocking_errors.empty()) {
            shell_script_interpreter::ErrorReporter::print_error_report(blocking_errors, true, true);
        }
    }

    return has_blocking_errors;
}

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::validate_comprehensive_syntax(
    const std::vector<std::string>& lines, bool check_semantics, bool check_style, bool check_performance) {
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

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::validate_variable_usage(const std::vector<std::string>& lines) {
    std::vector<SyntaxError> errors;
    std::map<std::string, std::vector<size_t>> defined_vars;
    std::map<std::string, std::vector<size_t>> used_vars;

    for (size_t line_num = 0; line_num < lines.size(); ++line_num) {
        const std::string& line = lines[line_num];
        size_t display_line = line_num + 1;

        if (should_skip_line(line)) {
            continue;
        }

        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string before_eq = line.substr(0, eq_pos);

            size_t start = before_eq.find_first_not_of(" \t");
            if (start != std::string::npos) {
                before_eq = before_eq.substr(start);

                before_eq = trim(before_eq);

                if (is_valid_identifier(before_eq)) {
                    defined_vars[before_eq].push_back(adjust_display_line(line, display_line, eq_pos));
                }
            }
        }

        QuoteState quote_state;

        for (size_t i = 0; i < line.length(); ++i) {
            char c = line[i];

            if (!should_process_char(quote_state, c, true)) {
                continue;
            }

            if (c == '$' && i + 1 < line.length()) {
                std::string var_name;
                size_t var_start = i + 1;
                size_t var_end = var_start;

                if (line[var_start] == '{') {
                    var_start++;
                    var_end = line.find('}', var_start);
                    if (var_end != std::string::npos) {
                        var_name = line.substr(var_start, var_end - var_start);

                        size_t colon_pos = var_name.find(':');
                        if (colon_pos != std::string::npos) {
                            var_name = var_name.substr(0, colon_pos);
                        }
                    } else {
                        errors.push_back(SyntaxError({display_line, i, i + 2, 0}, ErrorSeverity::ERROR, ErrorCategory::VARIABLES, "VAR001",
                                                     "Unclosed variable expansion ${", line, "Add closing brace '}'"));
                        continue;
                    }
                } else if (std::isalpha(line[var_start]) || line[var_start] == '_') {
                    while (var_end < line.length() && (std::isalnum(line[var_end]) || line[var_end] == '_')) {
                        var_end++;
                    }
                    var_name = line.substr(var_start, var_end - var_start);
                }

                if (!var_name.empty()) {
                    used_vars[var_name].push_back(adjust_display_line(line, display_line, i));
                }
            }
        }
    }

    for (const auto& [var_name, usage_lines] : used_vars) {
        if (defined_vars.find(var_name) == defined_vars.end()) {
            if (var_name != "PATH" && var_name != "HOME" && var_name != "USER" && var_name != "PWD" && var_name != "SHELL" &&
                var_name != "TERM" && var_name != "TMUX" && var_name != "DISPLAY" && var_name != "EDITOR" && var_name != "PAGER" &&
                var_name != "LANG" && var_name != "LC_ALL" && var_name != "TZ" && var_name != "SSH_CLIENT" && var_name != "SSH_TTY" &&
                !std::isdigit(var_name[0])) {
                for (size_t line : usage_lines) {
                    errors.push_back(SyntaxError({line, 0, 0, 0}, ErrorSeverity::WARNING, ErrorCategory::VARIABLES, "VAR002",
                                                 "Variable '" + var_name + "' used but not defined in this script", "",
                                                 "Define the variable before use: " + var_name + "=value"));
                }
            }
        }
    }

    for (const auto& [var_name, def_lines] : defined_vars) {
        if (used_vars.find(var_name) == used_vars.end()) {
            for (size_t line : def_lines) {
                errors.push_back(SyntaxError({line, 0, 0, 0}, ErrorSeverity::INFO, ErrorCategory::VARIABLES, "VAR003",
                                             "Variable '" + var_name + "' defined but never used", "",
                                             "Remove unused variable or add usage"));
            }
        }
    }

    return errors;
}

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::validate_redirection_syntax(
    const std::vector<std::string>& lines) {
    return process_lines_for_validation(
        lines, [](const std::string& line, const std::string&, size_t display_line, size_t) -> std::vector<SyntaxError> {
            std::vector<SyntaxError> line_errors;

            for_each_effective_char(
                line, false, true, [&](size_t i, char c, const QuoteState& state, size_t& next_index) -> IterationAction {
                    if (state.in_quotes) {
                        return IterationAction::Continue;
                    }

                    if (c == '<' || c == '>') {
                        size_t redir_start = i;
                        std::string redir_op;

                        if (c == '>' && i + 1 < line.length()) {
                            if (line[i + 1] == '>') {
                                redir_op = ">>";
                                next_index = i + 1;
                            } else if (line[i + 1] == '&') {
                                redir_op = ">&";
                                next_index = i + 1;
                            } else if (line[i + 1] == '|') {
                                redir_op = ">|";
                                next_index = i + 1;
                            } else {
                                redir_op = ">";
                            }
                        } else if (c == '<' && i + 1 < line.length()) {
                            if (line[i + 1] == '<') {
                                if (i + 2 < line.length() && line[i + 2] == '<') {
                                    redir_op = "<<<";
                                    next_index = i + 2;
                                } else {
                                    redir_op = "<<";
                                    next_index = i + 1;
                                }
                            } else {
                                redir_op = "<";
                            }
                        } else {
                            redir_op = c;
                        }

                        size_t check_pos = next_index + 1;
                        while (check_pos < line.length() && std::isspace(line[check_pos])) {
                            check_pos++;
                        }

                        if (check_pos < line.length()) {
                            char next_char = line[check_pos];
                            if ((redir_op == ">" && next_char == '>') || (redir_op == "<" && next_char == '<') ||
                                (redir_op == ">>" && next_char == '>') || (redir_op == "<<" && next_char == '<')) {
                                line_errors.push_back(SyntaxError({display_line, redir_start, check_pos + 1, 0}, ErrorSeverity::ERROR,
                                                                  ErrorCategory::REDIRECTION, "RED005",
                                                                  "Invalid redirection syntax '" + redir_op + " " + next_char + "'", line,
                                                                  "Use single redirection operator"));
                                return IterationAction::Continue;
                            }
                        }

                        size_t target_start = next_index + 1;
                        while (target_start < line.length() && std::isspace(line[target_start])) {
                            target_start++;
                        }

                        if (target_start >= line.length()) {
                            line_errors.push_back(SyntaxError({display_line, redir_start, next_index + 1, 0}, ErrorSeverity::ERROR,
                                                              ErrorCategory::REDIRECTION, "RED001",
                                                              "Redirection '" + redir_op + "' missing target", line,
                                                              "Add filename or file descriptor after " + redir_op));
                            return IterationAction::Continue;
                        }

                        std::string target;
                        size_t target_end = target_start;
                        bool in_target_quotes = false;
                        char target_quote = '\0';

                        while (target_end < line.length()) {
                            char tc = line[target_end];
                            if (!in_target_quotes && std::isspace(tc)) {
                                break;
                            }
                            if ((tc == '"' || tc == '\'') && !in_target_quotes) {
                                in_target_quotes = true;
                                target_quote = tc;
                            } else if (tc == target_quote && in_target_quotes) {
                                in_target_quotes = false;
                                target_quote = '\0';
                            }
                            target_end++;
                        }

                        target = line.substr(target_start, target_end - target_start);

                        if (redir_op == ">&" || redir_op == "<&") {
                            if (target.empty() || (!std::isdigit(static_cast<unsigned char>(target[0])) && target != "-")) {
                                line_errors.push_back(SyntaxError({display_line, target_start, target_end, 0}, ErrorSeverity::ERROR,
                                                                  ErrorCategory::REDIRECTION, "RED002",
                                                                  "File descriptor redirection requires "
                                                                  "digit or '-'",
                                                                  line, "Use format like 2>&1 or 2>&-"));
                            }
                        } else if (redir_op == "<<") {
                            if (target.empty()) {
                                line_errors.push_back(SyntaxError({display_line, target_start, target_end, 0}, ErrorSeverity::ERROR,
                                                                  ErrorCategory::REDIRECTION, "RED003", "Here document missing delimiter",
                                                                  line, "Provide delimiter like: << EOF"));
                            }
                        }

                        next_index = target_end - 1;
                        return IterationAction::Continue;
                    }

                    if (c == '|' && i + 1 < line.length()) {
                        if (line[i + 1] == '|') {
                            next_index = i + 1;
                        } else {
                            size_t pipe_pos = i;
                            size_t after_pipe = i + 1;
                            while (after_pipe < line.length() && std::isspace(line[after_pipe])) {
                                after_pipe++;
                            }

                            if (after_pipe >= line.length() || line[after_pipe] == '|' || line[after_pipe] == '&') {
                                line_errors.push_back(SyntaxError({display_line, pipe_pos, pipe_pos + 1, 0}, ErrorSeverity::ERROR,
                                                                  ErrorCategory::REDIRECTION, "RED004", "Pipe missing command after '|'",
                                                                  line, "Add command after pipe"));
                            }
                        }
                    }

                    return IterationAction::Continue;
                });

            return line_errors;
        });
}

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::validate_arithmetic_expressions(
    const std::vector<std::string>& lines) {
    return process_lines_for_validation(
        lines, [](const std::string& line, const std::string&, size_t display_line, size_t) -> std::vector<SyntaxError> {
            std::vector<SyntaxError> line_errors;
            for_each_effective_char(line, true, true, [&](size_t i, char c, const QuoteState&, size_t& next_index) -> IterationAction {
                if (c == '$' && i + 2 < line.length() && line[i + 1] == '(' && line[i + 2] == '(') {
                    size_t start = i;
                    size_t paren_count = 2;
                    size_t j = i + 3;
                    std::string expr;

                    while (j < line.length() && paren_count > 0) {
                        if (line[j] == '(') {
                            paren_count++;
                        } else if (line[j] == ')') {
                            paren_count--;
                        }
                        if (paren_count > 0) {
                            expr += line[j];
                        }
                        j++;
                    }

                    const size_t adjusted_line = adjust_display_line(line, display_line, start);

                    if (paren_count > 0) {
                        line_errors.push_back(SyntaxError({adjusted_line, start, j, 0}, ErrorSeverity::ERROR, ErrorCategory::SYNTAX,
                                                          "ARITH001", "Unclosed arithmetic expansion $(()", line, "Add closing ))"));
                    } else {
                        if (expr.empty()) {
                            line_errors.push_back(SyntaxError({adjusted_line, start, j, 0}, ErrorSeverity::ERROR, ErrorCategory::SYNTAX,
                                                              "ARITH002", "Empty arithmetic expression", line,
                                                              "Provide expression inside $(( ))"));
                        } else {
                            std::string trimmed_expr = expr;

                            trimmed_expr.erase(0, trimmed_expr.find_first_not_of(" \t"));
                            trimmed_expr.erase(trimmed_expr.find_last_not_of(" \t") + 1);

                            if (!trimmed_expr.empty()) {
                                char last_char = trimmed_expr.back();
                                if (last_char == '+' || last_char == '-' || last_char == '*' || last_char == '/' || last_char == '%' ||
                                    last_char == '&' || last_char == '|' || last_char == '^') {
                                    line_errors.push_back(SyntaxError({adjusted_line, start, j, 0}, ErrorSeverity::ERROR,
                                                                      ErrorCategory::SYNTAX, "ARITH003",
                                                                      "Incomplete arithmetic expression "
                                                                      "- missing "
                                                                      "operand",
                                                                      line, "Add operand after '" + std::string(1, last_char) + "'"));
                                }
                            }

                            if (expr.find("/0") != std::string::npos || expr.find("% 0") != std::string::npos) {
                                line_errors.push_back(SyntaxError({adjusted_line, start, j, 0}, ErrorSeverity::WARNING,
                                                                  ErrorCategory::SEMANTICS, "ARITH004", "Potential division by zero", line,
                                                                  "Ensure divisor is not zero"));
                            }

                            int balance = 0;
                            for (char ec : expr) {
                                if (ec == '(')
                                    balance++;
                                else if (ec == ')')
                                    balance--;
                                if (balance < 0)
                                    break;
                            }
                            if (balance != 0) {
                                line_errors.push_back(SyntaxError({display_line, start, j, 0}, ErrorSeverity::ERROR, ErrorCategory::SYNTAX,
                                                                  "ARITH005",
                                                                  "Unbalanced parentheses in arithmetic "
                                                                  "expression",
                                                                  line,
                                                                  "Check parentheses balance in "
                                                                  "expression"));
                            }
                        }
                    }

                    next_index = j - 1;
                    return IterationAction::Continue;
                }

                if (c == '$' && i + 1 < line.length() && line[i + 1] == '[') {
                    line_errors.push_back(SyntaxError({display_line, i, i + 2, 0}, ErrorSeverity::WARNING, ErrorCategory::STYLE, "ARITH006",
                                                      "Deprecated arithmetic syntax $[...], use $((...))", line,
                                                      "Replace $[expr] with $((expr))"));
                }

                return IterationAction::Continue;
            });

            return line_errors;
        });
}

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::validate_parameter_expansions(
    const std::vector<std::string>& lines) {
    return process_lines_for_validation(
        lines, [](const std::string& line, const std::string&, size_t display_line, size_t) -> std::vector<SyntaxError> {
            std::vector<SyntaxError> line_errors;
            for_each_effective_char(
                line, true, true, [&](size_t i, char c, const QuoteState& state, size_t& next_index) -> IterationAction {
                    if (c == '$' && i + 1 < line.length() && line[i + 1] == '(') {
                        size_t start = i;
                        size_t paren_count = 1;
                        size_t j = i + 2;
                        bool in_single_quote = false;
                        bool in_double_quote = false;
                        bool escaped = false;

                        while (j < line.length() && paren_count > 0) {
                            char ch = line[j];

                            if (escaped) {
                                escaped = false;
                            } else if (ch == '\\') {
                                escaped = true;
                            } else if (!in_single_quote && ch == '"') {
                                in_double_quote = !in_double_quote;
                            } else if (!in_double_quote && ch == '\'') {
                                in_single_quote = !in_single_quote;
                            } else if (!in_single_quote && !in_double_quote) {
                                if (ch == '(') {
                                    paren_count++;
                                } else if (ch == ')') {
                                    paren_count--;
                                }
                            }
                            j++;
                        }

                        if (paren_count > 0) {
                            line_errors.push_back(SyntaxError({display_line, start, j, 0}, ErrorSeverity::ERROR, ErrorCategory::SYNTAX,
                                                              "SYN005",
                                                              "Unclosed command substitution $() "
                                                              "- missing ')'",
                                                              line, "Add closing parenthesis"));
                        }

                        next_index = j - 1;
                        return IterationAction::Continue;
                    }

                    if (c == '`' && !state.in_quotes) {
                        size_t start = i;
                        size_t j = i + 1;
                        bool found_closing = false;

                        while (j < line.length()) {
                            if (line[j] == '`') {
                                found_closing = true;
                                j++;
                                break;
                            }
                            if (line[j] == '\\')
                                j++;
                            j++;
                        }

                        if (!found_closing) {
                            line_errors.push_back(SyntaxError({display_line, start, j, 0}, ErrorSeverity::ERROR, ErrorCategory::SYNTAX,
                                                              "SYN006",
                                                              "Unclosed backtick command "
                                                              "substitution - missing '`'",
                                                              line, "Add closing backtick"));
                        }

                        next_index = j - 1;
                        return IterationAction::Continue;
                    }

                    if (!state.in_quotes && c == '=' && i > 0) {
                        size_t var_start = i;

                        if (i > 0 && line[i - 1] == ']') {
                            size_t pos = i - 1;
                            int bracket_depth = 0;
                            bool found_open = false;
                            while (pos > 0) {
                                char bc = line[pos];
                                if (bc == ']')
                                    bracket_depth++;
                                else if (bc == '[') {
                                    if (bracket_depth == 0) {
                                        found_open = true;
                                        break;
                                    } else
                                        bracket_depth--;
                                }
                                pos--;
                            }
                            if (found_open) {
                                size_t name_end = pos;
                                size_t name_start = name_end;
                                while (name_start > 0 && (std::isalnum(line[name_start - 1]) || line[name_start - 1] == '_')) {
                                    name_start--;
                                }
                                if (name_start < name_end) {
                                    size_t index_start = pos + 1;
                                    size_t index_end = i - 2;
                                    std::string index_text;
                                    if (index_end >= index_start)
                                        index_text = line.substr(index_start, index_end - index_start + 1);
                                    std::string var_name_only = line.substr(name_start, name_end - name_start);

                                    std::string index_issue;
                                    if (!validate_array_index_expression(index_text, index_issue)) {
                                        line_errors.push_back(SyntaxError({display_line, name_start, i, 0}, ErrorSeverity::ERROR,
                                                                          ErrorCategory::VARIABLES, "VAR005",
                                                                          index_issue + " for array '" + var_name_only + "'", line,
                                                                          "Use a valid numeric or arithmetic "
                                                                          "expression "
                                                                          "index"));
                                    }

                                    var_start = name_start;
                                }
                            }
                        }

                        while (var_start > 0 && (std::isalnum(line[var_start - 1]) || line[var_start - 1] == '_')) {
                            var_start--;
                        }

                        if (var_start < i) {
                            std::string var_name = line.substr(var_start, i - var_start);

                            if (!var_name.empty()) {
                                std::string line_prefix = line.substr(0, var_start);
                                size_t first_word_end = line_prefix.find_first_of(" \t");
                                std::string first_word =
                                    (first_word_end != std::string::npos) ? line_prefix.substr(0, first_word_end) : line_prefix;

                                size_t start_pos = first_word.find_first_not_of(" \t");
                                if (start_pos != std::string::npos) {
                                    first_word = first_word.substr(start_pos);
                                }

                                if (first_word == "export" || first_word == "alias" || first_word == "local" || first_word == "declare" ||
                                    first_word == "readonly") {
                                    return IterationAction::Continue;
                                }

                                if (!is_valid_identifier_start(var_name[0])) {
                                    line_errors.push_back(SyntaxError({display_line, var_start, i, 0}, ErrorSeverity::ERROR,
                                                                      ErrorCategory::VARIABLES, "VAR004",
                                                                      "Invalid variable name '" + var_name +
                                                                          "' - must start with letter or "
                                                                          "underscore",
                                                                      line,
                                                                      "Use variable name starting with "
                                                                      "letter or "
                                                                      "underscore"));
                                }

                                if (var_start == 0 || line.substr(0, var_start).find_first_not_of(" \t") == std::string::npos) {
                                    if (var_start > 0 && std::isspace(line[var_start - 1])) {
                                        line_errors.push_back(SyntaxError({display_line, var_start - 1, i + 1, 0}, ErrorSeverity::ERROR,
                                                                          ErrorCategory::VARIABLES, "VAR005",
                                                                          "Variable assignment cannot have "
                                                                          "spaces around '='",
                                                                          line, "Remove spaces: " + var_name + "=value"));
                                    }
                                    if (i + 1 < line.length() && std::isspace(line[i + 1])) {
                                        line_errors.push_back(SyntaxError({display_line, var_start, i + 2, 0}, ErrorSeverity::ERROR,
                                                                          ErrorCategory::VARIABLES, "VAR005",
                                                                          "Variable assignment cannot have "
                                                                          "spaces around '='",
                                                                          line, "Remove spaces: " + var_name + "=value"));
                                    }
                                }
                            }
                        }
                    }

                    return IterationAction::Continue;
                });

            return line_errors;
        });
}

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::validate_command_existence(const std::vector<std::string>& lines) {
    (void)lines;
    std::vector<SyntaxError> errors;

    return errors;
}

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::analyze_control_flow(const std::vector<std::string>& lines) {
    (void)lines;
    std::vector<SyntaxError> errors;

    return errors;
}

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::check_style_guidelines(const std::vector<std::string>& lines) {
    std::vector<SyntaxError> errors;

    for (size_t line_num = 0; line_num < lines.size(); ++line_num) {
        const std::string& line = lines[line_num];
        size_t display_line = line_num + 1;

        std::string trimmed_line;
        size_t first_non_space = 0;
        if (!extract_trimmed_line(line, trimmed_line, first_non_space)) {
            continue;
        }

        if (trimmed_line.rfind("if ", 0) == 0 || trimmed_line.rfind("while ", 0) == 0 || trimmed_line.rfind("until ", 0) == 0) {
            int logical_ops = 0;
            int bracket_depth = 0;
            int max_bracket_depth = 0;
            QuoteState quote_state;

            for (size_t i = 0; i < line.length() - 1; ++i) {
                char c = line[i];

                if (!should_process_char(quote_state, c, false, false)) {
                    continue;
                }

                if (!quote_state.in_quotes) {
                    if ((c == '&' && line[i + 1] == '&') || (c == '|' && line[i + 1] == '|')) {
                        logical_ops++;
                        i++;
                    } else if (c == '[') {
                        bracket_depth++;
                        max_bracket_depth = std::max(max_bracket_depth, bracket_depth);
                    } else if (c == ']') {
                        bracket_depth--;
                    }
                }
            }

            if (logical_ops > 3) {
                errors.push_back(SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::INFO, ErrorCategory::STYLE, "STYLE001",
                                             "Complex condition with " + std::to_string(logical_ops) + " logical operators", line,
                                             "Consider breaking into multiple if statements or "
                                             "using a function"));
            }

            if (max_bracket_depth > 2) {
                errors.push_back(SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::INFO, ErrorCategory::STYLE, "STYLE002",
                                             "Deeply nested test conditions (depth: " + std::to_string(max_bracket_depth) + ")", line,
                                             "Consider simplifying the condition logic"));
            }
        }

        if (line.length() > 100) {
            errors.push_back(SyntaxError({display_line, 100, line.length(), 0}, ErrorSeverity::INFO, ErrorCategory::STYLE, "STYLE003",
                                         "Line length (" + std::to_string(line.length()) + " chars) exceeds recommended 100 characters",
                                         line, "Consider breaking long lines for better readability"));
        }

        if (line.find('\t') != std::string::npos && line.find(' ') != std::string::npos) {
            size_t first_tab = line.find('\t');
            size_t first_space = line.find(' ');
            if (first_tab < 20 && first_space < 20) {
                errors.push_back(SyntaxError({display_line, 0, std::min(first_tab, first_space), 0}, ErrorSeverity::INFO,
                                             ErrorCategory::STYLE, "STYLE004", "Mixed tabs and spaces for indentation", line,
                                             "Use consistent indentation (either all tabs or all "
                                             "spaces)"));
            }
        }

        if (trimmed_line.find("eval ") != std::string::npos || trimmed_line.find("$(") != std::string::npos) {
            std::string warning_type = trimmed_line.find("eval ") != std::string::npos ? "eval" : "command substitution";
            errors.push_back(SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::WARNING, ErrorCategory::STYLE, "STYLE005",
                                         "Use of " + warning_type + " - potential security risk", line,
                                         "Validate input carefully or consider safer alternatives"));
        }
    }

    return errors;
}

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::validate_pipeline_syntax(const std::vector<std::string>& lines) {
    return process_lines_for_validation(
        lines,
        [](const std::string& line, const std::string& trimmed_line, size_t display_line,
           size_t first_non_space) -> std::vector<SyntaxError> {
            std::vector<SyntaxError> line_errors;

            {
                std::string work = trimmed_line;

                size_t eq = work.find('=');
                if (eq != std::string::npos) {
                    std::string lhs = work.substr(0, eq);

                    while (!lhs.empty() && isspace(static_cast<unsigned char>(lhs.back())))
                        lhs.pop_back();

                    size_t lb = lhs.find('[');
                    size_t rb = lhs.rfind(']');
                    if (lb != std::string::npos && rb != std::string::npos && rb > lb && rb == lhs.size() - 1) {
                        std::string name = lhs.substr(0, lb);
                        bool name_ok = is_valid_identifier(name);
                        std::string index_text = lhs.substr(lb + 1, rb - lb - 1);
                        std::string issue;
                        if (name_ok && !validate_array_index_expression(index_text, issue)) {
                            line_errors.push_back(SyntaxError({display_line, first_non_space + lb, first_non_space + rb + 1, 0},
                                                              ErrorSeverity::ERROR, ErrorCategory::VARIABLES, "VAR005",
                                                              issue + " for array '" + name + "'", line,
                                                              "Use a valid numeric or arithmetic expression "
                                                              "index"));
                        }
                    }
                }
            }

            if (!trimmed_line.empty() && trimmed_line[0] == '|' && !(trimmed_line.size() > 1 && trimmed_line[1] == '|')) {
                line_errors.push_back(SyntaxError({display_line, first_non_space, first_non_space + 1, 0}, ErrorSeverity::ERROR,
                                                  ErrorCategory::REDIRECTION, "PIPE002", "Pipeline cannot start with pipe operator", line,
                                                  "Remove leading pipe or add command before pipe"));
            }

            for_each_effective_char(
                line, false, false, [&](size_t i, char c, const QuoteState& state, size_t& next_index) -> IterationAction {
                    if (!state.in_quotes && c == '|' && i + 1 < line.length()) {
                        if (line[i + 1] == '|' && !(i + 2 < line.length() && line[i + 2] == '|')) {
                            size_t after_logical = i + 2;
                            while (after_logical < line.length() && std::isspace(line[after_logical])) {
                                after_logical++;
                            }

                            if (after_logical < line.length() && line[after_logical] == '|') {
                                line_errors.push_back(SyntaxError({display_line, i, after_logical + 1, 0}, ErrorSeverity::ERROR,
                                                                  ErrorCategory::REDIRECTION, "PIPE001", "Invalid pipeline syntax", line,
                                                                  "Check pipe operator usage"));
                            }
                            next_index = i + 1;
                        } else if (line[i + 1] != '|') {
                            size_t after_pipe = i + 1;
                            while (after_pipe < line.length() && std::isspace(line[after_pipe])) {
                                after_pipe++;
                            }

                            if (after_pipe >= line.length() || line[after_pipe] == '|' || line[after_pipe] == '&') {
                                line_errors.push_back(SyntaxError({display_line, i, i + 1, 0}, ErrorSeverity::ERROR,
                                                                  ErrorCategory::REDIRECTION, "PIPE001", "Pipe missing command after '|'",
                                                                  line, "Add command after pipe"));
                            }
                        }
                    }

                    return IterationAction::Continue;
                });

            return line_errors;
        });
}

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::validate_function_syntax(const std::vector<std::string>& lines) {
    return process_lines_for_validation(
        lines, [](const std::string& line, const std::string& trimmed_line, size_t display_line, size_t) -> std::vector<SyntaxError> {
            std::vector<SyntaxError> line_errors;

            if (trimmed_line.rfind("function", 0) == 0) {
                auto tokens = tokenize_whitespace(trimmed_line);

                if (tokens.size() < 2) {
                    append_function_name_errors(line_errors, display_line, line, "", "Add function name: function name() { ... }");
                } else {
                    append_function_name_errors(line_errors, display_line, line, tokens[1], "Add function name before parentheses");
                }
            }

            size_t paren_pos = trimmed_line.find("()");
            if (paren_pos != std::string::npos && paren_pos > 0 && trimmed_line.rfind("function", 0) != 0) {
                if (trimmed_line.find("{", paren_pos) != std::string::npos) {
                    std::string potential_func = trim(trimmed_line.substr(0, paren_pos));
                    append_function_name_errors(line_errors, display_line, line, potential_func, "Add function name before parentheses");
                }
            }

            return line_errors;
        });
}

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::validate_loop_syntax(const std::vector<std::string>& lines) {
    return process_lines_for_validation(
        lines, [](const std::string& line, const std::string& trimmed_line, size_t display_line, size_t) -> std::vector<SyntaxError> {
            std::vector<SyntaxError> line_errors;

            auto tokens = tokenize_whitespace(trimmed_line);

            if (!tokens.empty()) {
                const std::string& first_token = tokens[0];

                if (first_token == "for") {
                    auto loop_check = analyze_for_loop_syntax(tokens, trimmed_line);
                    if (loop_check.incomplete) {
                        line_errors.push_back(SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR, ErrorCategory::CONTROL_FLOW,
                                                          "SYN002", "'for' statement incomplete", line,
                                                          "Complete for statement: for var in list; do"));
                    } else if (!loop_check.missing_in_keyword && loop_check.missing_do_keyword) {
                        line_errors.push_back(SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR, ErrorCategory::CONTROL_FLOW,
                                                          "SYN002", "'for' statement missing 'do' keyword", line,
                                                          "Add 'do' keyword: for var in list; do"));
                    }
                }

                else if (first_token == "while" || first_token == "until") {
                    auto loop_check = analyze_while_until_syntax(first_token, trimmed_line, tokens);

                    if (loop_check.missing_condition) {
                        line_errors.push_back(SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR, ErrorCategory::CONTROL_FLOW,
                                                          "SYN003", "'" + first_token + "' loop missing condition expression", line,
                                                          "Add a condition expression before 'do'"));
                    } else if (loop_check.unclosed_test) {
                        line_errors.push_back(SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR, ErrorCategory::CONTROL_FLOW,
                                                          "SYN003", "Unclosed test expression in '" + first_token + "' condition", line,
                                                          "Close the '[' with ']' or use '[[ ... ]]'"));
                    }

                    if (loop_check.missing_do_keyword) {
                        line_errors.push_back(SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR, ErrorCategory::CONTROL_FLOW,
                                                          "SYN002", "'" + first_token + "' statement missing 'do' keyword", line,
                                                          "Add 'do' keyword: " + first_token + " condition; do"));
                    }
                }
            }

            return line_errors;
        });
}

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::validate_conditional_syntax(
    const std::vector<std::string>& lines) {
    return process_lines_for_validation(
        lines, [](const std::string& line, const std::string& trimmed_line, size_t display_line, size_t) -> std::vector<SyntaxError> {
            std::vector<SyntaxError> line_errors;

            auto tokens = tokenize_whitespace(trimmed_line);

            if (!tokens.empty()) {
                const std::string& first_token = tokens[0];

                if (first_token == "if") {
                    auto if_check = analyze_if_syntax(tokens, trimmed_line);
                    if (if_check.missing_then_keyword) {
                        line_errors.push_back(SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR, ErrorCategory::CONTROL_FLOW,
                                                          "SYN004", "'if' statement missing 'then' keyword", line,
                                                          "Add 'then' keyword: if condition; then"));
                    }

                    if (if_check.missing_condition) {
                        line_errors.push_back(SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR, ErrorCategory::CONTROL_FLOW,
                                                          "SYN004", "'if' statement missing condition", line,
                                                          "Add condition: if [ condition ]; then"));
                    }
                }

                else if (first_token == "case") {
                    auto case_check = analyze_case_syntax(tokens);
                    if (case_check.incomplete) {
                        line_errors.push_back(SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR, ErrorCategory::CONTROL_FLOW,
                                                          "SYN008", "'case' statement incomplete", line,
                                                          "Complete case statement: case variable in"));
                    } else if (case_check.missing_in_keyword) {
                        line_errors.push_back(SyntaxError({display_line, 0, 0, 0}, ErrorSeverity::ERROR, ErrorCategory::CONTROL_FLOW,
                                                          "SYN008", "'case' statement missing 'in' keyword", line,
                                                          "Add 'in' keyword: case variable in"));
                    }
                }
            }

            return line_errors;
        });
}

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::validate_array_syntax(const std::vector<std::string>& lines) {
    return process_lines_for_validation(
        lines, [](const std::string& line, const std::string&, size_t display_line, size_t) -> std::vector<SyntaxError> {
            std::vector<SyntaxError> line_errors;

            for_each_effective_char(
                line, false, true, [&](size_t i, char c, const QuoteState& state, size_t& next_index) -> IterationAction {
                    if (!state.in_quotes && c == '(' && i > 0) {
                        size_t var_end = i;
                        while (var_end > 0 && std::isspace(static_cast<unsigned char>(line[var_end - 1]))) {
                            var_end--;
                        }

                        if (var_end > 0 && line[var_end - 1] == '=') {
                            size_t paren_count = 1;
                            size_t j = i + 1;
                            QuoteState nested_state;

                            while (j < line.length() && paren_count > 0) {
                                char inner_char = line[j];

                                if (!should_process_char(nested_state, inner_char, false)) {
                                    ++j;
                                    continue;
                                }

                                if (!nested_state.in_quotes) {
                                    if (inner_char == '(') {
                                        paren_count++;
                                    } else if (inner_char == ')') {
                                        paren_count--;
                                    }
                                }
                                ++j;
                            }

                            if (paren_count > 0) {
                                line_errors.push_back(SyntaxError({display_line, i, j, 0}, ErrorSeverity::ERROR, ErrorCategory::SYNTAX,
                                                                  "SYN009", "Unclosed array declaration - missing ')'", line,
                                                                  "Add closing parenthesis"));
                            }

                            if (j > 0) {
                                next_index = j - 1;
                            }
                        }
                    }

                    return IterationAction::Continue;
                });

            return line_errors;
        });
}

std::vector<ShellScriptInterpreter::SyntaxError> ShellScriptInterpreter::validate_heredoc_syntax(const std::vector<std::string>& lines) {
    std::vector<SyntaxError> errors;

    std::vector<std::pair<std::string, size_t>> heredoc_stack;

    for (size_t line_num = 0; line_num < lines.size(); ++line_num) {
        const std::string& line = lines[line_num];
        size_t display_line = line_num + 1;

        if (!heredoc_stack.empty()) {
            std::string trimmed_line = line;
            trimmed_line.erase(0, trimmed_line.find_first_not_of(" \t"));
            trimmed_line.erase(trimmed_line.find_last_not_of(" \t\r\n") + 1);

            if (trimmed_line == heredoc_stack.back().first) {
                heredoc_stack.pop_back();
                continue;
            }
        }

        size_t heredoc_pos = line.find("<<");
        if (heredoc_pos != std::string::npos) {
            bool in_quotes = false;
            char quote_char = '\0';

            for (size_t i = 0; i < heredoc_pos; ++i) {
                if ((line[i] == '"' || line[i] == '\'') && !in_quotes) {
                    in_quotes = true;
                    quote_char = line[i];
                } else if (line[i] == quote_char && in_quotes) {
                    in_quotes = false;
                    quote_char = '\0';
                }
            }

            if (!in_quotes) {
                size_t delim_start = heredoc_pos + 2;
                while (delim_start < line.length() && std::isspace(line[delim_start])) {
                    delim_start++;
                }

                if (delim_start < line.length()) {
                    size_t delim_end = delim_start;
                    while (delim_end < line.length() && !std::isspace(line[delim_end]) && line[delim_end] != ';' &&
                           line[delim_end] != '&' && line[delim_end] != '|') {
                        delim_end++;
                    }

                    if (delim_start < delim_end) {
                        std::string delimiter = line.substr(delim_start, delim_end - delim_start);

                        if ((delimiter.front() == '"' && delimiter.back() == '"') ||
                            (delimiter.front() == '\'' && delimiter.back() == '\'')) {
                            delimiter = delimiter.substr(1, delimiter.length() - 2);
                        }

                        if (!heredoc_stack.empty()) {
                            errors.push_back(SyntaxError(
                                {display_line, heredoc_pos, delim_end, 0}, ErrorSeverity::WARNING, ErrorCategory::SYNTAX, "SYN011",
                                "Nested heredoc detected - may cause parsing "
                                "issues",
                                line, "Consider closing previous heredoc '" + heredoc_stack.back().first + "' before starting new one"));
                        }

                        heredoc_stack.push_back({delimiter, display_line});
                    }
                }
            }
        }
    }

    while (!heredoc_stack.empty()) {
        auto& unclosed = heredoc_stack.back();
        errors.push_back(SyntaxError({unclosed.second, 0, 0, 0}, ErrorSeverity::ERROR, ErrorCategory::SYNTAX, "SYN010",
                                     "Unclosed here document - missing '" + unclosed.first + "'", "",
                                     "Add closing delimiter: " + unclosed.first));
        heredoc_stack.pop_back();
    }

    return errors;
}

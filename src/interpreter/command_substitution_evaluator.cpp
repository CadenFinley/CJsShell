#include "command_substitution_evaluator.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <utility>

#include "cjsh_filesystem.h"

CommandSubstitutionEvaluator::CommandSubstitutionEvaluator(CommandExecutor executor)
    : command_executor_(std::move(executor)) {
}

std::string CommandSubstitutionEvaluator::capture_command_output(const std::string& command) {
    // The command_executor_ callback is responsible for executing the command
    // and capturing its output. We just call it and return the result.
    return command_executor_(command);
}

bool CommandSubstitutionEvaluator::find_matching_delimiter(const std::string& text, size_t start,
                                                           char open_c, char close_c,
                                                           size_t& end_out) {
    int depth = 1;
    bool local_in_q = false;
    char local_q = '\0';

    for (size_t j = start; j < text.size(); ++j) {
        char d = text[j];
        if ((d == '"' || d == '\'') && (j == start || text[j - 1] != '\\')) {
            if (!local_in_q) {
                local_in_q = true;
                local_q = d;
            } else if (local_q == d) {
                local_in_q = false;
                local_q = '\0';
            }
        } else if (!local_in_q) {
            if (d == open_c) {
                depth++;
            } else if (d == close_c) {
                depth--;
                if (depth == 0) {
                    end_out = j;
                    return true;
                }
            }
        }
    }
    return false;
}

std::string CommandSubstitutionEvaluator::escape_for_double_quotes(const std::string& content) {
    std::string result;
    result.reserve(content.size() + (content.size() / 10) + 1);

    for (char c : content) {
        if (c == '"' || c == '\\' || c == '$' || c == '`') {
            result += '\\';
        }
        result += c;
    }

    return result;
}

CommandSubstitutionEvaluator::ExpansionResult CommandSubstitutionEvaluator::expand_substitutions(
    const std::string& input) {
    ExpansionResult result;
    result.text.reserve(input.size());

    bool in_quotes = false;
    char q = '\0';
    bool escaped = false;

    auto append_substitution_result = [&](const std::string& content) {
        if (in_quotes && q == '"') {
            std::string esc = escape_for_double_quotes(content);
            result.text += NOENV_START;
            result.text += esc;
            result.text += NOENV_END;
            return;
        }
        result.text += SUBST_LITERAL_START;
        result.text += content;
        result.text += SUBST_LITERAL_END;
    };

    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];

        if (escaped) {
            result.text += '\\';
            result.text += c;
            escaped = false;
            continue;
        }

        if (c == '\\' && (!in_quotes || q != '\'')) {
            escaped = true;
            continue;
        }

        if ((c == '"' || c == '\'') && (!in_quotes)) {
            in_quotes = true;
            q = c;
            result.text += c;
            continue;
        }
        if (in_quotes && c == q) {
            in_quotes = false;
            q = '\0';
            result.text += c;
            continue;
        }

        // Handle substitutions (only if not in single quotes, or in double quotes)
        if (!in_quotes || q == '"') {
            // Handle $((arithmetic))
            if (c == '$' && i + 2 < input.size() && input[i + 1] == '(' && input[i + 2] == '(') {
                size_t arith_end = 0;
                if (find_matching_delimiter(input, i + 3, '(', ')', arith_end)) {
                    if (arith_end + 1 < input.size() && input[arith_end + 1] == ')') {
                        // This is arithmetic expansion - just pass it through
                        // The caller will handle arithmetic evaluation
                        for (size_t k = i; k <= arith_end + 1; ++k) {
                            result.text += input[k];
                        }
                        i = arith_end + 1;
                        continue;
                    }
                }
            }

            // Handle $(command)
            if (c == '$' && i + 1 < input.size() && input[i + 1] == '(' &&
                (i + 2 >= input.size() || input[i + 2] != '(')) {
                size_t cmd_end = 0;
                if (find_matching_delimiter(input, i + 2, '(', ')', cmd_end)) {
                    std::string cmd_content = input.substr(i + 2, cmd_end - i - 2);
                    std::string cmd_output = capture_command_output(cmd_content);
                    result.outputs.push_back(cmd_output);
                    append_substitution_result(cmd_output);
                    i = cmd_end;
                    continue;
                }
            }

            // Handle `command`
            if (c == '`') {
                size_t backtick_end = i + 1;
                bool found_close = false;
                bool bt_escaped = false;

                while (backtick_end < input.size()) {
                    if (bt_escaped) {
                        bt_escaped = false;
                        backtick_end++;
                        continue;
                    }
                    if (input[backtick_end] == '\\') {
                        bt_escaped = true;
                        backtick_end++;
                        continue;
                    }
                    if (input[backtick_end] == '`') {
                        found_close = true;
                        break;
                    }
                    backtick_end++;
                }

                if (found_close) {
                    std::string cmd_content = input.substr(i + 1, backtick_end - i - 1);
                    std::string cmd_output = capture_command_output(cmd_content);
                    result.outputs.push_back(cmd_output);
                    append_substitution_result(cmd_output);
                    i = backtick_end;
                    continue;
                }
            }

            // Handle ${parameter}
            if (c == '$' && i + 1 < input.size() && input[i + 1] == '{') {
                size_t brace_end = 0;
                if (find_matching_delimiter(input, i + 2, '{', '}', brace_end)) {
                    // Just pass through - parameter expansion is handled elsewhere
                    for (size_t k = i; k <= brace_end; ++k) {
                        result.text += input[k];
                    }
                    i = brace_end;
                    continue;
                }
            }
        }

        result.text += c;
    }

    return result;
}

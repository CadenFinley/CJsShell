#include "conditional_evaluator.h"

#include <algorithm>
#include <cctype>

#include "cjsh.h"
#include "parser.h"
#include "shell_script_interpreter_utils.h"

using shell_script_interpreter::detail::process_line_for_validation;
using shell_script_interpreter::detail::strip_inline_comment;
using shell_script_interpreter::detail::trim;

namespace conditional_evaluator {

int handle_if_block(const std::vector<std::string>& src_lines, size_t& idx,
                    const std::function<int(const std::vector<std::string>&)>& execute_block,
                    const std::function<int(const std::string&)>& execute_simple_or_pipeline,
                    const std::function<int(const std::string&)>& evaluate_logical_condition,
                    Parser* shell_parser) {
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

                if (!in_quotes && (c == '"' || c == '\'' || c == '`')) {
                    in_quotes = true;
                    quote_char = c;
                } else if (in_quotes && c == quote_char) {
                    in_quotes = false;
                    quote_char = '\0';
                } else if (!in_quotes) {
                    if (search_pos + 3 < rem.length() && rem.substr(search_pos, 3) == "if ") {
                        if_depth++;
                        search_pos += 2;
                    }

                    else if (search_pos + 2 <= rem.length() && rem.substr(search_pos, 2) == "fi") {
                        bool is_word_start =
                            (search_pos == 0 || !std::isalnum(rem[search_pos - 1]));
                        bool is_word_end =
                            (search_pos + 2 >= rem.length() || !std::isalnum(rem[search_pos + 2]));
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

            if (rem.find("elif") != std::string::npos) {
            } else {
                if (fi_pos != std::string::npos) {
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

                        if (!in_quotes && (c == '"' || c == '\'' || c == '`')) {
                            in_quotes = true;
                            quote_char = c;
                        } else if (in_quotes && c == quote_char) {
                            in_quotes = false;
                            quote_char = '\0';
                        } else if (!in_quotes) {
                            if (search_else + 3 < body.length() &&
                                body.substr(search_else, 3) == "if ") {
                                nested_if_depth++;
                                search_else += 2;
                            }

                            else if (search_else + 2 <= body.length() &&
                                     body.substr(search_else, 2) == "fi") {
                                bool is_word_start =
                                    (search_else == 0 || !std::isalnum(body[search_else - 1]));
                                bool is_word_end = (search_else + 2 >= body.length() ||
                                                    !std::isalnum(body[search_else + 2]));
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

                    // Check if there are commands after 'fi' on the same line
                    // But only execute them if we didn't hit break/continue/return/exit
                    if (body_rc != 253 && body_rc != 254 && body_rc != 255 && !g_exit_flag) {
                        size_t after_fi_pos = fi_pos + 2;  // Position after "fi"
                        while (after_fi_pos < rem.length() &&
                               std::isspace(static_cast<unsigned char>(rem[after_fi_pos]))) {
                            after_fi_pos++;
                        }
                        if (after_fi_pos < rem.length() && rem[after_fi_pos] == ';') {
                            after_fi_pos++;  // Skip the semicolon
                            while (after_fi_pos < rem.length() &&
                                   std::isspace(static_cast<unsigned char>(rem[after_fi_pos]))) {
                                after_fi_pos++;
                            }
                        }
                        if (after_fi_pos < rem.length()) {
                            std::string after_commands = trim(rem.substr(after_fi_pos));
                            if (!after_commands.empty()) {
                                // Execute commands that come after the 'fi'
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
            bool is_word = (pos == 0 || !std::isalnum(line[pos - 1])) &&
                           (pos + 2 >= line.length() || !std::isalnum(line[pos + 2]));
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
                size_t else_pos = remaining.find("; else ", pos);

                size_t fi_pos = std::string::npos;
                size_t search_pos = pos;
                while (search_pos < remaining.length()) {
                    size_t candidate = remaining.find("fi", search_pos);
                    if (candidate == std::string::npos)
                        break;

                    bool is_word_end = (candidate + 2 >= remaining.length()) ||
                                       (remaining[candidate + 2] == ' ') ||
                                       (remaining[candidate + 2] == ';');
                    bool is_word_start = (candidate == 0) || (remaining[candidate - 1] == ' ') ||
                                         (remaining[candidate - 1] == ';');

                    if (is_word_start && is_word_end) {
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
                            condition_met = true;
                            idx = 0;
                            return 0;
                        }
                    }

                    pos = next_pos + 7;
                    size_t elif_then = remaining.find("; then", pos);
                    if (elif_then != std::string::npos) {
                        std::string elif_cond = trim(remaining.substr(pos, elif_then - pos));
                        int elif_result = execute_simple_or_pipeline(elif_cond);

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

                                bool is_word_end = (candidate + 2 >= remaining.length()) ||
                                                   (remaining[candidate + 2] == ' ') ||
                                                   (remaining[candidate + 2] == ';');
                                bool is_word_start = (candidate == 0) ||
                                                     (remaining[candidate - 1] == ' ') ||
                                                     (remaining[candidate - 1] == ';');

                                if (is_word_start && is_word_end) {
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

    // Track elif branches
    std::vector<std::pair<std::vector<std::string>, std::vector<std::string>>>
        elif_branches;  // condition lines, body lines
    std::vector<std::string> current_elif_cond;
    std::vector<std::string> current_elif_body;
    bool in_elif = false;
    bool in_elif_body = false;
    bool condition_met = (cond_rc == 0);

    while (k < src_lines.size() && depth > 0) {
        const std::string& cur_raw = src_lines[k];
        std::string cur = trim(strip_inline_comment(cur_raw));

        // Check for elif BEFORE checking for '; then' since elif lines may contain '; then'
        if (depth == 1 && (cur == "elif" || cur.rfind("elif ", 0) == 0)) {
            // Starting a new elif - save previous branch if we were in one
            if (in_elif_body && !current_elif_cond.empty()) {
                elif_branches.push_back({current_elif_cond, current_elif_body});
            }

            in_elif = true;
            in_elif_body = false;
            in_else = false;
            current_elif_cond.clear();
            current_elif_body.clear();

            // Extract condition from this line if present
            std::string elif_cond;
            if (cur.rfind("elif ", 0) == 0) {
                elif_cond = trim(cur.substr(5));
            }

            // Check if 'then' is on the same line
            auto then_pos = elif_cond.find("; then");
            if (then_pos == std::string::npos) {
                then_pos = elif_cond.find(";then");
            }

            if (then_pos != std::string::npos) {
                // then is on same line
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
                // Found 'then' for current elif
                in_elif = false;
                in_elif_body = true;

                // Add the last line of condition (the one with '; then')
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
            // Save current elif if we were in one
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
                // Accumulating elif condition
                current_elif_cond.push_back(cur_raw);
            } else if (in_elif_body) {
                // In elif body
                current_elif_body.push_back(cur_raw);
            } else if (!in_else) {
                then_lines.push_back(cur_raw);
            } else {
                else_lines.push_back(cur_raw);
            }
        }
        k++;
    }

    // Save last elif if we were in one
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
        condition_met = true;
    } else {
        // Try elif branches
        for (const auto& elif_branch : elif_branches) {
            // Build condition string from lines
            std::string elif_cond_str;
            for (const auto& line : elif_branch.first) {
                if (!elif_cond_str.empty()) {
                    elif_cond_str += " ";
                }
                elif_cond_str += trim(strip_inline_comment(line));
            }

            int elif_rc = evaluate_logical_condition(elif_cond_str);
            if (elif_rc == 0) {
                body_rc = execute_block(elif_branch.second);
                condition_met = true;
                break;
            }
        }

        // If no condition met, try else
        if (!condition_met && !else_lines.empty()) {
            body_rc = execute_block(else_lines);
        }
    }

    idx = k;
    return body_rc;
}

}  // namespace conditional_evaluator

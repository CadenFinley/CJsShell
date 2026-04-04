/*
  loop_evaluator.cpp

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

#include "loop_evaluator.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include "cjsh.h"
#include "error_out.h"
#include "exec.h"
#include "interpreter_utils.h"
#include "parser.h"
#include "parser_utils.h"
#include "shell.h"
#include "shell_env.h"
#include "signal_handler.h"
#include "token_constants.h"

using shell_script_interpreter::detail::strip_inline_comment;
using shell_script_interpreter::detail::trim;

namespace loop_evaluator {

namespace {

constexpr size_t kInlineLoopCacheLimit = 64;

thread_local std::unordered_map<std::string, std::shared_ptr<std::vector<std::string>>>
    g_inline_loop_cache;

std::string extract_loop_keyword(const std::string& segment) {
    std::string trimmed = trim(strip_inline_comment(segment));
    if (trimmed.empty()) {
        return "loop";
    }

    size_t end = trimmed.find_first_of(" \t;");
    if (end == std::string::npos) {
        return trimmed;
    }
    return trimmed.substr(0, end);
}

int report_inline_loop_syntax_error(const std::string& segment, std::string_view missing_token) {
    std::string keyword = extract_loop_keyword(segment);
    std::string message =
        "expected '" + std::string(missing_token) + "' to complete the " + keyword + " loop";
    std::vector<std::string> suggestions = {"Insert '" + std::string(missing_token) +
                                            "' between the loop header and body (e.g. '" + keyword +
                                            " ...; do ...; done')."};
    print_error({ErrorType::SYNTAX_ERROR, ErrorSeverity::ERROR, keyword, message, suggestions});
    return 2;
}

const std::shared_ptr<std::vector<std::string>>& get_cached_inline_loop_body(
    const std::string& body, Parser* shell_parser) {
    static const std::shared_ptr<std::vector<std::string>> kEmptyBody =
        std::make_shared<std::vector<std::string>>();

    if (body.empty() || shell_parser == nullptr) {
        return kEmptyBody;
    }

    auto cache_it = g_inline_loop_cache.find(body);
    if (cache_it != g_inline_loop_cache.end()) {
        return cache_it->second;
    }

    auto parsed_lines = shell_parser->parse_into_lines(body);
    auto parsed_ptr = std::make_shared<std::vector<std::string>>(std::move(parsed_lines));

    if (g_inline_loop_cache.size() >= kInlineLoopCacheLimit) {
        for (auto it = g_inline_loop_cache.begin(); it != g_inline_loop_cache.end(); ++it) {
            if (it->second.use_count() == 1) {
                g_inline_loop_cache.erase(it);
                break;
            }
        }
        if (g_inline_loop_cache.size() >= kInlineLoopCacheLimit) {
            g_inline_loop_cache.clear();
        }
    }

    auto [insert_it, _] = g_inline_loop_cache.emplace(body, std::move(parsed_ptr));
    return insert_it->second;
}

bool check_loop_interrupt(int& rc) {
    if (!g_shell) {
        return false;
    }

    if (!SignalHandler::has_pending_signals()) {
        return false;
    }

    SignalProcessingResult pending = g_shell->process_pending_signals();
    int exit_code = shell_script_interpreter::detail::pending_signal_exit_code(pending);
    if (exit_code >= 0) {
        rc = exit_code;
        return true;
    }
    return false;
}

int adjust_loop_signal(const char* env_name, int consumed_rc, int propagate_rc) {
    int level = 1;
    if (cjsh_env::shell_variable_is_set(env_name)) {
        std::string level_str = cjsh_env::get_shell_variable_value(env_name);
        try {
            level = std::stoi(level_str);
        } catch (...) {
            level = 1;
        }
        cjsh_env::unset_shell_variable_value(env_name);
    }
    if (level > 1) {
        std::string next_level = std::to_string(level - 1);
        cjsh_env::set_shell_variable_value(env_name, next_level);
        return propagate_rc;
    }
    return consumed_rc;
}

bool matches_keyword_only(const std::string& text, std::string_view keyword) {
    if (text.size() < keyword.size()) {
        return false;
    }
    if (text.compare(0, keyword.size(), keyword) != 0) {
        return false;
    }
    size_t pos = keyword.size();
    while (pos < text.size() && (std::isspace(static_cast<unsigned char>(text[pos])) != 0)) {
        pos++;
    }
    while (pos < text.size() && text[pos] == ';') {
        pos++;
        while (pos < text.size() && (std::isspace(static_cast<unsigned char>(text[pos])) != 0)) {
            pos++;
        }
    }
    while (pos < text.size() && (std::isspace(static_cast<unsigned char>(text[pos])) != 0)) {
        pos++;
    }
    if (pos >= text.size()) {
        return true;
    }

    auto is_redirection_start = [&](size_t start) {
        if (start >= text.size()) {
            return false;
        }
        char ch = text[start];
        if (ch == '<' || ch == '>') {
            return true;
        }
        if ((ch == '&') && start + 1 < text.size()) {
            char next = text[start + 1];
            return next == '>' || next == '<';
        }
        return false;
    };

    if (is_redirection_start(pos)) {
        return true;
    }

    if (std::isdigit(static_cast<unsigned char>(text[pos]))) {
        size_t digit_pos = pos;
        while (digit_pos < text.size() &&
               std::isdigit(static_cast<unsigned char>(text[digit_pos])) != 0) {
            digit_pos++;
        }
        return is_redirection_start(digit_pos);
    }

    return false;
}

bool starts_with_loop_keyword(const std::string& text) {
    const auto& loop_keywords = token_constants::loop_keywords();
    return std::any_of(loop_keywords.begin(), loop_keywords.end(), [&](const std::string& keyword) {
        if (text.size() < keyword.size()) {
            return false;
        }
        if (text.compare(0, keyword.size(), keyword) != 0) {
            return false;
        }
        if (text.size() == keyword.size()) {
            return true;
        }
        char next = text[keyword.size()];
        return (std::isspace(static_cast<unsigned char>(next)) != 0) || next == ';' || next == '(';
    });
}

bool collect_loop_body_lines(const std::vector<std::string>& src_lines, size_t start_index,
                             int initial_depth, std::vector<std::string>& body_lines,
                             size_t& next_index) {
    size_t k = start_index;
    int depth = initial_depth;

    if (depth <= 0) {
        next_index = k;
        return depth == 0;
    }

    while (k < src_lines.size() && depth > 0) {
        const std::string& cur_raw = src_lines[k];
        std::string cur = trim(strip_inline_comment(cur_raw));
        if (starts_with_loop_keyword(cur)) {
            ++depth;
        } else if (matches_keyword_only(cur, "done")) {
            --depth;
            if (depth == 0)
                break;
        }
        if (depth > 0)
            body_lines.push_back(cur_raw);
        ++k;
    }

    next_index = k;
    return depth == 0;
}

int iterate_numeric_range(int start, int end, bool is_ascending,
                          const std::function<LoopCommandOutcome(int)>& run_iteration) {
    int step = is_ascending ? 1 : -1;
    int value = start;
    int rc_local = 0;
    while (step > 0 ? value <= end : value >= end) {
        int signal_rc = 0;
        if (check_loop_interrupt(signal_rc)) {
            rc_local = signal_rc;
            break;
        }
        LoopCommandOutcome outcome = run_iteration(value);
        rc_local = outcome.code;
        if (outcome.flow == LoopFlow::NONE || outcome.flow == LoopFlow::CONTINUE) {
            value += step;
            continue;
        }
        break;
    }
    return rc_local;
}

struct CStyleForHeader {
    bool is_c_style = false;
    std::string init_expression;
    std::string condition_expression;
    std::string update_expression;
};

bool split_c_style_for_components(const std::string& text, std::array<std::string, 3>& parts) {
    size_t part_index = 0;
    std::string current;
    int paren_depth = 0;
    bool in_single = false;
    bool in_double = false;
    bool escaped = false;

    for (size_t i = 0; i < text.size(); ++i) {
        char ch = text[i];

        if (escaped) {
            current += ch;
            escaped = false;
            continue;
        }

        if (ch == '\\' && !in_single) {
            current += ch;
            escaped = true;
            continue;
        }

        if (!in_double && ch == '\'') {
            in_single = !in_single;
            current += ch;
            continue;
        }

        if (!in_single && ch == '"') {
            in_double = !in_double;
            current += ch;
            continue;
        }

        if (in_single || in_double) {
            current += ch;
            continue;
        }

        if (ch == '(') {
            ++paren_depth;
            current += ch;
            continue;
        }

        if (ch == ')') {
            if (paren_depth > 0) {
                --paren_depth;
            }
            current += ch;
            continue;
        }

        if (ch == ';' && paren_depth == 0) {
            if (part_index >= 2) {
                return false;
            }
            parts[part_index++] = trim(current);
            current.clear();
            continue;
        }

        current += ch;
    }

    if (part_index != 2) {
        return false;
    }

    parts[2] = trim(current);
    return true;
}

bool parse_c_style_for_header(const std::string& header, CStyleForHeader& out) {
    out = CStyleForHeader{};
    std::string normalized = trim(header);
    if (normalized.rfind("for", 0) != 0) {
        return false;
    }

    size_t pos = 3;
    while (pos < normalized.size() &&
           (std::isspace(static_cast<unsigned char>(normalized[pos])) != 0)) {
        ++pos;
    }

    if (pos + 1 >= normalized.size() || normalized.compare(pos, 2, "((") != 0) {
        return false;
    }

    size_t content_start = pos + 2;
    size_t i = content_start;
    int depth = 1;
    bool in_single = false;
    bool in_double = false;
    bool escaped = false;
    size_t content_end = std::string::npos;

    while (i < normalized.size()) {
        char ch = normalized[i];

        if (escaped) {
            escaped = false;
            ++i;
            continue;
        }

        if (ch == '\\' && !in_single) {
            escaped = true;
            ++i;
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

        if (in_single || in_double) {
            ++i;
            continue;
        }

        if (i + 1 < normalized.size() && normalized.compare(i, 2, "((") == 0) {
            ++depth;
            i += 2;
            continue;
        }

        if (i + 1 < normalized.size() && normalized.compare(i, 2, "))") == 0) {
            --depth;
            if (depth == 0) {
                content_end = i;
                i += 2;
                break;
            }
            i += 2;
            continue;
        }

        ++i;
    }

    if (depth != 0 || content_end == std::string::npos) {
        return false;
    }

    std::string trailing = trim(normalized.substr(i));
    if (!trailing.empty()) {
        return false;
    }

    std::array<std::string, 3> parts;
    std::string content = normalized.substr(content_start, content_end - content_start);
    if (!split_c_style_for_components(content, parts)) {
        return false;
    }

    out.is_c_style = true;
    out.init_expression = std::move(parts[0]);
    out.condition_expression = std::move(parts[1]);
    out.update_expression = std::move(parts[2]);
    return true;
}

bool contains_arithmetic_command_form(const std::string& text) {
    bool in_single = false;
    bool in_double = false;
    bool escaped = false;

    for (size_t i = 0; i + 1 < text.size(); ++i) {
        char ch = text[i];

        if (escaped) {
            escaped = false;
            continue;
        }

        if (!in_single && ch == '\\') {
            escaped = true;
            continue;
        }

        if (!in_double && ch == '\'') {
            in_single = !in_single;
            continue;
        }

        if (!in_single && ch == '"') {
            in_double = !in_double;
            continue;
        }

        if (in_single || in_double) {
            continue;
        }

        if (text.compare(i, 2, "((") == 0) {
            char previous = (i == 0) ? '\0' : text[i - 1];
            if (previous != '$') {
                return true;
            }
        }
    }

    return false;
}

std::pair<std::string, std::string> split_done_suffix(const std::string& suffix) {
    bool in_single = false;
    bool in_double = false;
    bool in_backtick = false;
    bool escaped = false;
    int paren_depth = 0;

    for (size_t i = 0; i < suffix.size(); ++i) {
        char ch = suffix[i];

        if (escaped) {
            escaped = false;
            continue;
        }

        if (!in_single && ch == '\\') {
            escaped = true;
            continue;
        }

        if (!in_double && !in_backtick && ch == '\'') {
            in_single = !in_single;
            continue;
        }

        if (!in_single && !in_backtick && ch == '"') {
            in_double = !in_double;
            continue;
        }

        if (!in_single && !in_double && ch == '`') {
            in_backtick = !in_backtick;
            continue;
        }

        if (in_single || in_double || in_backtick) {
            continue;
        }

        if (ch == '(') {
            ++paren_depth;
            continue;
        }

        if (ch == ')' && paren_depth > 0) {
            --paren_depth;
            continue;
        }

        if (ch == ';' && paren_depth == 0) {
            std::string redirections = trim(suffix.substr(0, i));
            std::string trailing_commands = trim(suffix.substr(i + 1));
            return {redirections, trailing_commands};
        }
    }

    return {trim(suffix), ""};
}

bool trailing_contains_block_closer_segment(const std::string& trailing_commands,
                                            Parser* shell_parser) {
    if (trailing_commands.empty()) {
        return false;
    }

    std::vector<std::string> segments;
    if (shell_parser != nullptr) {
        segments = shell_parser->parse_semicolon_commands(trailing_commands);
    }

    if (segments.empty()) {
        segments.push_back(trailing_commands);
    }

    for (const auto& segment : segments) {
        std::string trimmed_segment = trim(strip_inline_comment(segment));
        if (trimmed_segment.empty()) {
            continue;
        }

        if (matches_keyword_only(trimmed_segment, "done") ||
            matches_keyword_only(trimmed_segment, "fi") ||
            matches_keyword_only(trimmed_segment, "esac")) {
            return true;
        }
    }

    return false;
}

int handle_loop_block(const std::vector<std::string>& src_lines, size_t& idx,
                      const std::string& keyword, bool is_until,
                      const std::function<int(const std::vector<std::string>&)>& execute_block,
                      const std::function<int(const std::string&)>& execute_simple_or_pipeline,
                      Parser* shell_parser) {
    size_t loop_start_idx = idx;
    std::string first = trim(strip_inline_comment(src_lines[idx]));
    if (first != keyword && first.rfind(keyword + " ", 0) != 0)
        return 1;

    auto parse_cond_from = [&](const std::string& s, std::string& cond, bool& inline_do,
                               std::string& body_inline) {
        inline_do = false;
        body_inline.clear();
        cond.clear();
        std::string tmp = s;
        if (tmp == keyword) {
            return true;
        }
        if (tmp.rfind(keyword + " ", 0) == 0)
            tmp = tmp.substr(keyword.length() + 1);
        size_t do_pos = tmp.find("; do");
        if (do_pos != std::string::npos) {
            cond = trim(tmp.substr(0, do_pos));
            inline_do = true;
            body_inline = trim(tmp.substr(do_pos + 4));
            return true;
        }
        if (tmp == "do") {
            inline_do = true;
            return true;
        }
        if (tmp.rfind("do ", 0) == 0) {
            inline_do = true;
            body_inline = trim(tmp.substr(3));
            return true;
        }
        cond = trim(tmp);
        return true;
    };

    std::string cond;
    bool inline_do = false;
    std::string body_inline;
    parse_cond_from(first, cond, inline_do, body_inline);
    size_t j = idx;
    if (!inline_do) {
        while (++j < src_lines.size()) {
            const std::string& cur_raw = src_lines[j];
            std::string cur = trim(strip_inline_comment(cur_raw));
            if (cur == "do") {
                inline_do = true;
                break;
            }
            if (cur.rfind("do ", 0) == 0) {
                inline_do = true;
                body_inline = trim(cur.substr(3));
                break;
            }
            if (cur.find("; do") != std::string::npos) {
                inline_do = true;
                break;
            }
            if (!cur.empty()) {
                if (!cond.empty())
                    cond += " ";
                cond += cur;
            }
        }
    }
    if (!inline_do) {
        idx = j;
        return 1;
    }

    std::vector<std::string> body_lines;
    if (!body_inline.empty()) {
        std::string bi = body_inline;
        size_t done_pos = bi.rfind("; done");
        if (done_pos != std::string::npos)
            bi = trim(bi.substr(0, done_pos));
        body_lines = shell_parser->parse_into_lines(bi);
        idx = j;
    } else {
        size_t body_end_idx = 0;
        if (!collect_loop_body_lines(src_lines, j + 1, 1, body_lines, body_end_idx)) {
            idx = body_end_idx;
            return 1;
        }
        idx = body_end_idx;
    }

    auto run_loop_logic = [&]() -> int {
        int rc = 0;
        while (true) {
            if (check_loop_interrupt(rc)) {
                break;
            }

            int c = 0;
            if (!cond.empty()) {
                c = execute_simple_or_pipeline(cond);

                int signal_rc = 0;
                if (check_loop_interrupt(signal_rc)) {
                    rc = signal_rc;
                    break;
                }
            }

            bool continue_loop = is_until ? (c != 0) : (c == 0);
            if (!continue_loop)
                break;

            rc = execute_block(body_lines);
            auto outcome = handle_loop_command_result(rc, 0, 255, 0, 254, true);
            rc = outcome.code;
            if (outcome.flow == LoopFlow::BREAK)
                break;
            if (outcome.flow == LoopFlow::CONTINUE)
                continue;
        }
        return rc;
    };

    std::string done_redirections;
    std::string trailing_commands;
    if (idx < src_lines.size()) {
        std::string closing_trim = trim(strip_inline_comment(src_lines[idx]));
        size_t done_pos = parser_find_keyword_token(closing_trim, "done", 0);
        if (done_pos != std::string::npos) {
            std::string suffix = trim(closing_trim.substr(done_pos + 4));
            auto suffix_parts = split_done_suffix(suffix);
            done_redirections = suffix_parts.first;
            trailing_commands = suffix_parts.second;
        }
    }

    auto finalize_with_trailing_commands = [&](int loop_rc) {
        if (trailing_commands.empty()) {
            return loop_rc;
        }

        if (trailing_contains_block_closer_segment(trailing_commands, shell_parser)) {
            return loop_rc;
        }

        if (loop_rc == 253 || loop_rc == 254 || loop_rc == 255 || cjsh_env::exit_requested()) {
            return loop_rc;
        }

        return execute_simple_or_pipeline(trailing_commands);
    };

    auto command_has_redirections = [](const Command& cmd) {
        return !cmd.input_file.empty() || !cmd.here_doc.empty() || !cmd.here_string.empty() ||
               !cmd.output_file.empty() || !cmd.append_file.empty() || !cmd.stderr_file.empty() ||
               cmd.stderr_to_stdout || cmd.stdout_to_stderr || cmd.both_output ||
               !cmd.process_substitutions.empty() || !cmd.fd_redirections.empty() ||
               !cmd.fd_duplications.empty();
    };

    const bool condition_has_arithmetic_command_form = contains_arithmetic_command_form(cond);

    if (shell_parser && g_shell && g_shell->shell_exec) {
        Command control_cmd;
        control_cmd.args.push_back(keyword);

        if (!condition_has_arithmetic_command_form) {
            try {
                std::string loop_text;
                for (size_t line_idx = loop_start_idx;
                     line_idx <= idx && line_idx < src_lines.size(); ++line_idx) {
                    loop_text += src_lines[line_idx];
                    loop_text.push_back('\n');
                }

                std::vector<Command> loop_cmds =
                    shell_parser->parse_pipeline_with_preprocessing(loop_text);
                if (!loop_cmds.empty()) {
                    control_cmd = loop_cmds[0];
                }
            } catch (const std::exception&) {
                // Best-effort parse; skip redirection merge on failure.
            }
        }

        auto merge_redirections = [&](const Command& source) {
            if (control_cmd.input_file.empty() && !source.input_file.empty()) {
                control_cmd.input_file = source.input_file;
            }
            if (control_cmd.here_doc.empty() && !source.here_doc.empty()) {
                control_cmd.here_doc = source.here_doc;
            }
            if (control_cmd.here_string.empty() && !source.here_string.empty()) {
                control_cmd.here_string = source.here_string;
            }
            if (control_cmd.output_file.empty() && !source.output_file.empty()) {
                control_cmd.output_file = source.output_file;
                control_cmd.force_overwrite = source.force_overwrite;
            }
            if (control_cmd.append_file.empty() && !source.append_file.empty()) {
                control_cmd.append_file = source.append_file;
            }
            if (control_cmd.stderr_file.empty() && !source.stderr_file.empty()) {
                control_cmd.stderr_file = source.stderr_file;
                control_cmd.stderr_append = source.stderr_append;
            }
            if (!control_cmd.both_output && source.both_output) {
                control_cmd.both_output = true;
                control_cmd.both_output_file = source.both_output_file;
            }
            if (source.stderr_to_stdout) {
                control_cmd.stderr_to_stdout = true;
            }
            if (source.stdout_to_stderr) {
                control_cmd.stdout_to_stderr = true;
            }
            control_cmd.fd_redirections.insert(control_cmd.fd_redirections.end(),
                                               source.fd_redirections.begin(),
                                               source.fd_redirections.end());
            control_cmd.fd_duplications.insert(control_cmd.fd_duplications.end(),
                                               source.fd_duplications.begin(),
                                               source.fd_duplications.end());
            control_cmd.process_substitutions.insert(control_cmd.process_substitutions.end(),
                                                     source.process_substitutions.begin(),
                                                     source.process_substitutions.end());
        };

        if (!done_redirections.empty()) {
            std::string pseudo_command = "true " + done_redirections;
            try {
                auto pseudo_cmds = shell_parser->parse_pipeline_with_preprocessing(pseudo_command);
                if (!pseudo_cmds.empty()) {
                    merge_redirections(pseudo_cmds[0]);
                }
            } catch (const std::exception&) {
                // Best-effort parse; ignore redirection extraction on failure.
            }
        }

        if (command_has_redirections(control_cmd)) {
            bool action_invoked = false;
            int exit_code = g_shell->shell_exec->run_with_command_redirections(
                control_cmd, run_loop_logic, keyword, false, &action_invoked);
            return finalize_with_trailing_commands(exit_code);
        }
    }

    return finalize_with_trailing_commands(run_loop_logic());
}

}  // namespace

LoopCommandOutcome handle_loop_command_result(int rc, int break_consumed_rc, int break_propagate_rc,
                                              int continue_consumed_rc, int continue_propagate_rc,
                                              bool allow_error_continue) {
    if (rc == 255) {
        int adjusted =
            adjust_loop_signal("CJSH_BREAK_LEVEL", break_consumed_rc, break_propagate_rc);
        return {LoopFlow::BREAK, adjusted};
    }
    if (rc == 254) {
        int adjusted =
            adjust_loop_signal("CJSH_CONTINUE_LEVEL", continue_consumed_rc, continue_propagate_rc);
        if (adjusted == continue_consumed_rc)
            return {LoopFlow::CONTINUE, adjusted};
        return {LoopFlow::BREAK, adjusted};
    }
#ifdef SIGINT
    if (rc == 128 + SIGINT) {
        return {LoopFlow::BREAK, rc};
    }
#endif
#ifdef SIGTERM
    if (rc == 128 + SIGTERM) {
        return {LoopFlow::BREAK, rc};
    }
#endif
#ifdef SIGHUP
    if (rc == 128 + SIGHUP) {
        return {LoopFlow::BREAK, rc};
    }
#endif
    if (rc != 0) {
        if (g_shell && g_shell->should_abort_on_nonzero_exit())
            return {LoopFlow::BREAK, rc};
        if (!allow_error_continue)
            return {LoopFlow::BREAK, rc};
    }
    return {LoopFlow::NONE, rc};
}

int handle_for_block(
    const std::vector<std::string>& src_lines, size_t& idx,
    const std::function<int(const std::vector<std::string>&)>& execute_block,
    const std::function<long long(const std::string&)>& evaluate_arithmetic_expression,
    const std::function<int(const std::string&)>& execute_simple_or_pipeline,
    Parser* shell_parser) {
    std::string first = trim(strip_inline_comment(src_lines[idx]));
    if (first != "for" && first.rfind("for ", 0) != 0)
        return 1;

    std::string var;
    std::vector<std::string> items;
    CStyleForHeader c_style_header;

    struct RangeInfo {
        bool is_range = false;
        int start = 0;
        int end = 0;
        bool is_ascending = true;
    } range_info;

    auto finalize_with_trailing_commands = [&](int loop_rc, const std::string& trailing_commands) {
        if (trailing_commands.empty()) {
            return loop_rc;
        }

        if (trailing_contains_block_closer_segment(trailing_commands, shell_parser)) {
            return loop_rc;
        }

        if (loop_rc == 253 || loop_rc == 254 || loop_rc == 255 || cjsh_env::exit_requested()) {
            return loop_rc;
        }

        if (!execute_simple_or_pipeline) {
            return loop_rc;
        }

        return execute_simple_or_pipeline(trailing_commands);
    };

    auto assign_loop_variable = [&](const std::string& value) {
        if (g_shell) {
            cjsh_env::env_vars()[var] = value;
            if (shell_parser != nullptr) {
                shell_parser->expand_env_vars(var);
            }
        }
        cjsh_env::set_shell_variable_value(var, value);
    };

    auto parse_header = [&](const std::string& header) -> bool {
        std::string normalized_header = trim(header);
        while (!normalized_header.empty() && normalized_header.back() == ';') {
            normalized_header.pop_back();
            normalized_header = trim(normalized_header);
        }

        if (parse_c_style_for_header(normalized_header, c_style_header)) {
            return true;
        }

        c_style_header = CStyleForHeader{};

        std::vector<std::string> raw_toks;

        try {
            std::istringstream iss(normalized_header);
            std::string token;
            while (iss >> token) {
                raw_toks.push_back(token);
            }
        } catch (...) {
            return false;
        }

        size_t i = 0;
        if (i < raw_toks.size() && raw_toks[i] == "for")
            ++i;
        if (i >= raw_toks.size())
            return false;
        var = raw_toks[i++];

        if (i < raw_toks.size() && raw_toks[i] == "in") {
            ++i;

            if (i < raw_toks.size() && raw_toks[i].find('{') != std::string::npos &&
                raw_toks[i].find("..") != std::string::npos &&
                raw_toks[i].find('}') != std::string::npos) {
                const std::string& range_str = raw_toks[i];
                size_t start_brace = range_str.find('{');
                size_t end_brace = range_str.find('}');
                std::string range_content =
                    range_str.substr(start_brace + 1, end_brace - start_brace - 1);
                size_t dots_pos = range_content.find("..");
                if (dots_pos != std::string::npos) {
                    std::string start_str = range_content.substr(0, dots_pos);
                    std::string end_str = range_content.substr(dots_pos + 2);
                    try {
                        range_info.start = std::stoi(start_str);
                        range_info.end = std::stoi(end_str);
                        range_info.is_range = true;
                        range_info.is_ascending = range_info.start <= range_info.end;
                        return !var.empty();
                    } catch (...) {
                        return false;
                    }
                }
            }

            std::vector<std::string> toks = shell_parser->parse_command(normalized_header);
            i = 0;
            if (i < toks.size() && toks[i] == "for")
                ++i;
            if (i >= toks.size())
                return false;
            var = toks[i++];
            if (i < toks.size() && toks[i] == "in") {
                ++i;
                while (i < toks.size()) {
                    items.push_back(toks[i++]);
                }
            }
        }
        return !var.empty();
    };

    size_t do_pos = parser_find_inline_do_position(first);
    size_t done_pos_on_line = parser_find_keyword_token(first, "done", 0);
    if (do_pos != std::string::npos && done_pos_on_line != std::string::npos) {
        std::string header = trim(first.substr(0, do_pos));
        if (!parse_header(header))
            return 1;
        std::string tail = trim(first.substr(do_pos + 2));
        size_t done_pos = parser_find_keyword_token(tail, "done", 0);
        size_t search_from = (done_pos == std::string::npos) ? 0 : done_pos + 4;
        while (done_pos != std::string::npos) {
            size_t next = parser_find_keyword_token(tail, "done", search_from);
            if (next == std::string::npos) {
                break;
            }
            done_pos = next;
            search_from = done_pos + 4;
        }

        std::string body = done_pos == std::string::npos ? tail : trim(tail.substr(0, done_pos));
        std::string trailing_commands;
        if (done_pos != std::string::npos) {
            std::string done_suffix = trim(tail.substr(done_pos + 4));
            trailing_commands = split_done_suffix(done_suffix).second;
        }

        if (shell_parser == nullptr) {
            return 1;
        }

        auto body_lines_handle = get_cached_inline_loop_body(body, shell_parser);
        const auto* body_lines_ptr = body_lines_handle.get();

        if (body_lines_ptr == nullptr) {
            return 1;
        }

        auto run_cached_body = [&, body_lines_ptr]() -> LoopCommandOutcome {
            return handle_loop_command_result(execute_block(*body_lines_ptr), 0, 255, 0, 254, true);
        };

        int rc = 0;

        auto evaluate_arithmetic_or_fail = [&](const std::string& expression,
                                               long long& result_out) -> bool {
            if (!evaluate_arithmetic_expression) {
                print_error({ErrorType::RUNTIME_ERROR,
                             ErrorSeverity::ERROR,
                             "for",
                             "arithmetic evaluator not available for C-style for loop",
                             {"Use a regular 'for var in ...' loop or initialize arithmetic "
                              "support before executing this block."}});
                rc = 1;
                return false;
            }

            try {
                result_out = evaluate_arithmetic_expression(expression);
                return true;
            } catch (const std::exception& e) {
                print_error({ErrorType::RUNTIME_ERROR,
                             ErrorSeverity::ERROR,
                             "for",
                             std::string("failed to evaluate arithmetic expression: ") + e.what(),
                             {"Check C-style for-loop expressions for invalid operators or "
                              "division by zero."}});
                rc = 1;
                return false;
            }
        };

        if (c_style_header.is_c_style) {
            if (!c_style_header.init_expression.empty()) {
                long long ignored_result = 0;
                if (!evaluate_arithmetic_or_fail(c_style_header.init_expression, ignored_result)) {
                    return finalize_with_trailing_commands(rc, trailing_commands);
                }
            }

            while (true) {
                int signal_rc = 0;
                if (check_loop_interrupt(signal_rc)) {
                    rc = signal_rc;
                    break;
                }

                long long condition_value = 1;
                if (!c_style_header.condition_expression.empty()) {
                    if (!evaluate_arithmetic_or_fail(c_style_header.condition_expression,
                                                     condition_value)) {
                        return finalize_with_trailing_commands(rc, trailing_commands);
                    }
                }

                if (condition_value == 0) {
                    break;
                }

                auto outcome = run_cached_body();
                rc = outcome.code;

                if (outcome.flow == LoopFlow::BREAK) {
                    break;
                }

                if (!c_style_header.update_expression.empty()) {
                    long long ignored_result = 0;
                    if (!evaluate_arithmetic_or_fail(c_style_header.update_expression,
                                                     ignored_result)) {
                        return finalize_with_trailing_commands(rc, trailing_commands);
                    }
                }
            }
        } else if (range_info.is_range) {
            rc = iterate_numeric_range(range_info.start, range_info.end, range_info.is_ascending,
                                       [&](int value) -> LoopCommandOutcome {
                                           std::string val_str = std::to_string(value);
                                           assign_loop_variable(val_str);
                                           return run_cached_body();
                                       });
        } else {
            for (const auto& it : items) {
                int signal_rc = 0;
                if (check_loop_interrupt(signal_rc)) {
                    rc = signal_rc;
                    break;
                }
                assign_loop_variable(it);

                auto outcome = run_cached_body();
                rc = outcome.code;
                if (outcome.flow == LoopFlow::NONE || outcome.flow == LoopFlow::CONTINUE)
                    continue;
                break;
            }
        }
        return finalize_with_trailing_commands(rc, trailing_commands);
    }

    std::string header_accum = first;
    size_t j = idx;
    bool have_do = false;
    bool do_line_has_inline_body = false;
    std::string inline_body;

    if (first.find("; do") != std::string::npos && first.rfind("; do") == first.length() - 4) {
        have_do = true;
        header_accum = first.substr(0, first.length() - 4);
    } else {
        while (!have_do && ++j < src_lines.size()) {
            std::string cur = trim(strip_inline_comment(src_lines[j]));
            if (cur == "do") {
                have_do = true;
                break;
            }
            if (cur.empty()) {
                continue;
            }
            size_t inline_do_pos = cur.find("; do");
            if (inline_do_pos != std::string::npos) {
                have_do = true;
                if (!header_accum.empty())
                    header_accum += " ";
                header_accum += cur.substr(0, inline_do_pos);
                std::string after_do = trim(cur.substr(inline_do_pos + 4));
                if (!after_do.empty()) {
                    do_line_has_inline_body = true;
                    inline_body = after_do;
                }
                break;
            }
            if (cur.rfind("do ", 0) == 0) {
                have_do = true;
                do_line_has_inline_body = true;
                inline_body = trim(cur.substr(3));
                break;
            }
            if (!cur.empty()) {
                if (!header_accum.empty())
                    header_accum += " ";
                header_accum += cur;
            }
        }
    }
    if (!parse_header(header_accum)) {
        idx = j;
        return 1;
    }
    if (!have_do) {
        idx = j;
        return 1;
    }

    std::vector<std::string> body_lines;
    bool inline_consumes_done = false;
    if (do_line_has_inline_body) {
        std::string inline_content = trim(inline_body);
        if (!inline_content.empty()) {
            if (matches_keyword_only(inline_content, "done")) {
                inline_consumes_done = true;
            } else {
                size_t done_pos = inline_content.rfind("; done");
                if (done_pos != std::string::npos) {
                    inline_content = trim(inline_content.substr(0, done_pos));
                    inline_consumes_done = true;
                } else if (inline_content.rfind("done") == inline_content.length() - 4) {
                    inline_content = trim(inline_content.substr(0, inline_content.length() - 4));
                    inline_consumes_done = true;
                }
            }

            if (!inline_content.empty()) {
                auto temp_lines = shell_parser->parse_into_lines(inline_content);
                for (const auto& tl : temp_lines) {
                    std::string ts = trim(tl);
                    if (!ts.empty() && ts != "done")
                        body_lines.push_back(tl);
                }
            }
        }
    }

    size_t body_start_idx = inline_consumes_done ? j : (j + 1);
    size_t body_end_idx = 0;
    int initial_depth = inline_consumes_done ? 0 : 1;
    if (!collect_loop_body_lines(src_lines, body_start_idx, initial_depth, body_lines,
                                 body_end_idx)) {
        idx = body_end_idx;
        return 1;
    }

    std::string trailing_commands;
    if (body_end_idx < src_lines.size()) {
        std::string closing_trim = trim(strip_inline_comment(src_lines[body_end_idx]));
        size_t done_pos = parser_find_keyword_token(closing_trim, "done", 0);
        if (done_pos != std::string::npos) {
            std::string done_suffix = trim(closing_trim.substr(done_pos + 4));
            trailing_commands = split_done_suffix(done_suffix).second;
        }
    }

    int rc = 0;

    auto run_body_and_handle_result = [&]() -> LoopCommandOutcome {
        return handle_loop_command_result(execute_block(body_lines), 0, 255, 0, 254, true);
    };

    auto evaluate_arithmetic_or_fail = [&](const std::string& expression,
                                           long long& result_out) -> bool {
        if (!evaluate_arithmetic_expression) {
            print_error({ErrorType::RUNTIME_ERROR,
                         ErrorSeverity::ERROR,
                         "for",
                         "arithmetic evaluator not available for C-style for loop",
                         {"Use a regular 'for var in ...' loop or initialize arithmetic support "
                          "before executing this block."}});
            rc = 1;
            return false;
        }

        try {
            result_out = evaluate_arithmetic_expression(expression);
            return true;
        } catch (const std::exception& e) {
            print_error({ErrorType::RUNTIME_ERROR,
                         ErrorSeverity::ERROR,
                         "for",
                         std::string("failed to evaluate arithmetic expression: ") + e.what(),
                         {"Check C-style for-loop expressions for invalid operators or division "
                          "by zero."}});
            rc = 1;
            return false;
        }
    };

    if (c_style_header.is_c_style) {
        if (!c_style_header.init_expression.empty()) {
            long long ignored_result = 0;
            if (!evaluate_arithmetic_or_fail(c_style_header.init_expression, ignored_result)) {
                idx = body_end_idx;
                return finalize_with_trailing_commands(rc, trailing_commands);
            }
        }

        while (true) {
            int signal_rc = 0;
            if (check_loop_interrupt(signal_rc)) {
                rc = signal_rc;
                break;
            }

            long long condition_value = 1;
            if (!c_style_header.condition_expression.empty()) {
                if (!evaluate_arithmetic_or_fail(c_style_header.condition_expression,
                                                 condition_value)) {
                    idx = body_end_idx;
                    return finalize_with_trailing_commands(rc, trailing_commands);
                }
            }

            if (condition_value == 0) {
                break;
            }

            auto outcome = run_body_and_handle_result();
            rc = outcome.code;

            if (outcome.flow == LoopFlow::BREAK) {
                break;
            }

            if (!c_style_header.update_expression.empty()) {
                long long ignored_result = 0;
                if (!evaluate_arithmetic_or_fail(c_style_header.update_expression,
                                                 ignored_result)) {
                    idx = body_end_idx;
                    return finalize_with_trailing_commands(rc, trailing_commands);
                }
            }
        }
    } else if (range_info.is_range) {
        rc = iterate_numeric_range(range_info.start, range_info.end, range_info.is_ascending,
                                   [&](int value) -> LoopCommandOutcome {
                                       std::string val_str = std::to_string(value);
                                       assign_loop_variable(val_str);
                                       return run_body_and_handle_result();
                                   });
    } else {
        for (const auto& it : items) {
            int signal_rc = 0;
            if (check_loop_interrupt(signal_rc)) {
                rc = signal_rc;
                break;
            }
            assign_loop_variable(it);
            auto outcome = run_body_and_handle_result();
            rc = outcome.code;
            if (outcome.flow == LoopFlow::NONE || outcome.flow == LoopFlow::CONTINUE)
                continue;
            break;
        }
    }
    idx = body_end_idx;
    return finalize_with_trailing_commands(rc, trailing_commands);
}

int handle_condition_loop_block(
    LoopCondition condition, const std::vector<std::string>& src_lines, size_t& idx,
    const std::function<int(const std::vector<std::string>&)>& execute_block,
    const std::function<int(const std::string&)>& execute_simple_or_pipeline,
    Parser* shell_parser) {
    const char* keyword = condition == LoopCondition::WHILE ? "while" : "until";
    bool is_until = condition == LoopCondition::UNTIL;
    return handle_loop_block(src_lines, idx, keyword, is_until, execute_block,
                             execute_simple_or_pipeline, shell_parser);
}

std::optional<int> try_execute_inline_do_block(
    const std::string& first_segment, const std::vector<std::string>& segments,
    size_t& segment_index,
    const std::function<int(const std::vector<std::string>&, size_t&)>& handler) {
    if (first_segment.find("; do") != std::string::npos)
        return std::nullopt;

    size_t lookahead = segment_index + 1;
    if (lookahead >= segments.size())
        return std::optional<int>{report_inline_loop_syntax_error(first_segment, "do")};

    std::string next_segment = trim(strip_inline_comment(segments[lookahead]));
    if (next_segment != "do" && next_segment.rfind("do ", 0) != 0)
        return std::optional<int>{report_inline_loop_syntax_error(first_segment, "do")};

    std::string body = next_segment.size() > 3 && next_segment.rfind("do ", 0) == 0
                           ? trim(next_segment.substr(3))
                           : "";

    size_t scan = lookahead + 1;
    bool found_done = false;
    for (; scan < segments.size(); ++scan) {
        std::string seg = trim(strip_inline_comment(segments[scan]));
        if (seg == "done") {
            found_done = true;
            break;
        }
        if (!body.empty())
            body += "; ";
        body += seg;
    }

    if (!found_done)
        return std::optional<int>{report_inline_loop_syntax_error(first_segment, "done")};

    std::string combined = first_segment + "; do";
    if (!body.empty())
        combined += " " + body;
    combined += "; done";

    size_t local_idx = 0;
    std::vector<std::string> inline_lines{combined};
    int rc = handler(inline_lines, local_idx);
    segment_index = scan;
    return std::optional<int>{rc};
}

}  // namespace loop_evaluator

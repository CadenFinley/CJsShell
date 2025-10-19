#include "loop_evaluator.h"

#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>

#include "cjsh.h"
#include "parser.h"
#include "shell.h"
#include "shell_script_interpreter_utils.h"

using shell_script_interpreter::detail::strip_inline_comment;
using shell_script_interpreter::detail::trim;

namespace loop_evaluator {

namespace {

int adjust_loop_signal(const char* env_name, int consumed_rc, int propagate_rc) {
    int level = 1;
    if (const char* level_str = getenv(env_name)) {
        try {
            level = std::stoi(level_str);
        } catch (...) {
            level = 1;
        }
        unsetenv(env_name);
    }
    if (level > 1) {
        std::string next_level = std::to_string(level - 1);
        setenv(env_name, next_level.c_str(), 1);
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
    return pos == text.size();
}

int handle_loop_block(const std::vector<std::string>& src_lines, size_t& idx,
                      const std::string& keyword, bool is_until,
                      const std::function<int(const std::vector<std::string>&)>& execute_block,
                      const std::function<int(const std::string&)>& execute_simple_or_pipeline,
                      Parser* shell_parser) {
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
    } else {
        size_t k = j + 1;
        int depth = 1;
        while (k < src_lines.size() && depth > 0) {
            const std::string& cur_raw = src_lines[k];
            std::string cur = trim(strip_inline_comment(cur_raw));
            if (cur == keyword || cur.rfind(keyword + " ", 0) == 0) {
                depth++;
            } else if (matches_keyword_only(cur, "done")) {
                depth--;
                if (depth == 0)
                    break;
            }
            if (depth > 0)
                body_lines.push_back(cur_raw);
            k++;
        }
        if (depth != 0) {
            idx = k;
            return 1;
        }
        idx = k;
    }

    int rc = 0;
    while (true) {
        int c = 0;
        if (!cond.empty()) {
            c = execute_simple_or_pipeline(cond);
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
}

}  

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
    if (rc != 0) {
        if (g_shell && g_shell->is_errexit_enabled())
            return {LoopFlow::BREAK, rc};
        if (!allow_error_continue)
            return {LoopFlow::BREAK, rc};
    }
    return {LoopFlow::NONE, rc};
}

int handle_for_block(const std::vector<std::string>& src_lines, size_t& idx,
                     const std::function<int(const std::vector<std::string>&)>& execute_block,
                     Parser* shell_parser) {
    std::string first = trim(strip_inline_comment(src_lines[idx]));
    if (first != "for" && first.rfind("for ", 0) != 0)
        return 1;

    std::string var;
    std::vector<std::string> items;

    struct RangeInfo {
        bool is_range = false;
        int start = 0;
        int end = 0;
        bool is_ascending = true;
    } range_info;

    auto parse_header = [&](const std::string& header) -> bool {
        std::vector<std::string> raw_toks;

        try {
            std::istringstream iss(header);
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

            std::vector<std::string> toks = shell_parser->parse_command(header);
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

    if (first.find("; do") != std::string::npos && first.find("done") != std::string::npos) {
        size_t do_pos = first.find("; do");
        std::string header = trim(first.substr(0, do_pos));
        if (!parse_header(header))
            return 1;
        std::string tail = trim(first.substr(do_pos + 4));
        size_t done_pos = tail.rfind("; done");
        if (done_pos == std::string::npos)
            done_pos = tail.rfind("done");
        std::string body = done_pos == std::string::npos ? tail : trim(tail.substr(0, done_pos));
        int rc = 0;

        if (range_info.is_range) {
            auto execute_range_iteration = [&](int value) -> int {
                std::string val_str = std::to_string(value);
                if (g_shell) {
                    g_shell->get_env_vars()[var] = val_str;
                    if (shell_parser) {
                        shell_parser->expand_env_vars(var);
                    }
                }
                setenv(var.c_str(), val_str.c_str(), 1);

                std::vector<std::string> body_vec = shell_parser->parse_into_lines(body);
                int cmd_rc = execute_block(body_vec);
                auto outcome = handle_loop_command_result(cmd_rc, 0, 255, 0, 254, true);
                return outcome.code;
            };

            auto iterate_range = [&](int start, int end, int step) -> int {
                int value = start;
                int rc_local = 0;
                while (step > 0 ? value <= end : value >= end) {
                    rc_local = execute_range_iteration(value);
                    auto outcome = handle_loop_command_result(rc_local, 0, 255, 0, 254, true);
                    rc_local = outcome.code;
                    if (outcome.flow == LoopFlow::NONE) {
                        value += step;
                        continue;
                    }
                    if (outcome.flow == LoopFlow::CONTINUE) {
                        value += step;
                        continue;
                    }
                    break;
                }
                return rc_local;
            };

            rc = iterate_range(range_info.start, range_info.end, range_info.is_ascending ? 1 : -1);
        } else {
            for (const auto& it : items) {
                if (g_shell) {
                    g_shell->get_env_vars()[var] = it;
                    if (shell_parser != nullptr) {
                        shell_parser->expand_env_vars(var);
                    }
                }
                setenv(var.c_str(), it.c_str(), 1);

                std::vector<std::string> body_vec = shell_parser->parse_into_lines(body);
                rc = execute_block(body_vec);
                auto outcome = handle_loop_command_result(rc, 0, 255, 0, 254, true);
                rc = outcome.code;
                if (outcome.flow == LoopFlow::NONE)
                    continue;
                if (outcome.flow == LoopFlow::CONTINUE)
                    continue;
                break;
            }
        }
        return rc;
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

    size_t k = inline_consumes_done ? j : (j + 1);
    int depth = inline_consumes_done ? 0 : 1;
    while (k < src_lines.size() && depth > 0) {
        const std::string& cur_raw = src_lines[k];
        std::string cur = trim(strip_inline_comment(cur_raw));
        if (cur == "for" || cur.rfind("for ", 0) == 0) {
            depth++;
        } else if (matches_keyword_only(cur, "done")) {
            depth--;
            if (depth == 0)
                break;
        }
        if (depth > 0)
            body_lines.push_back(cur_raw);
        k++;
    }
    if (depth != 0) {
        idx = k;
        return 1;
    }

    int rc = 0;

    auto run_body_and_handle_result = [&]() -> LoopCommandOutcome {
        return handle_loop_command_result(execute_block(body_lines), 0, 255, 0, 254, true);
    };

    if (range_info.is_range) {
        auto iterate_range_body = [&](int start, int end, int step) -> int {
            int value = start;
            int rc_local = 0;
            while (step > 0 ? value <= end : value >= end) {
                if (g_shell) {
                    std::string val_str = std::to_string(value);
                    g_shell->get_env_vars()[var] = val_str;
                    if (shell_parser) {
                        shell_parser->expand_env_vars(var);
                    }
                    setenv(var.c_str(), val_str.c_str(), 1);
                }
                auto outcome = run_body_and_handle_result();
                rc_local = outcome.code;
                if (outcome.flow == LoopFlow::NONE) {
                    value += step;
                    continue;
                }
                if (outcome.flow == LoopFlow::CONTINUE) {
                    value += step;
                    continue;
                }
                break;
            }
            return rc_local;
        };

        rc = iterate_range_body(range_info.start, range_info.end, range_info.is_ascending ? 1 : -1);
    } else {
        for (const auto& it : items) {
            if (g_shell) {
                g_shell->get_env_vars()[var] = it;
                if (shell_parser != nullptr) {
                    shell_parser->expand_env_vars(var);
                }
                setenv(var.c_str(), it.c_str(), 1);
            }
            auto outcome = run_body_and_handle_result();
            rc = outcome.code;
            if (outcome.flow == LoopFlow::NONE)
                continue;
            if (outcome.flow == LoopFlow::CONTINUE)
                continue;
            break;
        }
    }
    idx = k;
    return rc;
}

int handle_while_block(const std::vector<std::string>& src_lines, size_t& idx,
                       const std::function<int(const std::vector<std::string>&)>& execute_block,
                       const std::function<int(const std::string&)>& execute_simple_or_pipeline,
                       Parser* shell_parser) {
    return handle_loop_block(src_lines, idx, "while", false, execute_block,
                             execute_simple_or_pipeline, shell_parser);
}

int handle_until_block(const std::vector<std::string>& src_lines, size_t& idx,
                       const std::function<int(const std::vector<std::string>&)>& execute_block,
                       const std::function<int(const std::string&)>& execute_simple_or_pipeline,
                       Parser* shell_parser) {
    return handle_loop_block(src_lines, idx, "until", true, execute_block,
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
        return std::optional<int>{1};

    std::string next_segment = trim(strip_inline_comment(segments[lookahead]));
    if (next_segment != "do" && next_segment.rfind("do ", 0) != 0)
        return std::optional<int>{1};

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
        return std::optional<int>{1};

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

}  

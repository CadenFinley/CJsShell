#include "parser.h"

#include <fcntl.h>
#include <glob.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string_view>

#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "command_preprocessor.h"
#include "delimiter_state.h"
#include "expansion_engine.h"
#include "job_control.h"
#include "parser_utils.h"
#include "quote_info.h"
#include "readonly_command.h"
#include "shell.h"
#include "shell_script_interpreter.h"
#include "tokenizer.h"
#include "variable_expander.h"

void Parser::set_shell(Shell* shell) {
    this->shell = shell;
    if (!tokenizer) {
        tokenizer = std::make_unique<Tokenizer>();
    }
    if (!variableExpander) {
        variableExpander = std::make_unique<VariableExpander>(shell, env_vars);
    }
    if (!expansionEngine) {
        expansionEngine = std::make_unique<ExpansionEngine>(shell);
    }
}

std::vector<std::string> Parser::parse_into_lines(const std::string& script) {
    auto strip_inline_comment = [](std::string_view s) -> std::string {
        bool in_quotes = false;
        char quote = '\0';
        const char* data = s.data();
        size_t size = s.size();

        for (size_t i = 0; i < size; ++i) {
            char c = data[i];

            if (c == '"' || c == '\'') {
                if (!is_char_escaped(data, i)) {
                    if (!in_quotes) {
                        in_quotes = true;
                        quote = c;
                    } else if (quote == c) {
                        in_quotes = false;
                        quote = '\0';
                    }
                }
            } else if (!in_quotes && c == '#') {
                return std::string(s.substr(0, i));
            }
        }
        return std::string(s);
    };

    std::vector<std::string> lines;
    lines.reserve(4);
    size_t start = 0;
    bool in_quotes = false;
    char quote_char = '\0';
    bool in_here_doc = false;
    bool strip_tabs = false;
    bool here_doc_expand = true;
    size_t here_doc_operator_pos = std::string::npos;
    size_t here_doc_operator_len = 0;
    size_t here_doc_delim_end_pos = std::string::npos;
    std::string here_doc_delimiter;
    here_doc_delimiter.reserve(32);
    std::string here_doc_content;
    here_doc_content.reserve(256);
    std::string current_here_doc_line;
    current_here_doc_line.reserve(128);

    auto add_here_doc_placeholder_line = [&](std::string before, const std::string& rest) {
        std::string placeholder = "HEREDOC_PLACEHOLDER_" + std::to_string(lines.size());

        std::string stored_content = here_doc_content;
        if (here_doc_expand) {
            stored_content = "__EXPAND__" + stored_content;
        }
        current_here_docs[placeholder] = std::move(stored_content);

        std::string segment = std::move(before);
        segment += "< ";
        segment += placeholder;
        segment += rest;

        if (!segment.empty() && segment.back() == '\r') {
            segment.pop_back();
        }
        lines.push_back(std::move(segment));
    };

    auto locate_here_operator = [&](std::string_view sv, size_t& pos_out,
                                    size_t& delim_end_out) -> bool {
        size_t pos = sv.find("<<");
        while (pos != std::string::npos) {
            size_t op_len = 2;
            if (pos + 2 < sv.size() && sv[pos + 2] == '-') {
                op_len = 3;
            }

            size_t delim_start = pos + op_len;
            while (delim_start < sv.size() &&
                   (std::isspace(static_cast<unsigned char>(sv[delim_start])) != 0)) {
                delim_start++;
            }

            size_t delim_end = delim_start;
            while (delim_end < sv.size() &&
                   (std::isspace(static_cast<unsigned char>(sv[delim_end])) == 0)) {
                delim_end++;
            }

            if (delim_start == delim_end) {
                pos = sv.find("<<", pos + 2);
                continue;
            }

            std::string_view candidate = sv.substr(delim_start, delim_end - delim_start);
            std::string_view unquoted = candidate;
            if (candidate.size() >= 2 &&
                ((candidate.front() == '"' && candidate.back() == '"') ||
                 (candidate.front() == '\'' && candidate.back() == '\''))) {
                unquoted = candidate.substr(1, candidate.size() - 2);
            }

            if (unquoted == here_doc_delimiter) {
                pos_out = pos;
                delim_end_out = delim_end;
                return true;
            }

            pos = sv.find("<<", pos + 2);
        }
        return false;
    };

    auto process_here_doc_segment = [&](std::string_view segment_view) {
        bool segment_created = false;

        auto append_placeholder_from_offsets = [&](size_t delim_end_in_segment) {
            if (here_doc_operator_pos > segment_view.size()) {
                return;
            }
            size_t effective_delim_end = std::min(delim_end_in_segment, segment_view.size());

            std::string before_here{segment_view.substr(0, here_doc_operator_pos)};
            size_t line_end = segment_view.find('\n', effective_delim_end);
            std::string rest_of_line;
            if (line_end != std::string::npos) {
                rest_of_line.assign(
                    segment_view.substr(effective_delim_end, line_end - effective_delim_end));
            } else {
                rest_of_line.assign(segment_view.substr(effective_delim_end));
            }

            add_here_doc_placeholder_line(std::move(before_here), rest_of_line);
            segment_created = true;
        };

        if (!segment_created && here_doc_operator_pos != std::string::npos &&
            here_doc_operator_len > 0) {
            append_placeholder_from_offsets(here_doc_delim_end_pos);
        }

        if (!segment_created) {
            size_t match_pos = std::string::npos;
            size_t delim_end = std::string::npos;
            if (locate_here_operator(segment_view, match_pos, delim_end)) {
                std::string before_here{segment_view.substr(0, match_pos)};
                size_t line_end = segment_view.find('\n', delim_end);
                std::string rest_of_line;
                if (line_end != std::string::npos) {
                    rest_of_line.assign(segment_view.substr(delim_end, line_end - delim_end));
                } else {
                    rest_of_line.assign(segment_view.substr(delim_end));
                }

                add_here_doc_placeholder_line(std::move(before_here), rest_of_line);
                segment_created = true;
            }
        }

        if (!segment_created) {
            std::string segment{segment_view};
            if (!segment.empty() && segment.back() == '\r') {
                segment.pop_back();
            }
            lines.push_back(std::move(segment));
        }
    };

    auto reset_here_doc_state = [&]() {
        here_doc_operator_pos = std::string::npos;
        here_doc_operator_len = 0;
        here_doc_delim_end_pos = std::string::npos;
        in_here_doc = false;
        strip_tabs = false;
        here_doc_expand = true;
        here_doc_delimiter.clear();
        here_doc_content.clear();
        current_here_doc_line.clear();
    };

    for (size_t i = 0; i < script.size(); ++i) {
        char c = script[i];

        if (in_here_doc) {
            if (c == '\n') {
                std::string trimmed_line = current_here_doc_line;

                trimmed_line.erase(0, trimmed_line.find_first_not_of(" \t"));
                trimmed_line.erase(trimmed_line.find_last_not_of(" \t\r") + 1);

                if (trimmed_line == here_doc_delimiter) {
                    std::string_view segment_view{script.data() + start, i - start};
                    process_here_doc_segment(segment_view);
                    reset_here_doc_state();
                    start = i + 1;
                } else {
                    if (!here_doc_content.empty()) {
                        here_doc_content += "\n";
                    }

                    std::string line_to_add = current_here_doc_line;
                    if (strip_tabs) {
                        size_t first_non_tab = line_to_add.find_first_not_of('\t');
                        if (first_non_tab != std::string::npos) {
                            line_to_add = line_to_add.substr(first_non_tab);
                        } else {
                            line_to_add.clear();
                        }
                    }
                    here_doc_content += line_to_add;
                }
                current_here_doc_line.clear();
            } else {
                current_here_doc_line += c;
            }
            continue;
        }

        if (!in_quotes && (c == '"' || c == '\'')) {
            in_quotes = true;
            quote_char = c;
        } else if (in_quotes && c == quote_char) {
            in_quotes = false;
        } else if (!in_quotes && c == '\n') {
            std::string_view segment_view{script.data() + start, i - start};
            if (!in_quotes && segment_view.find("<<") != std::string_view::npos) {
                std::string segment_no_comment = strip_inline_comment(segment_view);

                size_t here_pos = segment_no_comment.find("<<-");
                bool is_strip_tabs = false;
                size_t operator_len = 2;

                if (here_pos != std::string::npos) {
                    is_strip_tabs = true;
                    operator_len = 3;
                } else {
                    here_pos = segment_no_comment.find("<<");
                    if (here_pos == std::string::npos) {
                        goto normal_line_processing;
                    }
                }

                here_doc_operator_pos = here_pos;
                here_doc_operator_len = operator_len;

                size_t delim_start = here_pos + operator_len;
                while (delim_start < segment_no_comment.size() &&
                       (std::isspace(segment_no_comment[delim_start]) != 0)) {
                    delim_start++;
                }
                size_t delim_end = delim_start;
                while (delim_end < segment_no_comment.size() &&
                       (std::isspace(segment_no_comment[delim_end]) == 0)) {
                    delim_end++;
                }
                here_doc_delim_end_pos = delim_end;
                if (delim_start < delim_end) {
                    here_doc_delimiter =
                        segment_no_comment.substr(delim_start, delim_end - delim_start);

                    if (!here_doc_delimiter.empty() &&
                        ((here_doc_delimiter.front() == '"' && here_doc_delimiter.back() == '"') ||
                         (here_doc_delimiter.front() == '\'' &&
                          here_doc_delimiter.back() == '\''))) {
                        here_doc_expand = false;
                        if (here_doc_delimiter.size() >= 2) {
                            here_doc_delimiter =
                                here_doc_delimiter.substr(1, here_doc_delimiter.size() - 2);
                        }
                    } else {
                        here_doc_expand = true;
                    }

                    in_here_doc = true;
                    strip_tabs = is_strip_tabs;
                    here_doc_content.clear();
                    current_here_doc_line.clear();
                    continue;
                }
            }

        normal_line_processing:

            std::string segment{script.data() + start, i - start};
            if (!segment.empty() && segment.back() == '\r') {
                segment.pop_back();
            }
            lines.push_back(std::move(segment));
            start = i + 1;
        }
    }

    if (in_here_doc) {
        std::string trimmed_line = current_here_doc_line;
        size_t first_non_ws = trimmed_line.find_first_not_of(" \t");
        if (first_non_ws != std::string::npos) {
            trimmed_line.erase(0, first_non_ws);
        } else {
            trimmed_line.clear();
        }

        if (!trimmed_line.empty()) {
            size_t last_non_ws = trimmed_line.find_last_not_of(" \t\r");
            if (last_non_ws != std::string::npos) {
                trimmed_line.erase(last_non_ws + 1);
            } else {
                trimmed_line.clear();
            }
        }

        if (trimmed_line == here_doc_delimiter) {
            size_t segment_len = script.size() - start;
            if (segment_len >= current_here_doc_line.size()) {
                segment_len -= current_here_doc_line.size();
            } else {
                segment_len = 0;
            }

            if (segment_len > 0 && script[start + segment_len - 1] == '\n') {
                segment_len--;
            }
            if (segment_len > 0 && script[start + segment_len - 1] == '\r') {
                segment_len--;
            }

            std::string_view segment_view{script.data() + start, segment_len};
            process_here_doc_segment(segment_view);
            reset_here_doc_state();
            start = script.size();
        } else {
            std::string tail{script.substr(start)};
            if (!tail.empty() && tail.back() == '\r') {
                tail.pop_back();
            }
            lines.push_back(std::move(tail));
            reset_here_doc_state();
        }
    }

    if (start <= script.size()) {
        std::string_view tail_view{script.data() + start, script.size() - start};
        if (!tail_view.empty() && !in_quotes && !in_here_doc) {
            std::string tail{tail_view};
            if (!tail.empty() && tail.back() == '\r') {
                tail.pop_back();
            }
            lines.push_back(std::move(tail));
        } else if (!tail_view.empty()) {
            std::string tail{tail_view};
            if (!tail.empty() && tail.back() == '\r') {
                tail.pop_back();
            }
            lines.push_back(std::move(tail));
        }
    }

    return lines;
}

std::vector<std::string> Parser::parse_command(const std::string& cmdline) {
    if (!tokenizer) {
        tokenizer = std::make_unique<Tokenizer>();
    }
    if (!variableExpander) {
        variableExpander = std::make_unique<VariableExpander>(shell, env_vars);
    }
    if (!expansionEngine) {
        expansionEngine = std::make_unique<ExpansionEngine>(shell);
    }

    std::vector<std::string> args;
    args.reserve(8);

    try {
        std::vector<std::string> raw_args = Tokenizer::tokenize_command(cmdline);
        args = Tokenizer::merge_redirection_tokens(raw_args);
    } catch (const std::exception&) {
        return args;
    }

    if (!args.empty()) {
        auto alias_it = aliases.find(args[0]);
        if (alias_it != aliases.end()) {
            std::vector<std::string> alias_args;
            alias_args.reserve(8);

            try {
                std::vector<std::string> raw_alias_args =
                    Tokenizer::tokenize_command(alias_it->second);
                alias_args = Tokenizer::merge_redirection_tokens(raw_alias_args);

                if (!alias_args.empty()) {
                    std::vector<std::string> new_args;
                    new_args.reserve(alias_args.size() + args.size());
                    new_args.insert(new_args.end(), alias_args.begin(), alias_args.end());
                    if (args.size() > 1) {
                        new_args.insert(new_args.end(), args.begin() + 1, args.end());
                    }

                    args = new_args;

                    bool has_pipe = false;
                    for (const auto& arg : args) {
                        if (arg == "|") {
                            has_pipe = true;
                            break;
                        }
                    }

                    if (has_pipe) {
                        return {"__ALIAS_PIPELINE__", alias_it->second};
                    }
                }
            } catch (const std::exception&) {
            }
        }
    }

    std::vector<std::string> expanded_args;
    expanded_args.reserve(args.size() * 2);
    for (const auto& raw_arg : args) {
        QuoteInfo qi(raw_arg);

        if (qi.is_unquoted() && qi.value.find('{') != std::string::npos &&
            qi.value.find('}') != std::string::npos) {
            std::vector<std::string> brace_expansions = expansionEngine->expand_braces(qi.value);
            brace_expansions.reserve(8);
            expanded_args.insert(expanded_args.end(),
                                 std::make_move_iterator(brace_expansions.begin()),
                                 std::make_move_iterator(brace_expansions.end()));
        } else {
            if (qi.is_single) {
                expanded_args.push_back(create_quote_tag(QUOTE_SINGLE, qi.value));
            } else if (qi.is_double) {
                expanded_args.push_back(create_quote_tag(QUOTE_DOUBLE, qi.value));
            } else {
                expanded_args.push_back(qi.value);
            }
        }
    }
    args = std::move(expanded_args);

    std::vector<std::string> pre_expanded_args;
    for (const auto& raw_arg : args) {
        QuoteInfo qi(raw_arg);

        if (qi.is_double && qi.value == "$@" && (shell != nullptr)) {
            auto params = shell->get_positional_parameters();
            for (const auto& param : params) {
                pre_expanded_args.push_back(create_quote_tag(QUOTE_DOUBLE, param));
            }
            continue;
        }
        pre_expanded_args.push_back(raw_arg);
    }
    args = pre_expanded_args;

    for (auto& raw_arg : args) {
        QuoteInfo qi(raw_arg);

        if (!qi.is_single) {
            auto [noenv_stripped, had_noenv] = strip_noenv_sentinels(qi.value);
            if (!had_noenv) {
                try {
                    if (variableExpander->get_use_exported_vars_only()) {
                        variableExpander->expand_exported_env_vars_only(noenv_stripped);
                    } else {
                        variableExpander->expand_env_vars(noenv_stripped);
                    }
                } catch (const std::runtime_error& e) {
                    std::string error_msg = e.what();
                    if (shell != nullptr && shell->get_shell_option("nounset") &&
                        error_msg.find("parameter not set") != std::string::npos) {
                        std::cerr << "cjsh: " << e.what() << '\n';
                        throw;
                    }
                    std::cerr << "Warning: Error expanding environment variables: " << e.what()
                              << '\n';
                }
                strip_subst_literal_markers(noenv_stripped);
            } else {
                try {
                    variableExpander->expand_env_vars_selective(noenv_stripped);
                } catch (const std::runtime_error& e) {
                    std::cerr << "Warning: Error in selective environment "
                                 "variable expansion: "
                              << e.what() << '\n';
                }
                strip_subst_literal_markers(noenv_stripped);
            }

            raw_arg =
                qi.is_double ? create_quote_tag(QUOTE_DOUBLE, noenv_stripped) : noenv_stripped;
        }
    }

    std::vector<std::string> ifs_expanded_args;
    ifs_expanded_args.reserve(args.size() * 2);
    for (const auto& raw_arg : args) {
        QuoteInfo qi(raw_arg);

        if (qi.is_unquoted()) {
            std::vector<std::string> split_words = tokenizer->split_by_ifs(raw_arg, shell);
            ifs_expanded_args.insert(ifs_expanded_args.end(),
                                     std::make_move_iterator(split_words.begin()),
                                     std::make_move_iterator(split_words.end()));
        } else {
            ifs_expanded_args.push_back(raw_arg);
        }
    }
    args = std::move(ifs_expanded_args);
    auto tilde_expanded_args = expand_tilde_tokens(args);

    std::vector<std::string> final_args;
    final_args.reserve(tilde_expanded_args.size() * 2);

    bool is_double_bracket_command =
        !tilde_expanded_args.empty() && QuoteInfo(tilde_expanded_args[0]).value == "[[";

    for (const auto& raw_arg : tilde_expanded_args) {
        QuoteInfo qi(raw_arg);

        if (qi.is_unquoted() && !is_double_bracket_command) {
            auto gw = expansionEngine->expand_wildcards(qi.value);
            final_args.insert(final_args.end(), gw.begin(), gw.end());
        } else {
            final_args.push_back(qi.value);
        }
    }
    return final_args;
}

std::vector<std::string> Parser::parse_command_exported_vars_only(const std::string& cmdline) {
    if (!variableExpander) {
        variableExpander = std::make_unique<VariableExpander>(shell, env_vars);
    }
    variableExpander->set_use_exported_vars_only(true);
    std::vector<std::string> result = parse_command(cmdline);
    variableExpander->set_use_exported_vars_only(false);
    return result;
}

std::vector<Command> Parser::parse_pipeline(const std::string& command) {
    std::vector<Command> commands;
    commands.reserve(4);
    std::vector<std::string> command_parts;
    command_parts.reserve(4);

    std::string current;
    current.reserve(command.length() / 4);
    DelimiterState delimiters;

    for (size_t i = 0; i < command.length(); ++i) {
        if (delimiters.update_quote(command[i])) {
            current += command[i];
        } else if (!delimiters.in_quotes && command[i] == '(') {
            delimiters.paren_depth++;
            current += command[i];
        } else if (!delimiters.in_quotes && command[i] == ')') {
            delimiters.paren_depth--;
            current += command[i];
        } else if (!delimiters.in_quotes && command[i] == '[' && i + 1 < command.length() &&
                   command[i + 1] == '[') {
            delimiters.bracket_depth++;
            current += command[i];
            current += command[i + 1];
            i++;
        } else if (!delimiters.in_quotes && command[i] == ']' && i + 1 < command.length() &&
                   command[i + 1] == ']' && delimiters.bracket_depth > 0) {
            delimiters.bracket_depth--;
            current += command[i];
            current += command[i + 1];
            i++;
        } else if (command[i] == '|' && !delimiters.in_quotes && delimiters.paren_depth == 0 &&
                   delimiters.bracket_depth == 0) {
            if (i > 0 && command[i - 1] == '>') {
                current += command[i];
            } else {
                if (!current.empty()) {
                    command_parts.push_back(std::move(current));
                    current.clear();
                    current.reserve(command.length() / 4);
                }
            }
        } else {
            current += command[i];
        }
    }

    if (!current.empty()) {
        command_parts.push_back(std::move(current));
    }

    for (const auto& cmd_str : command_parts) {
        Command cmd;
        std::string cmd_part = cmd_str;

        bool is_background = false;
        std::string trimmed = trim_trailing_whitespace(cmd_part);

        if (!trimmed.empty() && trimmed.back() == '&') {
            bool inside_arithmetic = false;
            int arith_depth = 0;
            bool in_quotes = false;
            char quote_char = '\0';
            int bracket_depth = 0;

            for (size_t i = 0; i < trimmed.length(); ++i) {
                if (trimmed[i] == '"' || trimmed[i] == '\'') {
                    if (!in_quotes) {
                        in_quotes = true;
                        quote_char = trimmed[i];
                    } else if (quote_char == trimmed[i]) {
                        in_quotes = false;
                    }
                } else if (!in_quotes) {
                    if (i >= 2 && trimmed[i - 2] == '$' && trimmed[i - 1] == '(' &&
                        trimmed[i] == '(') {
                        arith_depth++;
                    }

                    else if (i + 1 < trimmed.length() && trimmed[i] == ')' &&
                             trimmed[i + 1] == ')' && arith_depth > 0) {
                        arith_depth--;
                        i++;
                    }

                    else if (i + 1 < trimmed.length() && trimmed[i] == '[' &&
                             trimmed[i + 1] == '[') {
                        bracket_depth++;
                        i++;
                    }

                    else if (i + 1 < trimmed.length() && trimmed[i] == ']' &&
                             trimmed[i + 1] == ']' && bracket_depth > 0) {
                        bracket_depth--;
                        i++;
                    }

                    else if (i == trimmed.length() - 1 && trimmed[i] == '&' && arith_depth > 0) {
                        inside_arithmetic = true;
                    }

                    else if (i == trimmed.length() - 1 && trimmed[i] == '&' && bracket_depth > 0) {
                        inside_arithmetic = true;
                    }
                }
            }

            if (!inside_arithmetic) {
                size_t amp_pos = cmd_part.rfind('&');
                if (amp_pos != std::string::npos && amp_pos + 1 < cmd_part.length()) {
                    size_t next_non_ws = amp_pos + 1;
                    while (next_non_ws < cmd_part.length() &&
                           (std::isspace(cmd_part[next_non_ws]) != 0)) {
                        next_non_ws++;
                    }

                    is_background =
                        next_non_ws >= cmd_part.length() || cmd_part[next_non_ws] != '>';
                } else {
                    is_background = true;
                }
            }
        }

        if (is_background) {
            cmd.background = true;
            cmd_part = trim_trailing_whitespace(trimmed.substr(0, trimmed.length() - 1));
        }

        cmd.original_text = trim_trailing_whitespace(trim_leading_whitespace(cmd_part));

        if (!cmd_part.empty()) {
            size_t lead = cmd_part.find_first_not_of(" \t\r\n");
            if (lead != std::string::npos && cmd_part[lead] == '(') {
                size_t close_paren = cmd_part.find(')', lead + 1);
                if (close_paren != std::string::npos) {
                    std::string subshell_content =
                        cmd_part.substr(lead + 1, close_paren - (lead + 1));
                    std::string remaining = cmd_part.substr(close_paren + 1);

                    cmd.args.push_back("__INTERNAL_SUBSHELL__");
                    cmd.args.push_back(subshell_content);

                    if (!remaining.empty()) {
                        std::vector<std::string> redir_tokens =
                            Tokenizer::tokenize_command(remaining);
                        std::vector<std::string> merged_redir =
                            Tokenizer::merge_redirection_tokens(redir_tokens);

                        for (size_t i = 0; i < merged_redir.size(); ++i) {
                            QuoteInfo qi_redir(merged_redir[i]);
                            const std::string& tok = qi_redir.value;
                            if (tok == "2>&1") {
                                cmd.stderr_to_stdout = true;
                            } else if (tok == ">&2") {
                                cmd.stdout_to_stderr = true;
                            } else if ((tok == "2>" || tok == "2>>") &&
                                       i + 1 < merged_redir.size()) {
                                cmd.stderr_file = QuoteInfo(merged_redir[++i]).value;
                                cmd.stderr_append = (tok == "2>>");
                            }
                        }
                    }

                    commands.push_back(cmd);
                    continue;
                }
            }
        }

        std::vector<std::string> raw_tokens = Tokenizer::tokenize_command(cmd_part);
        std::vector<std::string> tokens = Tokenizer::merge_redirection_tokens(raw_tokens);
        std::vector<std::string> filtered_args;

        auto is_all_digits = [](const std::string& s) {
            return !s.empty() && std::all_of(s.begin(), s.end(),
                                             [](unsigned char ch) { return std::isdigit(ch); });
        };

        auto handle_fd_duplication_token = [&](const std::string& token) {
            size_t pos = token.find("<&");
            char op = '<';
            if (pos == std::string::npos) {
                pos = token.find(">&");
                op = '>';
            }

            if (pos == std::string::npos) {
                return false;
            }

            std::string left = token.substr(0, pos);
            std::string right = token.substr(pos + 2);

            if (right.empty()) {
                return false;
            }

            int dst_fd = (op == '>') ? 1 : 0;
            if (!left.empty()) {
                if (!is_all_digits(left)) {
                    return false;
                }
                dst_fd = std::stoi(left);
            }

            if (right == "-") {
                cmd.fd_duplications[dst_fd] = -1;
                return true;
            }

            if (!is_all_digits(right)) {
                return false;
            }

            int src_fd = std::stoi(right);
            cmd.fd_duplications[dst_fd] = src_fd;
            return true;
        };

        for (size_t i = 0; i < tokens.size(); ++i) {
            QuoteInfo qi(tokens[i]);
            if ((qi.value == "<" || qi.value == ">" || qi.value == ">>" || qi.value == ">|" ||
                 qi.value == "&>" || qi.value == "<<" || qi.value == "<<-" || qi.value == "<<<" ||
                 qi.value == "2>" || qi.value == "2>>") &&
                (i + 1 >= tokens.size())) {
                throw std::runtime_error("cjsh: syntax error near unexpected token `newline'");
            }
        }

        auto get_next_token_value = [&tokens](size_t& i) { return QuoteInfo(tokens[++i]).value; };

        for (size_t i = 0; i < tokens.size(); ++i) {
            QuoteInfo qi(tokens[i]);

            if (qi.value == "<" && i + 1 < tokens.size()) {
                cmd.input_file = get_next_token_value(i);
            } else if (qi.value == ">" && i + 1 < tokens.size()) {
                cmd.output_file = get_next_token_value(i);
            } else if (qi.value == ">>" && i + 1 < tokens.size()) {
                cmd.append_file = get_next_token_value(i);
            } else if (qi.value == ">|" && i + 1 < tokens.size()) {
                cmd.output_file = get_next_token_value(i);
                cmd.force_overwrite = true;
            } else if (qi.value == "&>" && i + 1 < tokens.size()) {
                cmd.both_output_file = get_next_token_value(i);
                cmd.both_output = true;
            } else if ((qi.value == "<<" || qi.value == "<<-") && i + 1 < tokens.size()) {
                cmd.here_doc = get_next_token_value(i);
            } else if (qi.value == "<<<" && i + 1 < tokens.size()) {
                cmd.here_string = get_next_token_value(i);
            } else if ((qi.value == "2>" || qi.value == "2>>") && i + 1 < tokens.size()) {
                cmd.stderr_file = get_next_token_value(i);
                cmd.stderr_append = (qi.value == "2>>");
            } else if (qi.value == "2>&1") {
                cmd.stderr_to_stdout = true;
            } else if (qi.value == ">&2") {
                cmd.stdout_to_stderr = true;
            } else if (handle_fd_duplication_token(qi.value)) {
                continue;
            } else if (qi.value.find('<') == qi.value.length() - 1 && qi.value.length() > 1 &&
                       (std::isdigit(qi.value[0]) != 0) && i + 1 < tokens.size()) {
                try {
                    int fd = std::stoi(qi.value.substr(0, qi.value.length() - 1));
                    std::string file = get_next_token_value(i);
                    cmd.fd_redirections[fd] = "input:" + file;
                } catch (const std::exception&) {
                    filtered_args.push_back(tokens[i]);
                }
            } else if (qi.value.find('>') == qi.value.length() - 1 && qi.value.length() > 1 &&
                       (std::isdigit(qi.value[0]) != 0) && i + 1 < tokens.size()) {
                try {
                    int fd = std::stoi(qi.value.substr(0, qi.value.length() - 1));
                    std::string file = get_next_token_value(i);
                    cmd.fd_redirections[fd] = "output:" + file;
                } catch (const std::exception&) {
                    filtered_args.push_back(tokens[i]);
                }
            } else {
                if ((qi.value.find("<(") == 0 && qi.value.back() == ')') ||
                    (qi.value.find(">(") == 0 && qi.value.back() == ')')) {
                    cmd.process_substitutions.push_back(qi.value);
                    filtered_args.push_back(tokens[i]);
                } else {
                    filtered_args.push_back(tokens[i]);
                }
            }
        }

        bool is_double_bracket_cmd =
            !filtered_args.empty() && QuoteInfo(filtered_args[0]).value == "[[";

        auto tilde_expanded_args = expand_tilde_tokens(filtered_args);

        std::vector<std::string> final_args_local;
        bool is_subshell_command =
            !filtered_args.empty() && QuoteInfo(filtered_args[0]).value == "__INTERNAL_SUBSHELL__";
        bool is_brace_group_command = !filtered_args.empty() && QuoteInfo(filtered_args[0]).value ==
                                                                    "__INTERNAL_BRACE_GROUP__";

        for (size_t arg_idx = 0; arg_idx < tilde_expanded_args.size(); ++arg_idx) {
            const auto& raw = tilde_expanded_args[arg_idx];
            QuoteInfo qi(raw);
            std::string& val = qi.value;

            bool skip_env_expansion = (!qi.is_single) && ((is_subshell_command && arg_idx == 1) ||
                                                          (is_brace_group_command && arg_idx == 1));

            if (!qi.is_single && !skip_env_expansion) {
                if (!variableExpander) {
                    variableExpander = std::make_unique<VariableExpander>(shell, env_vars);
                }
                try {
                    variableExpander->expand_env_vars(val);
                } catch (const std::runtime_error&) {
                }
            }

            if (qi.is_unquoted() && val.find('{') != std::string::npos &&
                val.find('}') != std::string::npos) {
                if (!expansionEngine) {
                    expansionEngine = std::make_unique<ExpansionEngine>(shell);
                }
                std::vector<std::string> brace_expansions = expansionEngine->expand_braces(val);
                for (const auto& expanded_val : brace_expansions) {
                    if (!is_double_bracket_cmd &&
                        expanded_val.find_first_of("*?[]") != std::string::npos) {
                        auto wildcard_expanded = expansionEngine->expand_wildcards(expanded_val);
                        final_args_local.insert(final_args_local.end(), wildcard_expanded.begin(),
                                                wildcard_expanded.end());
                    } else {
                        final_args_local.push_back(expanded_val);
                    }
                }
            } else if (qi.is_unquoted() && !is_double_bracket_cmd &&
                       val.find_first_of("*?[]") != std::string::npos) {
                if (!expansionEngine) {
                    expansionEngine = std::make_unique<ExpansionEngine>(shell);
                }
                auto expanded = expansionEngine->expand_wildcards(val);
                final_args_local.insert(final_args_local.end(), expanded.begin(), expanded.end());
            } else {
                final_args_local.push_back(val);
            }
        }

        cmd.args = final_args_local;

        if (!variableExpander) {
            variableExpander = std::make_unique<VariableExpander>(shell, env_vars);
        }
        variableExpander->expand_command_redirection_paths(cmd);

        if (const char* home = std::getenv("HOME")) {
            if (!variableExpander) {
                variableExpander = std::make_unique<VariableExpander>(shell, env_vars);
            }
            variableExpander->expand_command_paths_with_home(cmd, std::string(home));
        }

        if (command_validation_enabled && !cmd.args.empty() &&
            should_validate_command(cmd.args[0]) && !is_valid_command(cmd.args[0])) {
            if (cjsh_filesystem::is_executable_in_cache(cmd.args[0])) {
                std::string full_path = cjsh_filesystem::find_executable_in_path(cmd.args[0]);
                if (full_path.empty()) {
                    cjsh_filesystem::remove_executable_from_cache(cmd.args[0]);
                }
            }

            throw std::runtime_error("command not found: " + cmd.args[0]);
        }

        commands.push_back(cmd);
    }

    return commands;
}

std::vector<Command> Parser::parse_pipeline_with_preprocessing(const std::string& command) {
    auto preprocessed = CommandPreprocessor::preprocess(command);

    for (const auto& pair : preprocessed.here_documents) {
        current_here_docs[pair.first] = pair.second;
    }

    auto transform_group_marker = [&preprocessed](const std::string& marker,
                                                  const std::string& builtin_name) {
        const std::string& text = preprocessed.processed_text;
        size_t lead = text.find_first_not_of(" \t\r\n");
        if (lead == std::string::npos) {
            return;
        }

        if (text.compare(lead, marker.size(), marker) != 0) {
            return;
        }

        auto find_matching_brace = [&text](size_t start_pos) {
            if (start_pos >= text.length() || text[start_pos] != '{') {
                return std::string::npos;
            }

            int depth = 0;
            for (size_t i = start_pos; i < text.length(); ++i) {
                if (is_inside_quotes(text, i)) {
                    continue;
                }

                if (text[i] == '{') {
                    depth++;
                } else if (text[i] == '}') {
                    depth--;
                    if (depth == 0) {
                        return i;
                    }
                }
            }

            return std::string::npos;
        };

        size_t brace_pos = lead + marker.size() - 1;
        size_t close_pos = find_matching_brace(brace_pos);
        if (close_pos == std::string::npos) {
            return;
        }

        size_t content_start = brace_pos + 1;
        std::string group_content = text.substr(content_start, close_pos - content_start);
        std::string remaining = text.substr(close_pos + 1);

        auto escape_double_quotes = [](const std::string& s) {
            std::string out;
            out.reserve(s.size() + 16);
            for (char c : s) {
                if (c == '"' || c == '\\') {
                    out += '\\';
                }
                out += c;
            }
            return out;
        };

        std::string rebuilt =
            builtin_name + " \"" + escape_double_quotes(group_content) + "\"" + remaining;

        std::string prefix = text.substr(0, lead);
        preprocessed.processed_text = prefix + rebuilt;
    };

    transform_group_marker("SUBSHELL{", "__INTERNAL_SUBSHELL__");
    transform_group_marker("BRACEGROUP{", "__INTERNAL_BRACE_GROUP__");

    std::vector<Command> commands = parse_pipeline(preprocessed.processed_text);

    for (auto& cmd : commands) {
        if (!cmd.input_file.empty() && cmd.input_file.find("HEREDOC_PLACEHOLDER_") == 0) {
            auto it = current_here_docs.find(cmd.input_file);
            if (it != current_here_docs.end()) {
                std::string content = it->second;

                if (content.length() >= 10 && content.substr(0, 10) == "__EXPAND__") {
                    content = content.substr(10);
                    if (!variableExpander) {
                        variableExpander = std::make_unique<VariableExpander>(shell, env_vars);
                    }
                    variableExpander->expand_env_vars(content);
                    variableExpander->expand_command_substitutions_in_string(content);
                }

                strip_subst_literal_markers(content);

                cmd.here_doc = content;
                cmd.input_file.clear();
            }
        }

        if (!cmd.here_doc.empty() && (current_here_docs.count(cmd.here_doc) != 0u)) {
            std::string content = current_here_docs[cmd.here_doc];

            if (content.length() >= 10 && content.substr(0, 10) == "__EXPAND__") {
                content = content.substr(10);
                if (!variableExpander) {
                    variableExpander = std::make_unique<VariableExpander>(shell, env_vars);
                }
                variableExpander->expand_env_vars(content);
                variableExpander->expand_command_substitutions_in_string(content);
            }

            strip_subst_literal_markers(content);

            cmd.here_doc = content;
        }
    }

    return commands;
}

bool Parser::is_env_assignment(const std::string& command, std::string& var_name,
                               std::string& var_value) {
    size_t equals_pos = command.find('=');
    if (equals_pos == std::string::npos || equals_pos == 0) {
        return false;
    }

    std::string name_part = trim_whitespace(command.substr(0, equals_pos));

    if (!is_valid_identifier(name_part)) {
        return false;
    }

    var_name = name_part;
    var_value = command.substr(equals_pos + 1);

    if (readonly_manager_is(var_name)) {
        std::cerr << "cjsh: " << var_name << ": readonly variable" << '\n';
        return false;
    }

    if (var_value.size() >= 2) {
        if ((var_value.front() == '"' && var_value.back() == '"') ||
            (var_value.front() == '\'' && var_value.back() == '\'')) {
            var_value = var_value.substr(1, var_value.length() - 2);
        }
    }
    return true;
}

std::vector<LogicalCommand> Parser::parse_logical_commands(const std::string& command) {
    std::vector<LogicalCommand> logical_commands;
    std::string current;
    DelimiterState delimiters;
    int arith_depth = 0;
    int single_bracket_depth = 0;
    int control_depth = 0;

    for (size_t i = 0; i < command.length(); ++i) {
        if (delimiters.update_quote(command[i])) {
            current += command[i];
            continue;
        }

        if (!delimiters.in_quotes && command[i] == '(') {
            if (i >= 2 && command[i - 2] == '$' && command[i - 1] == '(' && command[i] == '(') {
                arith_depth++;
            }
            delimiters.paren_depth++;
            current += command[i];
            continue;
        }

        if (!delimiters.in_quotes && command[i] == ')') {
            delimiters.paren_depth--;

            if (delimiters.paren_depth >= 0 && i + 1 < command.length() && command[i + 1] == ')' &&
                arith_depth > 0) {
                arith_depth--;
                current += command[i];
                current += command[i + 1];
                i++;
                continue;
            } else {
                current += command[i];
                continue;
            }
        }

        if (!delimiters.in_quotes && command[i] == '[') {
            if (i + 1 < command.length() && command[i + 1] == '[') {
                delimiters.bracket_depth++;
                current += command[i];
                current += command[i + 1];
                i++;
                continue;
            } else {
                bool is_test_bracket = (i == 0 || command[i - 1] == ' ' || command[i - 1] == '\t' ||
                                        command[i - 1] == ';' || command[i - 1] == '\n');
                if (is_test_bracket) {
                    single_bracket_depth++;
                }
                current += command[i];
                continue;
            }
        }

        if (!delimiters.in_quotes && command[i] == ']') {
            if (i + 1 < command.length() && command[i + 1] == ']' && delimiters.bracket_depth > 0) {
                delimiters.bracket_depth--;
                current += command[i];
                current += command[i + 1];
                i++;
                continue;
            } else {
                if (single_bracket_depth > 0) {
                    bool is_test_close =
                        (i + 1 >= command.length() || command[i + 1] == ' ' ||
                         command[i + 1] == '\t' || command[i + 1] == ';' || command[i + 1] == '&' ||
                         command[i + 1] == '|' || command[i + 1] == '\n');
                    if (is_test_close) {
                        single_bracket_depth--;
                    }
                }
                current += command[i];
                continue;
            }
        }

        if (!delimiters.in_quotes && delimiters.paren_depth == 0 && delimiters.brace_depth == 0) {
            if (command[i] == ' ' || command[i] == '\t' || i == 0) {
                size_t word_start = i;
                if (command[i] == ' ' || command[i] == '\t') {
                    word_start = i + 1;
                }

                std::string word;
                size_t j = word_start;
                while (j < command.length() && std::isalpha(command[j])) {
                    word += command[j];
                    j++;
                }

                if (word == "if" || word == "for" || word == "while" || word == "until" ||
                    word == "case") {
                    control_depth++;

                } else if ((word == "fi" || word == "done" || word == "esac") &&
                           control_depth > 0) {
                    control_depth--;
                }
            }
        }

        if (!delimiters.in_quotes && delimiters.paren_depth == 0 && arith_depth == 0 &&
            delimiters.bracket_depth == 0 && single_bracket_depth == 0 && control_depth == 0 &&
            i < command.length() - 1) {
            if (command[i] == '&' && command[i + 1] == '&') {
                if (!current.empty()) {
                    logical_commands.push_back({current, "&&"});
                    current.clear();
                }
                i++;
                continue;
            }
            if (command[i] == '|' && command[i + 1] == '|') {
                if (!current.empty()) {
                    logical_commands.push_back({current, "||"});
                    current.clear();
                }
                i++;
                continue;
            }
        }

        current += command[i];
    }

    if (!current.empty()) {
        logical_commands.push_back({current, ""});
    }

    return logical_commands;
}

std::vector<std::string> Parser::parse_semicolon_commands(const std::string& command,
                                                          bool split_on_newlines) {
    std::vector<std::string> commands;
    std::string current;
    DelimiterState scan_state;
    int control_depth = 0;

    std::vector<bool> is_semicolon_split_point(command.length(), false);

    for (size_t i = 0; i < command.length(); ++i) {
        if (scan_state.update_quote(command[i])) {
            continue;
        }
        if (!scan_state.in_quotes && command[i] == '(') {
            scan_state.paren_depth++;
        } else if (!scan_state.in_quotes && command[i] == ')') {
            scan_state.paren_depth--;
        } else if (!scan_state.in_quotes && command[i] == '{') {
            scan_state.brace_depth++;
        } else if (!scan_state.in_quotes && command[i] == '}') {
            scan_state.brace_depth--;
        } else if (!scan_state.in_quotes && scan_state.paren_depth == 0 &&
                   scan_state.brace_depth == 0) {
            if (command[i] == ' ' || command[i] == '\t' || i == 0) {
                size_t word_start = i;
                if (command[i] == ' ' || command[i] == '\t')
                    word_start = i + 1;

                std::string word;
                size_t j = word_start;
                while (j < command.length() && (std::isalpha(command[j]) != 0)) {
                    word += command[j];
                    j++;
                }

                if (word == "if" || word == "for" || word == "while" || word == "until" ||
                    word == "case") {
                    control_depth++;
                } else if ((word == "fi" || word == "done" || word == "esac") &&
                           control_depth > 0) {
                    control_depth--;
                }
            }

            if (command[i] == ';' && control_depth == 0) {
                bool is_escaped = false;
                if (i > 0 && command[i - 1] == '\\') {
                    size_t backslash_count = 0;
                    for (size_t j = i - 1; j < command.length() && command[j] == '\\'; --j) {
                        backslash_count++;
                        if (j == 0)
                            break;
                    }

                    is_escaped = (backslash_count % 2) == 1;
                }

                if (!is_escaped) {
                    is_semicolon_split_point[i] = true;
                }
            }
        }
    }

    DelimiterState parse_state;
    current.clear();

    for (size_t i = 0; i < command.length(); ++i) {
        if (parse_state.update_quote(command[i])) {
            current += command[i];
        } else if ((command[i] == ';' && is_semicolon_split_point[i]) ||
                   (split_on_newlines && !parse_state.in_quotes && command[i] == '\n')) {
            if (!current.empty()) {
                current = trim_trailing_whitespace(trim_leading_whitespace(current));
                if (!current.empty()) {
                    commands.push_back(current);
                }
                current.clear();
            }
        } else {
            current += command[i];
        }
    }

    if (!current.empty()) {
        current = trim_trailing_whitespace(trim_leading_whitespace(current));
        if (!current.empty()) {
            commands.push_back(current);
        }
    }

    return commands;
}

std::vector<std::string> Parser::expand_wildcards(const std::string& pattern) {
    if (!expansionEngine) {
        expansionEngine = std::make_unique<ExpansionEngine>(shell);
    }
    return expansionEngine->expand_wildcards(pattern);
}

void Parser::expand_env_vars(std::string& arg) {
    if (!variableExpander) {
        variableExpander = std::make_unique<VariableExpander>(shell, env_vars);
    }
    variableExpander->expand_env_vars(arg);
}

void Parser::expand_env_vars_selective(std::string& arg) {
    if (!variableExpander) {
        variableExpander = std::make_unique<VariableExpander>(shell, env_vars);
    }
    variableExpander->expand_env_vars_selective(arg);
}

void Parser::expand_exported_env_vars_only(std::string& arg) {
    if (!variableExpander) {
        variableExpander = std::make_unique<VariableExpander>(shell, env_vars);
    }
    variableExpander->expand_exported_env_vars_only(arg);
}

std::vector<std::string> Parser::split_by_ifs(const std::string& input) {
    if (!tokenizer) {
        tokenizer = std::make_unique<Tokenizer>();
    }
    return tokenizer->split_by_ifs(input, shell);
}

bool Parser::should_validate_command(const std::string& command) const {
    if (looks_like_assignment(command)) {
        return false;
    }

    if (command == "&&" || command == "||" || command == "|" || command == ";" || command == "(" ||
        command == ")" || command == "{" || command == "}" || command == "if" ||
        command == "then" || command == "else" || command == "elif" || command == "fi" ||
        command == "for" || command == "while" || command == "do" || command == "done" ||
        command == "case" || command == "esac" || command == "function") {
        return false;
    }

    if (command.empty() || command[0] == '>' || command[0] == '<' || command == ">>" ||
        command == "<<" || command == "2>" || command == "2>>" || command == "&>" ||
        command == "&>>") {
        return false;
    }

    return true;
}

bool Parser::is_valid_command(const std::string& command_name) const {
    if (command_name.empty()) {
        return false;
    }

    if ((shell != nullptr) && (shell->get_built_ins() != nullptr) &&
        (shell->get_built_ins()->is_builtin_command(command_name) != 0)) {
        return true;
    }

    if (shell != nullptr) {
        auto aliases = shell->get_aliases();
        if (aliases.find(command_name) != aliases.end()) {
            return true;
        }
    }

    if ((shell != nullptr) && (shell->get_shell_script_interpreter() != nullptr)) {
        auto* interpreter = shell->get_shell_script_interpreter();
        if ((interpreter != nullptr) && interpreter->has_function(command_name)) {
            return true;
        }
    }

    std::string cwd;
    if (shell != nullptr && shell->get_built_ins() != nullptr) {
        cwd = shell->get_built_ins()->get_current_directory();
    } else {
        std::error_code cwd_ec;
        auto current = std::filesystem::current_path(cwd_ec);
        if (!cwd_ec) {
            cwd = current.string();
        }
    }

    if (!cwd.empty()) {
        std::string trimmed_command = command_name;
        while (trimmed_command.size() > 1 && trimmed_command.back() == '/') {
            trimmed_command.pop_back();
        }

        if (!trimmed_command.empty() && trimmed_command.find('/') == std::string::npos) {
            std::error_code ec;
            std::filesystem::path candidate(command_name);
            if (!candidate.is_absolute()) {
                candidate = std::filesystem::path(cwd) / candidate;
            }

            if (std::filesystem::exists(candidate, ec) && !ec &&
                std::filesystem::is_directory(candidate, ec) && !ec) {
                return true;
            }
        }
    }

    if (command_name.find('/') != std::string::npos) {
        return true;
    }
    std::string path = cjsh_filesystem::find_executable_in_path(command_name);
    return !path.empty();
}

std::string Parser::get_command_validation_error(const std::string& command_name) const {
    if (command_name.empty()) {
        return "cjsh: empty command name";
    }

    return "cjsh: command not found: " + command_name;
}

long long Parser::evaluate_arithmetic(const std::string& expr) {
    std::string trimmed = trim_whitespace(expr);
    if (trimmed.empty()) {
        return 0;
    }

    auto find_operator = [](const std::string& s, const std::string& op) -> size_t {
        int paren_depth = 0;
        for (size_t i = 0; i <= s.length() - op.length(); ++i) {
            if (s[i] == '(') {
                paren_depth++;
            } else if (s[i] == ')') {
                paren_depth--;
            } else if (paren_depth == 0 && s.substr(i, op.length()) == op) {
                if (op.length() == 1) {
                    if ((op == "+" || op == "-") && i > 0 && s[i - 1] == op[0])
                        continue;
                    if ((op == "+" || op == "-") && i + 1 < s.length() && s[i + 1] == op[0])
                        continue;
                }
                return i;
            }
        }
        return std::string::npos;
    };

    auto parse_number = [](const std::string& s) -> long long {
        std::string clean = trim_whitespace(s);
        if (clean.empty())
            return 0;
        try {
            return std::stoll(clean);
        } catch (...) {
            return 0;
        }
    };

    size_t paren_start = trimmed.find('(');
    if (paren_start != std::string::npos) {
        int depth = 1;
        size_t paren_end = paren_start + 1;
        while (paren_end < trimmed.length() && depth > 0) {
            if (trimmed[paren_end] == '(') {
                depth++;
            } else if (trimmed[paren_end] == ')') {
                depth--;
            }
            paren_end++;
        }
        if (depth == 0) {
            std::string inner = trimmed.substr(paren_start + 1, paren_end - paren_start - 2);
            long long inner_result = evaluate_arithmetic(inner);
            std::string new_expr = trimmed.substr(0, paren_start) + std::to_string(inner_result) +
                                   trimmed.substr(paren_end);
            return evaluate_arithmetic(new_expr);
        }
    }

    std::vector<std::string> operators = {"+", "-", "*", "/", "%"};

    for (const auto& op : operators) {
        size_t pos = find_operator(trimmed, op);
        if (pos != std::string::npos) {
            std::string left = trimmed.substr(0, pos);
            std::string right = trimmed.substr(pos + op.length());

            long long left_val = evaluate_arithmetic(left);
            long long right_val = evaluate_arithmetic(right);

            switch (op[0]) {
                case '+':
                    return left_val + right_val;
                case '-':
                    return left_val - right_val;
                case '*':
                    return left_val * right_val;
                case '/':
                    if (right_val == 0)
                        throw std::runtime_error("Division by zero");
                    return left_val / right_val;
                case '%':
                    if (right_val == 0)
                        throw std::runtime_error("Division by zero");
                    return left_val % right_val;
            }
        }
    }

    return parse_number(trimmed);
}

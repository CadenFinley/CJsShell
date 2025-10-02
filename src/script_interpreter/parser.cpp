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
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include "builtin.h"
#include "cjsh.h"
#include "command_preprocessor.h"
#include "error_out.h"
#include "job_control.h"
#include "readonly_command.h"
#include "shell.h"
#include "shell_script_interpreter.h"
#include "utils/cjsh_filesystem.h"
#include "utils/suggestion_utils.h"

namespace {
struct DelimiterState {
    bool in_quotes = false;
    char quote_char = '\0';
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;

    bool update_quote(char c) {
        if (c == '"' || c == '\'') {
            if (!in_quotes) {
                in_quotes = true;
                quote_char = c;
            } else if (quote_char == c) {
                in_quotes = false;
            }
            return true;
        }
        return false;
    }

    void reset() {
        *this = {};
    }
};
}  // namespace

std::vector<std::string> Parser::parse_into_lines(const std::string& script) {
    auto strip_inline_comment = [](std::string_view s) -> std::string {
        bool in_quotes = false;
        char quote = '\0';
        const char* data = s.data();
        size_t size = s.size();

        for (size_t i = 0; i < size; ++i) {
            char c = data[i];

            if (c == '"' || c == '\'') {
                size_t backslash_count = 0;
                for (size_t j = i; j > 0; --j) {
                    if (data[j - 1] == '\\') {
                        backslash_count++;
                    } else {
                        break;
                    }
                }

                bool is_escaped = (backslash_count % 2) == 1;

                if (!is_escaped) {
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
    lines.reserve(32);
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
    here_doc_delimiter.reserve(64);
    std::string here_doc_content;
    here_doc_content.reserve(1024);
    std::string current_here_doc_line;
    current_here_doc_line.reserve(256);

    auto add_here_doc_placeholder_line = [&](std::string before, std::string rest) {
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

    for (size_t i = 0; i < script.size(); ++i) {
        char c = script[i];

        if (in_here_doc) {
            if (c == '\n') {
                std::string trimmed_line = current_here_doc_line;

                trimmed_line.erase(0, trimmed_line.find_first_not_of(" \t"));
                trimmed_line.erase(trimmed_line.find_last_not_of(" \t\r") + 1);

                if (trimmed_line == here_doc_delimiter) {
                    std::string_view segment_view{script.data() + start, i - start};

                    bool segment_created = false;
                    if (here_doc_operator_pos != std::string::npos && here_doc_operator_len > 0) {
                        size_t delim_end_in_segment = here_doc_delim_end_pos;
                        if (delim_end_in_segment > segment_view.size()) {
                            delim_end_in_segment = segment_view.size();
                        }

                        std::string before_here{segment_view.substr(0, here_doc_operator_pos)};
                        size_t line_end = segment_view.find('\n', delim_end_in_segment);
                        std::string rest_of_line;
                        if (line_end != std::string::npos) {
                            rest_of_line.assign(segment_view.substr(delim_end_in_segment, line_end - delim_end_in_segment));
                        } else {
                            rest_of_line.assign(segment_view.substr(delim_end_in_segment));
                        }

                        add_here_doc_placeholder_line(std::move(before_here), std::move(rest_of_line));
                        segment_created = true;
                    }

                    if (!segment_created) {
                        size_t here_pos = segment_view.find("<<- " + here_doc_delimiter);
                        if (here_pos == std::string::npos) {
                            here_pos = segment_view.find("<<-" + here_doc_delimiter);
                            if (here_pos == std::string::npos) {
                                here_pos = segment_view.find("<< " + here_doc_delimiter);
                                if (here_pos == std::string::npos) {
                                    here_pos = segment_view.find("<<" + here_doc_delimiter);
                                }
                            }
                        }

                        if (here_pos != std::string::npos) {
                            size_t operator_len = 2;
                            if (segment_view.substr(here_pos, 3) == "<<-") {
                                operator_len = 3;
                            }

                            size_t delim_start_in_segment = here_pos + operator_len;
                            while (delim_start_in_segment < segment_view.size() &&
                                   std::isspace(static_cast<unsigned char>(segment_view[delim_start_in_segment]))) {
                                delim_start_in_segment++;
                            }

                            size_t delim_end_in_segment = delim_start_in_segment;
                            while (delim_end_in_segment < segment_view.size() &&
                                   !std::isspace(static_cast<unsigned char>(segment_view[delim_end_in_segment]))) {
                                delim_end_in_segment++;
                            }

                            std::string before_here{segment_view.substr(0, here_pos)};
                            size_t line_end = segment_view.find('\n', delim_end_in_segment);
                            std::string rest_of_line;
                            if (line_end != std::string::npos) {
                                rest_of_line.assign(segment_view.substr(delim_end_in_segment, line_end - delim_end_in_segment));
                            } else {
                                rest_of_line.assign(segment_view.substr(delim_end_in_segment));
                            }

                            add_here_doc_placeholder_line(std::move(before_here), std::move(rest_of_line));
                            segment_created = true;
                        } else {
                            std::string segment{segment_view};
                            if (!segment.empty() && segment.back() == '\r') {
                                segment.pop_back();
                            }
                            lines.push_back(std::move(segment));
                            segment_created = true;
                        }
                    }

                    here_doc_operator_pos = std::string::npos;
                    here_doc_operator_len = 0;
                    here_doc_delim_end_pos = std::string::npos;
                    in_here_doc = false;
                    strip_tabs = false;
                    here_doc_expand = true;
                    here_doc_delimiter.clear();
                    here_doc_content.clear();
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
                while (delim_start < segment_no_comment.size() && std::isspace(segment_no_comment[delim_start])) {
                    delim_start++;
                }
                size_t delim_end = delim_start;
                while (delim_end < segment_no_comment.size() && !std::isspace(segment_no_comment[delim_end])) {
                    delim_end++;
                }
                here_doc_delim_end_pos = delim_end;
                if (delim_start < delim_end) {
                    here_doc_delimiter = segment_no_comment.substr(delim_start, delim_end - delim_start);

                    if (!here_doc_delimiter.empty() && ((here_doc_delimiter.front() == '"' && here_doc_delimiter.back() == '"') ||
                                                        (here_doc_delimiter.front() == '\'' && here_doc_delimiter.back() == '\''))) {
                        here_doc_expand = false;
                        if (here_doc_delimiter.size() >= 2) {
                            here_doc_delimiter = here_doc_delimiter.substr(1, here_doc_delimiter.size() - 2);
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

            std::string segment = (segment_len > 0) ? script.substr(start, segment_len) : std::string();

            std::string_view segment_view{segment};

            bool segment_created = false;
            if (here_doc_operator_pos != std::string::npos && here_doc_operator_len > 0) {
                size_t delim_end_in_segment = here_doc_delim_end_pos;
                if (delim_end_in_segment > segment_view.size()) {
                    delim_end_in_segment = segment_view.size();
                }

                std::string before_here{segment_view.substr(0, here_doc_operator_pos)};
                size_t line_end = segment_view.find('\n', delim_end_in_segment);
                std::string rest_of_line;
                if (line_end != std::string::npos) {
                    rest_of_line.assign(segment_view.substr(delim_end_in_segment, line_end - delim_end_in_segment));
                } else {
                    rest_of_line.assign(segment_view.substr(delim_end_in_segment));
                }

                std::string placeholder = "HEREDOC_PLACEHOLDER_" + std::to_string(lines.size());

                std::string stored_content = here_doc_content;
                if (here_doc_expand) {
                    stored_content = "__EXPAND__" + stored_content;
                }
                current_here_docs[placeholder] = std::move(stored_content);

                std::string rebuilt = before_here + "< " + placeholder + rest_of_line;
                if (!rebuilt.empty() && rebuilt.back() == '\r') {
                    rebuilt.pop_back();
                }
                lines.push_back(std::move(rebuilt));
                segment_created = true;
            }

            if (!segment_created) {
                size_t here_pos = segment_view.find("<<- " + here_doc_delimiter);
                if (here_pos == std::string::npos) {
                    here_pos = segment_view.find("<<-" + here_doc_delimiter);
                    if (here_pos == std::string::npos) {
                        here_pos = segment_view.find("<< " + here_doc_delimiter);
                        if (here_pos == std::string::npos) {
                            here_pos = segment_view.find("<<" + here_doc_delimiter);
                        }
                    }
                }

                if (here_pos != std::string::npos) {
                    size_t operator_len = 2;
                    if (segment_view.substr(here_pos, 3) == "<<-") {
                        operator_len = 3;
                    }

                    size_t delim_start_in_segment = here_pos + operator_len;
                    while (delim_start_in_segment < segment_view.size() &&
                           std::isspace(static_cast<unsigned char>(segment_view[delim_start_in_segment]))) {
                        delim_start_in_segment++;
                    }

                    size_t delim_end_in_segment = delim_start_in_segment;
                    while (delim_end_in_segment < segment_view.size() &&
                           !std::isspace(static_cast<unsigned char>(segment_view[delim_end_in_segment]))) {
                        delim_end_in_segment++;
                    }

                    std::string before_here{segment_view.substr(0, here_pos)};
                    size_t line_end = segment_view.find('\n', delim_end_in_segment);
                    std::string rest_of_line;
                    if (line_end != std::string::npos) {
                        rest_of_line.assign(segment_view.substr(delim_end_in_segment, line_end - delim_end_in_segment));
                    } else {
                        rest_of_line.assign(segment_view.substr(delim_end_in_segment));
                    }

                    std::string placeholder = "HEREDOC_PLACEHOLDER_" + std::to_string(lines.size());

                    std::string stored_content = here_doc_content;
                    if (here_doc_expand) {
                        stored_content = "__EXPAND__" + stored_content;
                    }
                    current_here_docs[placeholder] = std::move(stored_content);

                    std::string rebuilt = before_here + "< " + placeholder + rest_of_line;
                    if (!rebuilt.empty() && rebuilt.back() == '\r') {
                        rebuilt.pop_back();
                    }
                    lines.push_back(std::move(rebuilt));
                    segment_created = true;
                } else {
                    if (!segment.empty() && segment.back() == '\r') {
                        segment.pop_back();
                    }
                    lines.push_back(std::move(segment));
                    segment_created = true;
                }
            }

            in_here_doc = false;
            strip_tabs = false;
            here_doc_expand = true;
            here_doc_delimiter.clear();
            here_doc_content.clear();
            current_here_doc_line.clear();
            start = script.size();
            here_doc_operator_pos = std::string::npos;
            here_doc_operator_len = 0;
            here_doc_delim_end_pos = std::string::npos;
        } else {
            std::string tail{script.substr(start)};
            if (!tail.empty() && tail.back() == '\r') {
                tail.pop_back();
            }
            lines.push_back(std::move(tail));
            in_here_doc = false;
            strip_tabs = false;
            here_doc_expand = true;
            here_doc_operator_pos = std::string::npos;
            here_doc_operator_len = 0;
            here_doc_delim_end_pos = std::string::npos;
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

namespace {

static const char QUOTE_PREFIX = '\x1F';
static const char QUOTE_SINGLE = 'S';
static const char QUOTE_DOUBLE = 'D';
static const std::string SUBST_LITERAL_START = "\x1E__SUBST_LITERAL_START__\x1E";
static const std::string SUBST_LITERAL_END = "\x1E__SUBST_LITERAL_END__\x1E";

inline bool is_single_quoted_token(const std::string& s) {
    return s.size() >= 2 && s[0] == QUOTE_PREFIX && s[1] == QUOTE_SINGLE;
}

inline bool is_double_quoted_token(const std::string& s) {
    return s.size() >= 2 && s[0] == QUOTE_PREFIX && s[1] == QUOTE_DOUBLE;
}

inline std::string strip_quote_tag(const std::string& s) {
    if (s.size() >= 2 && s[0] == QUOTE_PREFIX && (s[1] == QUOTE_SINGLE || s[1] == QUOTE_DOUBLE)) {
        return s.substr(2);
    }
    return s;
}

static inline std::pair<std::string, bool> strip_noenv_sentinels(const std::string& s) {
    const std::string start = "\x1E__NOENV_START__\x1E";
    const std::string end = "\x1E__NOENV_END__\x1E";
    size_t a = s.find(start);
    size_t b = s.rfind(end);
    if (a != std::string::npos && b != std::string::npos && b >= a + start.size()) {
        std::string mid = s.substr(a + start.size(), b - (a + start.size()));
        std::string out = s.substr(0, a) + mid + s.substr(b + end.size());
        return {out, true};
    }
    return {s, false};
}

static inline bool strip_subst_literal_markers(std::string& value) {
    if (value.empty()) {
        return false;
    }

    bool changed = false;
    std::string result;
    result.reserve(value.size());

    for (size_t i = 0; i < value.size();) {
        if (value.compare(i, SUBST_LITERAL_START.size(), SUBST_LITERAL_START) == 0) {
            i += SUBST_LITERAL_START.size();
            changed = true;
            continue;
        }
        if (value.compare(i, SUBST_LITERAL_END.size(), SUBST_LITERAL_END) == 0) {
            i += SUBST_LITERAL_END.size();
            changed = true;
            continue;
        }

        result.push_back(value[i]);
        ++i;
    }

    if (changed) {
        value.swap(result);
    }

    return changed;
}

static void expand_command_substitutions_in_string(std::string& text) {
    if (text.find('$') == std::string::npos && text.find('`') == std::string::npos) {
        return;
    }

    std::string result;
    result.reserve(text.size());

    for (size_t i = 0; i < text.size();) {
        if (text[i] == '$' && i + 1 < text.size() && text[i + 1] == '(') {
            size_t pos = i + 2;
            int depth = 1;
            bool in_single = false;
            bool in_double = false;
            while (pos < text.size() && depth > 0) {
                char ch = text[pos];
                if (ch == '\\') {
                    pos += 2;
                    continue;
                }
                if (ch == '\'' && !in_double) {
                    in_single = !in_single;
                } else if (ch == '"' && !in_single) {
                    in_double = !in_double;
                } else if (!in_single) {
                    if (ch == '(') {
                        depth++;
                    } else if (ch == ')') {
                        depth--;
                        if (depth == 0) {
                            break;
                        }
                    }
                }
                pos++;
            }

            if (depth == 0 && pos < text.size()) {
                std::string command = text.substr(i + 2, pos - (i + 2));
                auto output = cjsh_filesystem::FileOperations::read_command_output(command);
                if (output.is_ok()) {
                    std::string value = output.value();
                    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
                        value.pop_back();
                    }
                    result += value;
                    i = pos + 1;
                    continue;
                }
            }
        } else if (text[i] == '`') {
            size_t pos = i + 1;
            while (pos < text.size()) {
                if (text[pos] == '\\' && pos + 1 < text.size()) {
                    pos += 2;
                    continue;
                }
                if (text[pos] == '`') {
                    break;
                }
                pos++;
            }
            if (pos < text.size() && text[pos] == '`') {
                std::string command = text.substr(i + 1, pos - (i + 1));
                std::string cleaned;
                cleaned.reserve(command.size());
                for (size_t k = 0; k < command.size(); ++k) {
                    if (command[k] == '\\' && k + 1 < command.size() && command[k + 1] == '`') {
                        cleaned.push_back('`');
                        ++k;
                    } else {
                        cleaned.push_back(command[k]);
                    }
                }
                auto output = cjsh_filesystem::FileOperations::read_command_output(cleaned);
                if (output.is_ok()) {
                    std::string value = output.value();
                    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
                        value.pop_back();
                    }
                    result += value;
                    i = pos + 1;
                    continue;
                }
            }
        }

        result += text[i];
        ++i;
    }

    text.swap(result);
}

inline bool contains_tilde(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    if (value.front() == '~') {
        return true;
    }
    return value.find('~', 1) != std::string::npos;
}

inline std::string expand_tilde_value(const std::string& value, const std::string& home) {
    if (!value.empty() && value.front() == '~') {
        return home + value.substr(1);
    }
    return value;
}

std::vector<std::string> expand_tilde_tokens(const std::vector<std::string>& tokens) {
    std::vector<std::string> result;
    result.reserve(tokens.size());

    const char* home_dir = std::getenv("HOME");
    const bool has_home = home_dir != nullptr;
    const std::string home = has_home ? std::string(home_dir) : std::string();

    for (const auto& raw : tokens) {
        const bool is_single = is_single_quoted_token(raw);
        const bool is_double = is_double_quoted_token(raw);
        std::string value = strip_quote_tag(raw);

        if (!is_single && !is_double && has_home && contains_tilde(value)) {
            result.push_back(expand_tilde_value(value, home));
        } else if (!is_single && !is_double) {
            result.push_back(value);
        } else if (is_single) {
            result.push_back(std::string(1, QUOTE_PREFIX) + QUOTE_SINGLE + value);
        } else {
            result.push_back(std::string(1, QUOTE_PREFIX) + QUOTE_DOUBLE + value);
        }
    }

    return result;
}

void expand_command_paths_with_home(Command& cmd, const std::string& home) {
    auto expand_path = [&](std::string& path) {
        if (!path.empty() && path.front() == '~') {
            path = home + path.substr(1);
        }
    };

    expand_path(cmd.input_file);
    expand_path(cmd.output_file);
    expand_path(cmd.append_file);
    expand_path(cmd.stderr_file);
    expand_path(cmd.both_output_file);

    for (auto& fd_redir : cmd.fd_redirections) {
        expand_path(fd_redir.second);
    }
}
}  // namespace

std::vector<std::string> tokenize_command(const std::string& cmdline) {
    std::vector<std::string> tokens;
    tokens.reserve(16);
    std::string current_token;
    current_token.reserve(128);
    bool in_quotes = false;
    char quote_char = '\0';
    bool escaped = false;
    int arith_depth = 0;
    int brace_depth = 0;
    int bracket_depth = 0;
    bool in_subst_literal = false;

    bool token_saw_single = false;
    bool token_saw_double = false;

    auto flush_current_token = [&]() {
        if (!current_token.empty() || token_saw_single || token_saw_double) {
            if (token_saw_single && !token_saw_double) {
                tokens.push_back(std::string(1, QUOTE_PREFIX) + QUOTE_SINGLE + current_token);
            } else if (token_saw_double && !token_saw_single) {
                tokens.push_back(std::string(1, QUOTE_PREFIX) + QUOTE_DOUBLE + current_token);
            } else if (!current_token.empty()) {
                tokens.push_back(current_token);
            }
            current_token.clear();
            token_saw_single = token_saw_double = false;
        }
    };

    for (size_t i = 0; i < cmdline.length(); ++i) {
        if (!in_subst_literal && cmdline.compare(i, SUBST_LITERAL_START.size(), SUBST_LITERAL_START) == 0) {
            in_subst_literal = true;
            i += SUBST_LITERAL_START.size() - 1;
            continue;
        }

        if (in_subst_literal && cmdline.compare(i, SUBST_LITERAL_END.size(), SUBST_LITERAL_END) == 0) {
            in_subst_literal = false;
            i += SUBST_LITERAL_END.size() - 1;
            continue;
        }

        char c = cmdline[i];

        if (escaped) {
            if (in_quotes && quote_char == '"') {
                if (c == '$' || c == '`' || c == '"' || c == '\\' || c == '\n') {
                    current_token += c;
                } else {
                    current_token += '\\';
                    current_token += c;
                }
            } else {
                if (c == '*' || c == '?' || c == '[' || c == ']') {
                    current_token += '\x1F';
                }
                current_token += c;
            }
            escaped = false;
        } else if (!in_subst_literal && c == '\\' && (!in_quotes || quote_char != '\'')) {
            escaped = true;
        } else if ((c == '"' || c == '\'') && !in_quotes && !in_subst_literal) {
            in_quotes = true;
            quote_char = c;
            if (c == '\'')
                token_saw_single = true;
            else
                token_saw_double = true;
        } else if (c == quote_char && in_quotes && !in_subst_literal) {
            in_quotes = false;
            quote_char = '\0';
        } else if (!in_quotes) {
            if (c == '{' && current_token.length() >= 1 && current_token.back() == '$') {
                brace_depth++;
                current_token += c;
            }

            else if (c == '}' && brace_depth > 0) {
                brace_depth--;
                current_token += c;
            }

            else if (std::isspace(c)) {
                if ((!current_token.empty() || token_saw_single || token_saw_double) && arith_depth == 0 && brace_depth == 0) {
                    flush_current_token();
                } else if (arith_depth > 0 || brace_depth > 0) {
                    current_token += c;
                }
            }

            else if (c == '(' && i >= 1 && cmdline[i - 1] == '(' && current_token.length() >= 1 && current_token.back() == '$') {
                arith_depth++;
                current_token += c;
            }

            else if (c == ')' && i + 1 < cmdline.length() && cmdline[i + 1] == ')' && arith_depth > 0) {
                arith_depth--;
                current_token += c;
                current_token += cmdline[i + 1];
                i++;
            }

            else if (c == '[' && i + 1 < cmdline.length() && cmdline[i + 1] == '[') {
                bracket_depth++;
                flush_current_token();
                tokens.push_back("[[");
                i++;
            }

            else if (c == ']' && i + 1 < cmdline.length() && cmdline[i + 1] == ']' && bracket_depth > 0) {
                bracket_depth--;
                flush_current_token();
                tokens.push_back("]]");
                i++;
            }

            else if (bracket_depth > 0 && i + 1 < cmdline.length() &&
                     ((c == '&' && cmdline[i + 1] == '&') || (c == '|' && cmdline[i + 1] == '|'))) {
                flush_current_token();

                tokens.push_back(std::string(1, c) + cmdline[i + 1]);
                i++;
            }

            else if ((c == '(' || c == ')' || c == '<' || c == '>' ||
                      (c == '&' && arith_depth == 0 && brace_depth == 0 && bracket_depth == 0) ||
                      (c == '|' && arith_depth == 0 && brace_depth == 0 && bracket_depth == 0))) {
                flush_current_token();

                bool handled_special = false;
                if ((c == '<' || c == '>') && i + 1 < cmdline.length() && cmdline[i + 1] == '(') {
                    size_t j = i + 2;
                    int paren_depth = 1;
                    bool in_single = false;
                    bool in_double = false;

                    while (j < cmdline.length()) {
                        char ch = cmdline[j];
                        if (!in_double && ch == '\'' && (j == i + 2 || cmdline[j - 1] != '\\')) {
                            in_single = !in_single;
                        } else if (!in_single && ch == '"' && (j == i + 2 || cmdline[j - 1] != '\\')) {
                            in_double = !in_double;
                        } else if (!in_single && !in_double) {
                            if (ch == '(') {
                                paren_depth++;
                            } else if (ch == ')') {
                                paren_depth--;
                                if (paren_depth == 0) {
                                    break;
                                }
                            }
                        }

                        if (ch == '\\' && !in_single) {
                            j += 2;
                            continue;
                        }
                        j++;
                    }

                    if (paren_depth == 0 && j < cmdline.length()) {
                        tokens.push_back(cmdline.substr(i, j - i + 1));
                        i = j;
                        handled_special = true;
                    }
                }

                if (handled_special) {
                    continue;
                }

                if (c == '<' && i + 1 < cmdline.length() && cmdline[i + 1] == '&') {
                    size_t j = i + 2;
                    while (j < cmdline.length() && (std::isdigit(static_cast<unsigned char>(cmdline[j])) || cmdline[j] == '-')) {
                        j++;
                    }
                    if (j > i + 2) {
                        tokens.push_back(cmdline.substr(i, j - i));
                        i = j - 1;
                        continue;
                    }
                }
                if (c == '>' && i + 1 < cmdline.length() && cmdline[i + 1] == '&') {
                    size_t j = i + 2;
                    while (j < cmdline.length() && (std::isdigit(static_cast<unsigned char>(cmdline[j])) || cmdline[j] == '-')) {
                        j++;
                    }
                    if (j > i + 2) {
                        tokens.push_back(cmdline.substr(i, j - i));
                        i = j - 1;
                        continue;
                    }
                }
                if (c == '&' && !tokens.empty() && (tokens.back() == "<" || tokens.back() == ">" || tokens.back() == ">>") &&
                    i + 1 < cmdline.length()) {
                    size_t j = i + 1;
                    while (j < cmdline.length() && (std::isdigit(static_cast<unsigned char>(cmdline[j])) || cmdline[j] == '-')) {
                        j++;
                    }
                    if (j > i + 1) {
                        std::string merged = tokens.back();
                        merged += '&';
                        merged += cmdline.substr(i + 1, j - (i + 1));
                        tokens.back() = std::move(merged);
                        i = j - 1;
                        continue;
                    }
                }
                tokens.push_back(std::string(1, c));
            } else {
                current_token += c;
            }
        } else {
            current_token += c;
        }
    }

    flush_current_token();

    if (in_quotes) {
        throw std::runtime_error("cjsh: Unclosed quote: missing closing " + std::string(1, quote_char));
    }

    return tokens;
}

std::vector<std::string> merge_redirection_tokens(const std::vector<std::string>& tokens) {
    std::vector<std::string> result;
    result.reserve(tokens.size());

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];

        if (token == "2" && i + 1 < tokens.size()) {
            if (tokens[i + 1] == ">&1") {
                result.push_back("2>&1");
                i++;
            } else if (i + 3 < tokens.size() && tokens[i + 1] == ">" && tokens[i + 2] == "&" && tokens[i + 3] == "1") {
                result.push_back("2>&1");
                i += 3;
            } else if (i + 2 < tokens.size() && tokens[i + 1] == ">" && tokens[i + 2] == ">") {
                result.push_back("2>>");
                i += 2;
            } else if (i + 1 < tokens.size() && tokens[i + 1] == ">") {
                result.push_back("2>");
                i++;
            } else {
                result.push_back(token);
            }
        }

        else if (token == "&" && i + 1 < tokens.size() && tokens[i + 1] == ">") {
            result.push_back("&>");
            i++;
        }

        else if (token == ">" && i + 1 < tokens.size() && tokens[i + 1] == "|") {
            result.push_back(">|");
            i++;
        }

        else if (token == "<" && i + 2 < tokens.size() && tokens[i + 1] == "<" && tokens[i + 2] == "<") {
            result.push_back("<<<");
            i += 2;
        }

        else if (token == "<" && i + 2 < tokens.size() && tokens[i + 1] == "<" && tokens[i + 2] == "-") {
            result.push_back("<<-");
            i += 2;
        }

        else if (token == "<" && i + 1 < tokens.size() && tokens[i + 1] == "<") {
            result.push_back("<<");
            i++;
        }

        else if (token == "<<" && i + 1 < tokens.size() && tokens[i + 1] == "<") {
            result.push_back("<<<");
            i++;
        }

        else if (token == "<<" && i + 1 < tokens.size() && tokens[i + 1] == "-") {
            result.push_back("<<-");
            i++;
        }

        else if (token == "<" && i + 2 < tokens.size() && tokens[i + 1] == "&" && tokens[i + 2].length() > 0 &&
                 std::isdigit(tokens[i + 2][0])) {
            result.push_back("<&" + tokens[i + 2]);
            i += 2;
        }

        else if (token == ">" && i + 1 < tokens.size() && tokens[i + 1] == ">") {
            result.push_back(">>");
            i++;
        }

        else if ((token == ">>" || token == ">") && i + 1 < tokens.size() && (tokens[i + 1] == "&1" || tokens[i + 1] == "&2")) {
            result.push_back(token + tokens[i + 1]);
            i++;
        }

        else if (token == ">" && i + 2 < tokens.size() && tokens[i + 1] == "&" && tokens[i + 2] == "2") {
            result.push_back(">&2");
            i += 2;
        }

        else if (token == ">" && i + 2 < tokens.size() && tokens[i + 1] == "&" && tokens[i + 2] == "1") {
            result.push_back(">&1");
            i += 2;
        }

        else if (token == "2" && i + 2 < tokens.size() && tokens[i + 1] == "&" && tokens[i + 2] == "1") {
            result.push_back("2>&1");
            i += 2;
        }

        else if (token == "2" && i + 1 < tokens.size() && (tokens[i + 1] == ">" || tokens[i + 1] == ">>")) {
            result.push_back("2" + tokens[i + 1]);
            i++;
        }

        else if (std::isdigit(token[0]) && token.length() == 1 && i + 1 < tokens.size() && (tokens[i + 1] == "<" || tokens[i + 1] == ">")) {
            result.push_back(token + tokens[i + 1]);
            i++;
        }

        else if (std::isdigit(token[0]) && token.find_first_not_of("0123456789") == std::string::npos && i + 1 < tokens.size() &&
                 tokens[i + 1].rfind("<&", 0) == 0) {
            result.push_back(token + tokens[i + 1]);
            i++;
        }

        else if (std::isdigit(token[0]) && token.find_first_not_of("0123456789") == std::string::npos && i + 1 < tokens.size() &&
                 tokens[i + 1].rfind(">&", 0) == 0) {
            result.push_back(token + tokens[i + 1]);
            i++;
        }

        else if (std::isdigit(token[0]) && token.length() > 1) {
            size_t first_non_digit = 0;
            while (first_non_digit < token.length() && std::isdigit(token[first_non_digit])) {
                first_non_digit++;
            }
            if (first_non_digit > 0 && first_non_digit < token.length()) {
                std::string rest = token.substr(first_non_digit);
                if (rest == "<" || rest == ">" || rest.find(">&") == 0) {
                    result.push_back(token);
                } else {
                    result.push_back(token);
                }
            } else {
                result.push_back(token);
            }
        } else {
            result.push_back(token);
        }
    }

    return result;
}

std::vector<std::string> Parser::parse_command(const std::string& cmdline) {
    std::vector<std::string> args;
    args.reserve(8);

    try {
        std::vector<std::string> raw_args = tokenize_command(cmdline);
        args = merge_redirection_tokens(raw_args);
    } catch (const std::exception&) {
        return args;
    }

    if (!args.empty()) {
        auto alias_it = aliases.find(args[0]);
        if (alias_it != aliases.end()) {
            std::vector<std::string> alias_args;
            alias_args.reserve(8);

            try {
                std::vector<std::string> raw_alias_args = tokenize_command(alias_it->second);
                alias_args = merge_redirection_tokens(raw_alias_args);

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
        const bool is_single = is_single_quoted_token(raw_arg);
        const bool is_double = is_double_quoted_token(raw_arg);
        const std::string arg = strip_quote_tag(raw_arg);

        if (!is_single && !is_double && arg.find('{') != std::string::npos && arg.find('}') != std::string::npos) {
            std::vector<std::string> brace_expansions = expand_braces(arg);
            brace_expansions.reserve(8);
            expanded_args.insert(expanded_args.end(), std::make_move_iterator(brace_expansions.begin()),
                                 std::make_move_iterator(brace_expansions.end()));
        } else {
            if (is_single) {
                expanded_args.push_back(std::string(1, QUOTE_PREFIX) + QUOTE_SINGLE + arg);
            } else if (is_double) {
                expanded_args.push_back(std::string(1, QUOTE_PREFIX) + QUOTE_DOUBLE + arg);
            } else {
                expanded_args.push_back(arg);
            }
        }
    }
    args = std::move(expanded_args);

    std::vector<std::string> pre_expanded_args;
    for (const auto& raw_arg : args) {
        if (is_double_quoted_token(raw_arg)) {
            std::string tmp = strip_quote_tag(raw_arg);

            if (tmp == "$@" && shell) {
                auto params = shell->get_positional_parameters();
                for (const auto& param : params) {
                    pre_expanded_args.push_back(std::string(1, QUOTE_PREFIX) + QUOTE_DOUBLE + param);
                }
                continue;
            }
        }
        pre_expanded_args.push_back(raw_arg);
    }
    args = pre_expanded_args;

    for (auto& raw_arg : args) {
        if (!is_single_quoted_token(raw_arg)) {
            std::string tmp = strip_quote_tag(raw_arg);
            auto [noenv_stripped, had_noenv] = strip_noenv_sentinels(tmp);
            if (!had_noenv) {
                try {
                    if (use_exported_vars_only) {
                        expand_exported_env_vars_only(noenv_stripped);
                    } else {
                        expand_env_vars(noenv_stripped);
                    }
                } catch (const std::runtime_error& e) {
                    std::cerr << "Warning: Error expanding environment variables: " << e.what() << std::endl;
                }
                strip_subst_literal_markers(noenv_stripped);
            } else {
                try {
                    expand_env_vars_selective(tmp);
                    noenv_stripped = tmp;
                } catch (const std::runtime_error& e) {
                    std::cerr << "Warning: Error in selective environment "
                                 "variable expansion: "
                              << e.what() << std::endl;
                    noenv_stripped = tmp;
                }
                strip_subst_literal_markers(noenv_stripped);
            }

            if (is_double_quoted_token(raw_arg)) {
                raw_arg = std::string(1, QUOTE_PREFIX) + QUOTE_DOUBLE + noenv_stripped;
            } else {
                raw_arg = noenv_stripped;
            }
        }
    }

    std::vector<std::string> ifs_expanded_args;
    ifs_expanded_args.reserve(args.size() * 2);
    for (const auto& raw_arg : args) {
        if (!is_single_quoted_token(raw_arg) && !is_double_quoted_token(raw_arg)) {
            std::vector<std::string> split_words = split_by_ifs(raw_arg);
            ifs_expanded_args.insert(ifs_expanded_args.end(), std::make_move_iterator(split_words.begin()),
                                     std::make_move_iterator(split_words.end()));
        } else {
            ifs_expanded_args.push_back(raw_arg);
        }
    }
    args = std::move(ifs_expanded_args);
    auto tilde_expanded_args = expand_tilde_tokens(args);

    std::vector<std::string> final_args;
    final_args.reserve(tilde_expanded_args.size() * 2);

    bool is_double_bracket_command = !tilde_expanded_args.empty() && strip_quote_tag(tilde_expanded_args[0]) == "[[";

    for (const auto& raw_arg : tilde_expanded_args) {
        const bool is_single = is_single_quoted_token(raw_arg);
        const bool is_double = is_double_quoted_token(raw_arg);
        std::string arg = strip_quote_tag(raw_arg);
        if (!is_single && !is_double && !is_double_bracket_command) {
            auto gw = expand_wildcards(arg);
            final_args.insert(final_args.end(), gw.begin(), gw.end());
        } else {
            final_args.push_back(arg);
        }
    }
    return final_args;
}

std::vector<std::string> Parser::expand_braces(const std::string& pattern) {
    std::vector<std::string> result;

    static const size_t MAX_EXPANSION_SIZE = 10000000;

    size_t open_pos = pattern.find('{');
    if (open_pos == std::string::npos) {
        result.push_back(pattern);
        return result;
    }

    int depth = 1;
    size_t close_pos = open_pos + 1;

    while (close_pos < pattern.size() && depth > 0) {
        if (pattern[close_pos] == '{') {
            depth++;
        } else if (pattern[close_pos] == '}') {
            depth--;
        }

        if (depth > 0) {
            close_pos++;
        }
    }

    if (depth != 0) {
        result.push_back(pattern);
        return result;
    }

    std::string prefix = pattern.substr(0, open_pos);
    std::string content = pattern.substr(open_pos + 1, close_pos - open_pos - 1);
    std::string suffix = pattern.substr(close_pos + 1);

    if (content.empty() && prefix.empty() && suffix.empty()) {
        result.push_back(pattern);
        return result;
    }

    size_t range_pos = content.find("..");
    if (range_pos != std::string::npos) {
        std::string start_str = content.substr(0, range_pos);
        std::string end_str = content.substr(range_pos + 2);

        try {
            int start = std::stoi(start_str);
            int end = std::stoi(end_str);

            size_t range_size = std::abs(end - start) + 1;

            if (range_size > MAX_EXPANSION_SIZE) {
                result.push_back(pattern);
                return result;
            }

            result.reserve(range_size);

            if (start <= end) {
                for (int i = start; i <= end; ++i) {
                    std::vector<std::string> expanded_results = expand_braces(prefix + std::to_string(i) + suffix);
                    result.insert(result.end(), std::make_move_iterator(expanded_results.begin()),
                                  std::make_move_iterator(expanded_results.end()));
                }
            } else {
                for (int i = start; i >= end; --i) {
                    std::vector<std::string> expanded_results = expand_braces(prefix + std::to_string(i) + suffix);
                    result.insert(result.end(), std::make_move_iterator(expanded_results.begin()),
                                  std::make_move_iterator(expanded_results.end()));
                }
            }
            return result;
        } catch (const std::exception&) {
            if (start_str.length() == 1 && end_str.length() == 1 && std::isalpha(start_str[0]) && std::isalpha(end_str[0])) {
                char start_char = start_str[0];
                char end_char = end_str[0];

                size_t char_range_size = std::abs(end_char - start_char) + 1;

                if (char_range_size > MAX_EXPANSION_SIZE) {
                    result.push_back(pattern);
                    return result;
                }

                result.reserve(char_range_size);

                if (start_char <= end_char) {
                    for (char c = start_char; c <= end_char; ++c) {
                        std::vector<std::string> expanded_results = expand_braces(prefix + std::string(1, c) + suffix);
                        result.insert(result.end(), expanded_results.begin(), expanded_results.end());
                    }
                } else {
                    for (char c = start_char; c >= end_char; --c) {
                        std::vector<std::string> expanded_results = expand_braces(prefix + std::string(1, c) + suffix);
                        result.insert(result.end(), std::make_move_iterator(expanded_results.begin()),
                                      std::make_move_iterator(expanded_results.end()));
                    }
                }
                return result;
            }
        }
    }

    std::vector<std::string> options;
    size_t start = 0;
    depth = 0;

    for (size_t i = 0; i <= content.size(); ++i) {
        if (i == content.size() || (content[i] == ',' && depth == 0)) {
            options.emplace_back(content.substr(start, i - start));
            start = i + 1;
        } else if (content[i] == '{') {
            depth++;
        } else if (content[i] == '}') {
            depth--;
        }
    }

    if (options.size() > MAX_EXPANSION_SIZE) {
        result.push_back(pattern);
        return result;
    }

    result.reserve(options.size());

    for (const auto& option : options) {
        std::vector<std::string> expanded_results = expand_braces(prefix + option + suffix);
        result.insert(result.end(), std::make_move_iterator(expanded_results.begin()), std::make_move_iterator(expanded_results.end()));
    }

    return result;
}

std::string Parser::get_variable_value(const std::string& var_name) {
    if (shell && shell->get_shell_script_interpreter()) {
        std::string result = shell->get_shell_script_interpreter()->get_variable_value(var_name);
        return result;
    }

    const char* env_val = getenv(var_name.c_str());
    std::string result = env_val ? env_val : "";
    return result;
}

std::string Parser::get_exported_variable_value(const std::string& var_name) {
    if (var_name == "?" || var_name == "$" || var_name == "#" || var_name == "*" || var_name == "@" || var_name == "!") {
        if (shell && shell->get_shell_script_interpreter()) {
            return shell->get_shell_script_interpreter()->get_variable_value(var_name);
        }
    }

    if (var_name.length() == 1 && isdigit(var_name[0])) {
        if (shell && shell->get_shell_script_interpreter()) {
            return shell->get_shell_script_interpreter()->get_variable_value(var_name);
        }
    }

    const char* env_val = getenv(var_name.c_str());
    std::string result = env_val ? env_val : "";
    return result;
}

std::string Parser::resolve_parameter_value(const std::string& var_name) {
    if (var_name.empty()) {
        return "";
    }

    if (var_name == "?") {
        const char* status_env = getenv("?");
        return status_env ? status_env : "0";
    }

    if (var_name == "$") {
        return std::to_string(getpid());
    }

    if (var_name == "#") {
        if (shell) {
            return std::to_string(shell->get_positional_parameter_count());
        }
        return "0";
    }

    if (var_name == "*" || var_name == "@") {
        if (shell) {
            auto params = shell->get_positional_parameters();
            std::string joined;
            for (size_t i = 0; i < params.size(); ++i) {
                if (i > 0) {
                    joined += " ";
                }
                joined += params[i];
            }
            return joined;
        }
        return "";
    }

    if (var_name == "!") {
        const char* last_bg_pid = getenv("!");
        if (last_bg_pid) {
            return last_bg_pid;
        }

        pid_t last_pid = JobManager::instance().get_last_background_pid();
        if (last_pid > 0) {
            return std::to_string(last_pid);
        }
        return "";
    }

    if (!var_name.empty() && std::isdigit(static_cast<unsigned char>(var_name[0])) && var_name.length() == 1) {
        std::string value = get_variable_value(var_name);
        if (!value.empty()) {
            return value;
        }

        if (shell) {
            auto params = shell->get_positional_parameters();
            int param_num = var_name[0] - '0';
            if (param_num > 0 && static_cast<size_t>(param_num - 1) < params.size()) {
                return params[param_num - 1];
            }
        }

        auto it = env_vars.find(var_name);
        if (it != env_vars.end()) {
            return it->second;
        }
        return "";
    }

    std::string value = get_variable_value(var_name);
    if (value.empty()) {
        auto it = env_vars.find(var_name);
        if (it != env_vars.end()) {
            value = it->second;
        }
    }
    return value;
}

void Parser::expand_env_vars(std::string& arg) {
    std::string result;
    result.reserve(arg.length() * 1.5);
    bool in_var = false;
    std::string var_name;
    var_name.reserve(64);

    for (size_t i = 0; i < arg.length(); ++i) {
        if (arg[i] == '$' && i + 2 < arg.length() && arg[i + 1] == '(' && arg[i + 2] == '(') {
            size_t start = i + 3;
            size_t paren_depth = 1;
            size_t end = start;

            while (end < arg.length() && paren_depth > 0) {
                if (arg[end] == '(' && end + 1 < arg.length() && arg[end + 1] == '(') {
                    paren_depth++;
                    end += 2;
                } else if (arg[end] == ')' && end + 1 < arg.length() && arg[end + 1] == ')') {
                    paren_depth--;
                    if (paren_depth == 0) {
                        break;
                    }
                    end += 2;
                } else {
                    end++;
                }
            }

            if (paren_depth == 0 && end + 1 < arg.length()) {
                std::string expr = arg.substr(start, end - start);
                std::string expanded_expr = expr;
                expand_env_vars(expanded_expr);

                try {
                    long long arithmetic_result = evaluate_arithmetic(expanded_expr);
                    std::string result_str = std::to_string(arithmetic_result);
                    result += result_str;
                    i = end + 1;
                    continue;
                } catch (const std::exception& e) {
                    result += arg.substr(i, end - i + 2);
                    i = end + 1;
                    continue;
                }
            } else {
                result += arg[i];
                continue;
            }
        }

        if (arg[i] == '$' && i > 0 && arg[i - 1] == '\\') {
            if (!result.empty() && result.back() == '\\') {
                result.pop_back();
            }
            result += '$';
            continue;
        }

        if (arg[i] == '$' && i + 1 < arg.length() && arg[i + 1] == '{') {
            size_t start = i + 2;
            size_t brace_depth = 1;
            size_t end = start;

            while (end < arg.length() && brace_depth > 0) {
                if (arg[end] == '{') {
                    brace_depth++;
                } else if (arg[end] == '}') {
                    brace_depth--;
                }
                if (brace_depth > 0)
                    end++;
            }

            if (brace_depth == 0 && end < arg.length()) {
                std::string param_expr = arg.substr(start, end - start);
                std::string value;

                size_t colon_pos = param_expr.find(':');
                size_t dash_pos = param_expr.find('-', colon_pos != std::string::npos ? colon_pos + 1 : 0);

                if (colon_pos != std::string::npos && dash_pos != std::string::npos) {
                    std::string var_name = param_expr.substr(0, colon_pos);
                    std::string default_val = param_expr.substr(dash_pos + 1);

                    std::string env_val = get_variable_value(var_name);
                    if (!env_val.empty()) {
                        value = env_val;
                    } else {
                        expand_env_vars(default_val);
                        value = default_val;
                    }
                } else if (colon_pos == std::string::npos && param_expr.find('-') != std::string::npos) {
                    size_t dash_pos = param_expr.find('-');
                    std::string var_name = param_expr.substr(0, dash_pos);
                    std::string default_val = param_expr.substr(dash_pos + 1);

                    std::string env_val = get_variable_value(var_name);
                    if (!env_val.empty()) {
                        value = env_val;
                    } else {
                        expand_env_vars(default_val);
                        value = default_val;
                    }
                } else {
                    if (shell && shell->get_shell_script_interpreter()) {
                        try {
                            value = shell->get_shell_script_interpreter()->expand_parameter_expression(param_expr);
                        } catch (const std::runtime_error& e) {
                            std::string error_msg = e.what();
                            if (error_msg.find("parameter null or not set") != std::string::npos ||
                                error_msg.find("parameter not set") != std::string::npos) {
                                value = get_variable_value(param_expr);
                            } else {
                                value = get_variable_value(param_expr);
                            }
                        } catch (...) {
                            value = get_variable_value(param_expr);
                        }
                    } else {
                        value = get_variable_value(param_expr);
                    }
                }

                result += value;
                i = end;
                continue;
            } else {
            }
        }

        if (in_var) {
            if (isalnum(arg[i]) || arg[i] == '_' || (var_name.empty() && isdigit(arg[i])) ||
                (var_name.empty() &&
                 (arg[i] == '?' || arg[i] == '$' || arg[i] == '#' || arg[i] == '*' || arg[i] == '@' || arg[i] == '!'))) {
                var_name += arg[i];
            } else {
                in_var = false;
                std::string value;

                auto read_default_value = [&](size_t steps_to_advance) {
                    for (size_t step = 0; step < steps_to_advance && i < arg.length(); ++step) {
                        i++;
                    }

                    std::string default_val;
                    while (i < arg.length() && !isspace(arg[i])) {
                        default_val += arg[i];
                        i++;
                    }
                    if (i < arg.length() && isspace(arg[i])) {
                        i--;
                    }
                    expand_env_vars(default_val);
                    return default_val;
                };

                if (arg[i] == ':' && i + 1 < arg.length() && arg[i + 1] == '-') {
                    std::string env_val = get_variable_value(var_name);
                    if (!env_val.empty()) {
                        value = env_val;

                        i++;
                        i++;

                        while (i < arg.length() && !isspace(arg[i])) {
                            i++;
                        }
                        if (i < arg.length() && isspace(arg[i])) {
                            i--;
                        }
                    } else {
                        value = read_default_value(2);
                    }
                } else if (arg[i] == '-' && i >= 1) {
                    std::string env_val = get_variable_value(var_name);
                    if (!env_val.empty()) {
                        value = env_val;

                        i++;
                        while (i < arg.length() && !isspace(arg[i])) {
                            i++;
                        }
                        if (i < arg.length() && isspace(arg[i])) {
                            i--;
                        }
                    } else {
                        value = read_default_value(1);
                    }
                } else {
                    value = resolve_parameter_value(var_name);
                }
                result += value;

                if (arg[i] != '$' && !(arg[i] == ':' && i + 1 < arg.length() && arg[i + 1] == '-') && arg[i] != '-') {
                    result += arg[i];
                } else if (arg[i] == '$') {
                    i--;
                }
            }
        } else if (arg[i] == '$' && (i + 1 < arg.length()) &&
                   (isalpha(arg[i + 1]) || arg[i + 1] == '_' || isdigit(arg[i + 1]) || arg[i + 1] == '?' || arg[i + 1] == '$' ||
                    arg[i + 1] == '#' || arg[i + 1] == '*' || arg[i + 1] == '@' || arg[i + 1] == '!')) {
            in_var = true;
            var_name.clear();
            continue;
        } else {
            result += arg[i];
        }
    }

    if (in_var) {
        result += resolve_parameter_value(var_name);
    }

    arg = result;
}

void Parser::expand_env_vars_selective(std::string& arg) {
    const std::string start_marker = "\x1E__NOENV_START__\x1E";
    const std::string end_marker = "\x1E__NOENV_END__\x1E";

    std::string result;
    result.reserve(arg.length() * 1.5);

    size_t pos = 0;
    while (pos < arg.length()) {
        size_t start_pos = arg.find(start_marker, pos);

        if (start_pos == std::string::npos) {
            std::string remaining = arg.substr(pos);
            expand_env_vars(remaining);
            result += remaining;
            break;
        }

        std::string before_marker = arg.substr(pos, start_pos - pos);
        expand_env_vars(before_marker);
        result += before_marker;

        size_t end_pos = arg.find(end_marker, start_pos + start_marker.length());
        if (end_pos == std::string::npos) {
            result += arg.substr(start_pos);
            break;
        }

        size_t content_start = start_pos + start_marker.length();
        size_t content_length = end_pos - content_start;
        result += arg.substr(content_start, content_length);

        pos = end_pos + end_marker.length();
    }

    arg = result;
}

void Parser::expand_exported_env_vars_only(std::string& arg) {
    std::string result;
    result.reserve(arg.length() * 1.5);
    bool in_var = false;
    std::string var_name;
    var_name.reserve(64);

    for (size_t i = 0; i < arg.length(); ++i) {
        if (arg[i] == '$' && i + 2 < arg.length() && arg[i + 1] == '(' && arg[i + 2] == '(') {
            size_t start = i + 3;
            size_t paren_depth = 1;
            size_t end = start;

            while (end < arg.length() && paren_depth > 0) {
                if (arg[end] == '(' && end + 1 < arg.length() && arg[end + 1] == '(') {
                    paren_depth++;
                    end += 2;
                } else if (arg[end] == ')' && end + 1 < arg.length() && arg[end + 1] == ')') {
                    paren_depth--;
                    if (paren_depth == 0) {
                        break;
                    }
                    end += 2;
                } else {
                    end++;
                }
            }

            if (paren_depth == 0 && end + 1 < arg.length()) {
                std::string expr = arg.substr(start, end - start);
                std::string expanded_expr = expr;
                expand_exported_env_vars_only(expanded_expr);

                try {
                    long long arithmetic_result = evaluate_arithmetic(expanded_expr);
                    result += std::to_string(arithmetic_result);
                } catch (const std::exception& e) {
                    result += "0";
                }

                i = end + 1;
                continue;
            }
        }

        if (arg[i] == '$' && !in_var) {
            in_var = true;
            var_name.clear();
        } else if (in_var) {
            if (arg[i] == '{') {
                size_t brace_start = i + 1;
                size_t brace_end = brace_start;
                int brace_depth = 1;

                while (brace_end < arg.length() && brace_depth > 0) {
                    if (arg[brace_end] == '{') {
                        brace_depth++;
                    } else if (arg[brace_end] == '}') {
                        brace_depth--;
                    }
                    if (brace_depth > 0)
                        brace_end++;
                }

                if (brace_depth == 0) {
                    std::string param_expr = arg.substr(brace_start, brace_end - brace_start);

                    std::string value;
                    if (param_expr.find(":-") != std::string::npos) {
                        size_t sep_pos = param_expr.find(":-");
                        std::string var_name = param_expr.substr(0, sep_pos);
                        std::string default_val = param_expr.substr(sep_pos + 2);

                        std::string env_val = get_exported_variable_value(var_name);
                        if (env_val.empty()) {
                            expand_exported_env_vars_only(default_val);
                            value = default_val;
                        } else {
                            value = env_val;
                        }
                    } else if (param_expr.find(":=") != std::string::npos) {
                        size_t sep_pos = param_expr.find(":=");
                        std::string var_name = param_expr.substr(0, sep_pos);
                        std::string default_val = param_expr.substr(sep_pos + 2);

                        std::string env_val = get_exported_variable_value(var_name);
                        if (env_val.empty()) {
                            expand_exported_env_vars_only(default_val);
                            value = default_val;

                        } else {
                            value = env_val;
                        }
                    } else {
                        value = get_exported_variable_value(param_expr);
                    }

                    result += value;
                    i = brace_end;
                    in_var = false;
                } else {
                    result += arg[i];
                }
            } else if (std::isalnum(arg[i]) || arg[i] == '_' || arg[i] == '?' || arg[i] == '$' || arg[i] == '#' || arg[i] == '*' ||
                       arg[i] == '@' || arg[i] == '!' || std::isdigit(arg[i])) {
                var_name += arg[i];
            } else {
                std::string value = get_exported_variable_value(var_name);
                result += value;

                if (arg[i] != '$' && !(arg[i] == ':' && i + 1 < arg.length() && arg[i + 1] == '-') && arg[i] != '-') {
                    result += arg[i];
                } else if (arg[i] == '$') {
                    i--;
                }
                in_var = false;
                var_name.clear();
            }
        } else {
            result += arg[i];
        }
    }

    if (in_var) {
        std::string value = get_exported_variable_value(var_name);
        result += value;
    }

    arg = result;
}

std::vector<std::string> Parser::parse_command_exported_vars_only(const std::string& cmdline) {
    bool saved_flag = use_exported_vars_only;
    use_exported_vars_only = true;

    std::vector<std::string> result = parse_command(cmdline);

    use_exported_vars_only = saved_flag;

    return result;
}

std::vector<std::string> Parser::split_by_ifs(const std::string& input) {
    std::vector<std::string> result;

    std::string ifs = " \t\n";
    if (shell) {
        const auto& env_vars = shell->get_env_vars();
        auto it = env_vars.find("IFS");
        if (it != env_vars.end()) {
            ifs = it->second;
        } else {
            const char* ifs_env = getenv("IFS");
            if (ifs_env) {
                ifs = ifs_env;
            }
        }
    } else {
        const char* ifs_env = getenv("IFS");
        if (ifs_env) {
            ifs = ifs_env;
        }
    }

    if (input.empty()) {
        return result;
    }

    auto looks_like_assignment = [](const std::string& value) -> bool {
        size_t equals_pos = value.find('=');
        if (equals_pos == std::string::npos || equals_pos == 0) {
            return false;
        }

        size_t name_end = equals_pos;
        if (name_end > 0 && value[name_end - 1] == '+') {
            name_end--;
        }

        if (name_end == 0) {
            return false;
        }

        auto is_name_char = [](unsigned char ch) { return std::isalnum(ch) || ch == '_'; };

        unsigned char first = static_cast<unsigned char>(value[0]);
        if (!(std::isalpha(first) || first == '_')) {
            return false;
        }

        for (size_t i = 1; i < name_end; ++i) {
            if (!is_name_char(static_cast<unsigned char>(value[i]))) {
                return false;
            }
        }
        return true;
    };

    if (looks_like_assignment(input)) {
        result.push_back(input);
        return result;
    }

    if (ifs.empty()) {
        result.push_back(input);
        return result;
    }

    std::string current_word;
    bool in_word = false;
    int process_depth = 0;
    bool in_single = false;
    bool in_double = false;

    size_t idx = 0;
    while (idx < input.size()) {
        char c = input[idx];

        if (process_depth == 0) {
            if ((c == '<' || c == '>') && idx + 1 < input.size() && input[idx + 1] == '(') {
                if (!in_word) {
                    current_word.clear();
                }
                current_word += c;
                current_word += '(';
                in_word = true;
                process_depth = 1;
                idx += 2;
                continue;
            }

            if (ifs.find(c) != std::string::npos) {
                if (in_word) {
                    result.push_back(current_word);
                    current_word.clear();
                    in_word = false;
                }
                idx++;
                continue;
            }
        } else {
            if (!in_double && c == '\'' && (idx == 0 || input[idx - 1] != '\\')) {
                in_single = !in_single;
                current_word += c;
                idx++;
                continue;
            }
            if (!in_single && c == '"' && (idx == 0 || input[idx - 1] != '\\')) {
                in_double = !in_double;
                current_word += c;
                idx++;
                continue;
            }
            if (!in_single && !in_double) {
                if (c == '(') {
                    process_depth++;
                } else if (c == ')') {
                    process_depth--;
                    if (process_depth == 0) {
                        in_single = false;
                        in_double = false;
                    }
                }
            }
            if (c == '\\' && !in_single && idx + 1 < input.size()) {
                current_word += c;
                current_word += input[idx + 1];
                idx += 2;
                continue;
            }
        }

        current_word += c;
        in_word = true;
        idx++;
    }

    if (in_word) {
        result.push_back(current_word);
    }

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
        } else if (!delimiters.in_quotes && command[i] == '[' && i + 1 < command.length() && command[i + 1] == '[') {
            delimiters.bracket_depth++;
            current += command[i];
            current += command[i + 1];
            i++;
        } else if (!delimiters.in_quotes && command[i] == ']' && i + 1 < command.length() && command[i + 1] == ']' &&
                   delimiters.bracket_depth > 0) {
            delimiters.bracket_depth--;
            current += command[i];
            current += command[i + 1];
            i++;
        } else if (command[i] == '|' && !delimiters.in_quotes && delimiters.paren_depth == 0 && delimiters.bracket_depth == 0) {
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
        std::string trimmed = cmd_part;

        trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

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
                    if (i >= 2 && trimmed[i - 2] == '$' && trimmed[i - 1] == '(' && trimmed[i] == '(') {
                        arith_depth++;
                    }

                    else if (i + 1 < trimmed.length() && trimmed[i] == ')' && trimmed[i + 1] == ')' && arith_depth > 0) {
                        arith_depth--;
                        i++;
                    }

                    else if (i + 1 < trimmed.length() && trimmed[i] == '[' && trimmed[i + 1] == '[') {
                        bracket_depth++;
                        i++;
                    }

                    else if (i + 1 < trimmed.length() && trimmed[i] == ']' && trimmed[i + 1] == ']' && bracket_depth > 0) {
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
                    while (next_non_ws < cmd_part.length() && std::isspace(cmd_part[next_non_ws])) {
                        next_non_ws++;
                    }

                    if (next_non_ws < cmd_part.length() && cmd_part[next_non_ws] == '>') {
                        is_background = false;
                    } else {
                        is_background = true;
                    }
                } else {
                    is_background = true;
                }
            }
        }

        if (is_background) {
            cmd.background = true;
            cmd_part = trimmed.substr(0, trimmed.length() - 1);
            cmd_part.erase(cmd_part.find_last_not_of(" \t\n\r") + 1);
        }

        if (!cmd_part.empty()) {
            size_t lead = cmd_part.find_first_not_of(" \t\r\n");
            if (lead != std::string::npos && cmd_part[lead] == '(') {
                size_t close_paren = cmd_part.find(')', lead + 1);
                if (close_paren != std::string::npos) {
                    std::string subshell_content = cmd_part.substr(lead + 1, close_paren - (lead + 1));
                    std::string remaining = cmd_part.substr(close_paren + 1);

                    cmd.args.push_back("__INTERNAL_SUBSHELL__");
                    cmd.args.push_back(subshell_content);

                    if (!remaining.empty()) {
                        std::vector<std::string> redir_tokens = tokenize_command(remaining);
                        std::vector<std::string> merged_redir = merge_redirection_tokens(redir_tokens);

                        for (size_t i = 0; i < merged_redir.size(); ++i) {
                            const std::string tok = strip_quote_tag(merged_redir[i]);
                            if (tok == "2>&1") {
                                cmd.stderr_to_stdout = true;
                            } else if (tok == ">&2") {
                                cmd.stdout_to_stderr = true;
                            } else if ((tok == "2>" || tok == "2>>") && i + 1 < merged_redir.size()) {
                                cmd.stderr_file = strip_quote_tag(merged_redir[++i]);
                                cmd.stderr_append = (tok == "2>>");
                            }
                        }
                    }

                    commands.push_back(cmd);
                    continue;
                }
            }
        }

        std::vector<std::string> raw_tokens = tokenize_command(cmd_part);
        std::vector<std::string> tokens = merge_redirection_tokens(raw_tokens);
        std::vector<std::string> filtered_args;

        auto is_all_digits = [](const std::string& s) {
            return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char ch) { return std::isdigit(ch); });
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
            const std::string tok = strip_quote_tag(tokens[i]);
            if ((tok == "<" || tok == ">" || tok == ">>" || tok == ">|" || tok == "&>" || tok == "<<" || tok == "<<-" || tok == "<<<" ||
                 tok == "2>" || tok == "2>>") &&
                (i + 1 >= tokens.size())) {
                throw std::runtime_error("cjsh: syntax error near unexpected token `newline'");
            }
        }

        for (size_t i = 0; i < tokens.size(); ++i) {
            const std::string tok = strip_quote_tag(tokens[i]);
            if (tok == "<" && i + 1 < tokens.size()) {
                cmd.input_file = strip_quote_tag(tokens[++i]);
            } else if (tok == ">" && i + 1 < tokens.size()) {
                cmd.output_file = strip_quote_tag(tokens[++i]);
            } else if (tok == ">>" && i + 1 < tokens.size()) {
                cmd.append_file = strip_quote_tag(tokens[++i]);
            } else if (tok == ">|" && i + 1 < tokens.size()) {
                cmd.output_file = strip_quote_tag(tokens[++i]);
                cmd.force_overwrite = true;
            } else if (tok == "&>" && i + 1 < tokens.size()) {
                cmd.both_output_file = strip_quote_tag(tokens[++i]);
                cmd.both_output = true;
            } else if ((tok == "<<" || tok == "<<-") && i + 1 < tokens.size()) {
                cmd.here_doc = strip_quote_tag(tokens[++i]);
            } else if (tok == "<<<" && i + 1 < tokens.size()) {
                cmd.here_string = strip_quote_tag(tokens[++i]);
            } else if ((tok == "2>" || tok == "2>>") && i + 1 < tokens.size()) {
                cmd.stderr_file = strip_quote_tag(tokens[++i]);
                cmd.stderr_append = (tok == "2>>");
            } else if (tok == "2>&1") {
                cmd.stderr_to_stdout = true;
            } else if (tok == ">&2") {
                cmd.stdout_to_stderr = true;
            } else if (handle_fd_duplication_token(tok)) {
                continue;
            } else if (tok.find("<") == tok.length() - 1 && tok.length() > 1 && std::isdigit(tok[0]) && i + 1 < tokens.size()) {
                try {
                    int fd = std::stoi(tok.substr(0, tok.length() - 1));
                    std::string file = strip_quote_tag(tokens[++i]);
                    cmd.fd_redirections[fd] = "input:" + file;
                } catch (const std::exception&) {
                    filtered_args.push_back(tokens[i]);
                }
            } else if (tok.find(">") == tok.length() - 1 && tok.length() > 1 && std::isdigit(tok[0]) && i + 1 < tokens.size()) {
                try {
                    int fd = std::stoi(tok.substr(0, tok.length() - 1));
                    std::string file = strip_quote_tag(tokens[++i]);
                    cmd.fd_redirections[fd] = "output:" + file;
                } catch (const std::exception&) {
                    filtered_args.push_back(tokens[i]);
                }
            } else {
                if ((tok.find("<(") == 0 && tok.back() == ')') || (tok.find(">(") == 0 && tok.back() == ')')) {
                    cmd.process_substitutions.push_back(tok);
                    filtered_args.push_back(tokens[i]);
                } else {
                    filtered_args.push_back(tokens[i]);
                }
            }
        }

        bool is_double_bracket_cmd = !filtered_args.empty() && strip_quote_tag(filtered_args[0]) == "[[";

        auto tilde_expanded_args = expand_tilde_tokens(filtered_args);

        std::vector<std::string> final_args_local;
        bool is_subshell_command = !filtered_args.empty() && strip_quote_tag(filtered_args[0]) == "__INTERNAL_SUBSHELL__";

        for (size_t arg_idx = 0; arg_idx < tilde_expanded_args.size(); ++arg_idx) {
            const auto& raw = tilde_expanded_args[arg_idx];
            bool is_single = is_single_quoted_token(raw);
            bool is_double = is_double_quoted_token(raw);
            std::string val = strip_quote_tag(raw);

            if (!is_single && !(is_subshell_command && arg_idx == 1)) {
                try {
                    expand_env_vars(val);
                } catch (const std::runtime_error&) {
                }
            }

            if (!is_single && !is_double && val.find('{') != std::string::npos && val.find('}') != std::string::npos) {
                std::vector<std::string> brace_expansions = expand_braces(val);
                for (const auto& expanded_val : brace_expansions) {
                    if (!is_double_bracket_cmd && expanded_val.find_first_of("*?[]") != std::string::npos) {
                        auto wildcard_expanded = expand_wildcards(expanded_val);
                        final_args_local.insert(final_args_local.end(), wildcard_expanded.begin(), wildcard_expanded.end());
                    } else {
                        final_args_local.push_back(expanded_val);
                    }
                }
            } else if (!is_single && !is_double && !is_double_bracket_cmd && val.find_first_of("*?[]") != std::string::npos) {
                auto expanded = expand_wildcards(val);
                final_args_local.insert(final_args_local.end(), expanded.begin(), expanded.end());
            } else {
                final_args_local.push_back(val);
            }
        }

        cmd.args = final_args_local;

        if (const char* home = std::getenv("HOME")) {
            expand_command_paths_with_home(cmd, std::string(home));
        }

        if (command_validation_enabled && !cmd.args.empty() && should_validate_command(cmd.args[0]) && !is_valid_command(cmd.args[0])) {
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

    {
        const std::string& pt = preprocessed.processed_text;
        size_t lead = pt.find_first_not_of(" \t\r\n");
        if (lead != std::string::npos && pt.find("SUBSHELL{", lead) == lead) {
            size_t start = preprocessed.processed_text.find('{') + 1;
            size_t end = preprocessed.processed_text.find('}', start);
            if (end != std::string::npos) {
                std::string subshell_content = preprocessed.processed_text.substr(start, end - start);
                std::string remaining = preprocessed.processed_text.substr(end + 1);

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

                std::string rebuilt = "__INTERNAL_SUBSHELL__ \"" + escape_double_quotes(subshell_content) + "\"" + remaining;

                std::string prefix = preprocessed.processed_text.substr(0, lead);
                preprocessed.processed_text = prefix + rebuilt;
            }
        }
    }

    std::vector<Command> commands = parse_pipeline(preprocessed.processed_text);

    for (auto& cmd : commands) {
        if (!cmd.input_file.empty() && cmd.input_file.find("HEREDOC_PLACEHOLDER_") == 0) {
            auto it = current_here_docs.find(cmd.input_file);
            if (it != current_here_docs.end()) {
                std::string content = it->second;

                if (content.length() >= 10 && content.substr(0, 10) == "__EXPAND__") {
                    content = content.substr(10);

                    expand_env_vars(content);
                    expand_command_substitutions_in_string(content);
                }

                strip_subst_literal_markers(content);

                cmd.here_doc = content;
                cmd.input_file.clear();
            }
        }

        if (!cmd.here_doc.empty() && current_here_docs.count(cmd.here_doc)) {
            std::string content = current_here_docs[cmd.here_doc];

            if (content.length() >= 10 && content.substr(0, 10) == "__EXPAND__") {
                content = content.substr(10);

                expand_env_vars(content);
                expand_command_substitutions_in_string(content);
            }

            strip_subst_literal_markers(content);

            cmd.here_doc = content;
        }
    }

    return commands;
}

bool Parser::is_env_assignment(const std::string& command, std::string& var_name, std::string& var_value) {
    size_t equals_pos = command.find('=');
    if (equals_pos == std::string::npos || equals_pos == 0) {
        return false;
    }

    std::string name_part = command.substr(0, equals_pos);

    size_t name_start = name_part.find_first_not_of(" \t\n\r");
    if (name_start == std::string::npos) {
        return false;
    }
    name_part = name_part.substr(name_start);

    if (name_part.empty() || (!std::isalpha(name_part[0]) && name_part[0] != '_')) {
        return false;
    }
    for (size_t i = 1; i < name_part.length(); ++i) {
        if (!std::isalnum(name_part[i]) && name_part[i] != '_') {
            return false;
        }
    }

    var_name = name_part;
    var_value = command.substr(equals_pos + 1);

    if (ReadonlyManager::instance().is_readonly(var_name)) {
        std::cerr << "cjsh: " << var_name << ": readonly variable" << std::endl;
        return false;
    }

    if (var_value.size() >= 2) {
        if ((var_value.front() == '"' && var_value.back() == '"') || (var_value.front() == '\'' && var_value.back() == '\'')) {
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

    for (size_t i = 0; i < command.length(); ++i) {
        if (delimiters.update_quote(command[i])) {
            current += command[i];
        } else if (!delimiters.in_quotes && command[i] == '(') {
            if (i >= 2 && command[i - 2] == '$' && command[i - 1] == '(' && command[i] == '(') {
                arith_depth++;
            }
            delimiters.paren_depth++;
            current += command[i];
        } else if (!delimiters.in_quotes && command[i] == ')') {
            delimiters.paren_depth--;

            if (delimiters.paren_depth >= 0 && i + 1 < command.length() && command[i + 1] == ')' && arith_depth > 0) {
                arith_depth--;
                current += command[i];
                current += command[i + 1];
                i++;
            } else {
                current += command[i];
            }
        } else if (!delimiters.in_quotes && command[i] == '[' && i + 1 < command.length() && command[i + 1] == '[') {
            delimiters.bracket_depth++;
            current += command[i];
            current += command[i + 1];
            i++;
        } else if (!delimiters.in_quotes && command[i] == ']' && i + 1 < command.length() && command[i + 1] == ']' &&
                   delimiters.bracket_depth > 0) {
            delimiters.bracket_depth--;
            current += command[i];
            current += command[i + 1];
            i++;
        } else if (!delimiters.in_quotes && delimiters.paren_depth == 0 && arith_depth == 0 && delimiters.bracket_depth == 0 &&
                   i < command.length() - 1) {
            if (command[i] == '&' && command[i + 1] == '&') {
                if (!current.empty()) {
                    logical_commands.push_back({current, "&&"});
                    current.clear();
                }
                i++;
            } else if (command[i] == '|' && command[i + 1] == '|') {
                if (!current.empty()) {
                    logical_commands.push_back({current, "||"});
                    current.clear();
                }
                i++;
            } else {
                current += command[i];
            }
        } else {
            current += command[i];
        }
    }

    if (!current.empty()) {
        logical_commands.push_back({current, ""});
    }

    return logical_commands;
}

std::vector<std::string> Parser::parse_semicolon_commands(const std::string& command) {
    std::vector<std::string> commands;
    std::string current;
    DelimiterState scan_state;
    int control_depth = 0;

    std::vector<bool> is_semicolon_split_point(command.length(), false);

    for (size_t i = 0; i < command.length(); ++i) {
        if (scan_state.update_quote(command[i])) {
            continue;
        } else if (!scan_state.in_quotes && command[i] == '(') {
            scan_state.paren_depth++;
        } else if (!scan_state.in_quotes && command[i] == ')') {
            scan_state.paren_depth--;
        } else if (!scan_state.in_quotes && command[i] == '{') {
            scan_state.brace_depth++;
        } else if (!scan_state.in_quotes && command[i] == '}') {
            scan_state.brace_depth--;
        } else if (!scan_state.in_quotes && scan_state.paren_depth == 0 && scan_state.brace_depth == 0) {
            if (command[i] == ' ' || command[i] == '\t' || i == 0) {
                size_t word_start = i;
                if (command[i] == ' ' || command[i] == '\t')
                    word_start = i + 1;

                std::string word;
                size_t j = word_start;
                while (j < command.length() && std::isalpha(command[j])) {
                    word += command[j];
                    j++;
                }

                if (word == "if" || word == "for" || word == "while" || word == "until" || word == "case") {
                    control_depth++;
                } else if ((word == "fi" || word == "done" || word == "esac") && control_depth > 0) {
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
        } else if (command[i] == ';' && is_semicolon_split_point[i]) {
            if (!current.empty()) {
                current.erase(0, current.find_first_not_of(" \t\n\r"));
                current.erase(current.find_last_not_of(" \t\n\r") + 1);
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
        current.erase(0, current.find_first_not_of(" \t\n\r"));
        current.erase(current.find_last_not_of(" \t\n\r") + 1);
        if (!current.empty()) {
            commands.push_back(current);
        }
    }

    return commands;
}

std::vector<std::string> Parser::expand_wildcards(const std::string& pattern) {
    std::vector<std::string> result;

    bool has_wildcards = false;

    for (size_t i = 0; i < pattern.length(); ++i) {
        char c = pattern[i];

        if (c == '\x1F' && i + 1 < pattern.length()) {
            i++;
            continue;
        }

        if (c == '*' || c == '?' || c == '[') {
            has_wildcards = true;
            break;
        }
    }

    std::string unescaped;
    unescaped.reserve(pattern.length());

    for (size_t i = 0; i < pattern.length(); ++i) {
        if (pattern[i] == '\x1F') {
            if (i + 1 < pattern.length()) {
                i++;
                unescaped += pattern[i];
            }
        } else {
            unescaped += pattern[i];
        }
    }

    if (!has_wildcards) {
        result.push_back(unescaped);
        return result;
    }

    glob_t glob_result;
    memset(&glob_result, 0, sizeof(glob_result));

    int return_value = glob(unescaped.c_str(), GLOB_TILDE | GLOB_MARK, NULL, &glob_result);
    if (return_value == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
            result.push_back(std::string(glob_result.gl_pathv[i]));
        }
        globfree(&glob_result);
    } else if (return_value == GLOB_NOMATCH) {
        result.push_back(unescaped);
    }

    return result;
}

bool Parser::should_validate_command(const std::string& command) const {
    size_t equals_pos = command.find('=');
    if (equals_pos != std::string::npos) {
        std::string var_name = command.substr(0, equals_pos);
        if (!var_name.empty()) {
            bool valid_var_name = true;
            for (char c : var_name) {
                if (!std::isalnum(c) && c != '_') {
                    valid_var_name = false;
                    break;
                }
            }
            if (valid_var_name && std::isalpha(var_name[0])) {
                return false;
            }
        }
    }

    if (command == "&&" || command == "||" || command == "|" || command == ";" || command == "(" || command == ")" || command == "{" ||
        command == "}" || command == "if" || command == "then" || command == "else" || command == "elif" || command == "fi" ||
        command == "for" || command == "while" || command == "do" || command == "done" || command == "case" || command == "esac" ||
        command == "function") {
        return false;
    }

    if (command.empty() || command[0] == '>' || command[0] == '<' || command == ">>" || command == "<<" || command == "2>" ||
        command == "2>>" || command == "&>" || command == "&>>") {
        return false;
    }

    return true;
}

bool Parser::is_valid_command(const std::string& command_name) const {
    if (command_name.empty()) {
        return false;
    }

    if (shell && shell->get_built_ins() && shell->get_built_ins()->is_builtin_command(command_name)) {
        return true;
    }

    if (shell) {
        auto aliases = shell->get_aliases();
        if (aliases.find(command_name) != aliases.end()) {
            return true;
        }
    }

    if (shell && shell->get_shell_script_interpreter()) {
        auto* interpreter = shell->get_shell_script_interpreter();
        if (interpreter && interpreter->has_function(command_name)) {
            return true;
        }
    }

    if (command_name.find('/') != std::string::npos) {
        return true;
    } else {
        std::string path = cjsh_filesystem::find_executable_in_path(command_name);
        return !path.empty();
    }
}

std::string Parser::get_command_validation_error(const std::string& command_name) const {
    if (command_name.empty()) {
        return "cjsh: empty command name";
    }

    return "cjsh: command not found: " + command_name;
}

long long Parser::evaluate_arithmetic(const std::string& expr) {
    std::string trimmed = expr;

    size_t start = trimmed.find_first_not_of(" \t\n\r");
    size_t end = trimmed.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return 0;
    }
    trimmed = trimmed.substr(start, end - start + 1);

    auto find_operator = [](const std::string& s, const std::string& op) -> size_t {
        int paren_depth = 0;
        for (size_t i = 0; i <= s.length() - op.length(); ++i) {
            if (s[i] == '(')
                paren_depth++;
            else if (s[i] == ')')
                paren_depth--;
            else if (paren_depth == 0 && s.substr(i, op.length()) == op) {
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
        if (s.empty())
            return 0;
        std::string clean = s;

        size_t start = clean.find_first_not_of(" \t\n\r");
        size_t end = clean.find_last_not_of(" \t\n\r");
        if (start == std::string::npos)
            return 0;
        clean = clean.substr(start, end - start + 1);

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
            if (trimmed[paren_end] == '(')
                depth++;
            else if (trimmed[paren_end] == ')')
                depth--;
            paren_end++;
        }
        if (depth == 0) {
            std::string inner = trimmed.substr(paren_start + 1, paren_end - paren_start - 2);
            long long inner_result = evaluate_arithmetic(inner);
            std::string new_expr = trimmed.substr(0, paren_start) + std::to_string(inner_result) + trimmed.substr(paren_end);
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

            if (op == "+")
                return left_val + right_val;
            else if (op == "-")
                return left_val - right_val;
            else if (op == "*")
                return left_val * right_val;
            else if (op == "/") {
                if (right_val == 0)
                    throw std::runtime_error("Division by zero");
                return left_val / right_val;
            } else if (op == "%") {
                if (right_val == 0)
                    throw std::runtime_error("Division by zero");
                return left_val % right_val;
            }
        }
    }

    return parse_number(trimmed);
}

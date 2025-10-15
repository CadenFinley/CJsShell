#include "shell_script_interpreter_utils.h"

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

namespace shell_script_interpreter::detail {

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
        return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

std::string strip_inline_comment(const std::string& s) {
    bool in_quotes = false;
    bool in_brace_expansion = false;
    char quote = '\0';

    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];

        if (!in_quotes && c == '$' && i + 1 < s.size() && s[i + 1] == '{') {
            in_brace_expansion = true;
        } else if (in_brace_expansion && c == '}') {
            in_brace_expansion = false;
        }

        if (!in_quotes && !in_brace_expansion && c == '$' && i + 1 < s.size()) {
            char next = s[i + 1];
            if (next == '#' || next == '?' || next == '$' || next == '*' || next == '@' ||
                next == '!' || (std::isdigit(static_cast<unsigned char>(next)) != 0)) {
                ++i;
                continue;
            }
        }

        if (c == '"' || c == '\'') {
            size_t backslash_count = 0;
            for (size_t j = i; j > 0; --j) {
                if (s[j - 1] == '\\') {
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
        } else if (!in_quotes && !in_brace_expansion && c == '#') {
            return s.substr(0, i);
        }
    }
    return s;
}

std::string process_line_for_validation(const std::string& line) {
    return trim(strip_inline_comment(line));
}

std::vector<std::string> split_ampersand(const std::string& s) {
    std::vector<std::string> parts;
    bool in_quotes = false;
    char q = '\0';
    int arith_depth = 0;
    int bracket_depth = 0;
    std::string cur;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if ((c == '"' || c == '\'') && (i == 0 || s[i - 1] != '\\')) {
            if (!in_quotes) {
                in_quotes = true;
                q = c;
            } else if (q == c) {
                in_quotes = false;
                q = '\0';
            }
            cur += c;
        } else if (!in_quotes) {
            if (i >= 2 && s[i - 2] == '$' && s[i - 1] == '(' && s[i] == '(') {
                arith_depth++;
                cur += c;
            }

            else if (i + 1 < s.size() && s[i] == ')' && s[i + 1] == ')' && arith_depth > 0) {
                arith_depth--;
                cur += c;
                cur += s[i + 1];
                i++;
            }

            else if (i + 1 < s.size() && s[i] == '[' && s[i + 1] == '[') {
                bracket_depth++;
                cur += c;
                cur += s[i + 1];
                i++;
            }

            else if (i + 1 < s.size() && s[i] == ']' && s[i + 1] == ']' && bracket_depth > 0) {
                bracket_depth--;
                cur += c;
                cur += s[i + 1];
                i++;
            }

            else if (c == '&' && arith_depth == 0 && bracket_depth == 0) {
                if (i + 1 < s.size() && s[i + 1] == '&') {
                    cur += c;
                    cur += s[i + 1];
                    ++i;
                } else if (i > 0 && s[i - 1] == '>' && i + 1 < s.size() &&
                           (std::isdigit(static_cast<unsigned char>(s[i + 1])) != 0 ||
                            s[i + 1] == '-')) {
                    cur += c;
                } else if (i > 0 && s[i - 1] == '<' && i + 1 < s.size() &&
                           (std::isdigit(static_cast<unsigned char>(s[i + 1])) != 0 ||
                            s[i + 1] == '-')) {
                    cur += c;
                } else if (i + 1 < s.size() && s[i + 1] == '>') {
                    cur += c;
                } else {
                    std::string seg = trim(cur);
                    if (!seg.empty() && seg.back() != '&')
                        seg += " &";
                    if (!seg.empty())
                        parts.push_back(seg);
                    cur.clear();
                }
            } else {
                cur += c;
            }
        } else {
            cur += c;
        }
    }
    std::string tail = trim(cur);
    if (!tail.empty())
        parts.push_back(tail);
    return parts;
}

std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool is_readable_file(const std::string& path) {
    struct stat st{};
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && access(path.c_str(), R_OK) == 0;
}

bool is_control_flow_exit_code(int code) {
    return code == 253 || code == 254 || code == 255;
}

bool should_skip_line(const std::string& line) {
    return line == "fi" || line == "then" || line == "else" || line == "done" || line == "esac" ||
           line == "}" || line == ";;";
}

bool contains_token(const std::string& text, const std::string& token) {
    if (text.empty()) {
        return false;
    }
    std::string normalized;
    normalized.reserve(text.size());
    for (char ch : text) {
        normalized.push_back((ch == ';') ? ' ' : ch);
    }
    std::istringstream iss(normalized);
    std::string word;
    while (iss >> word) {
        if (word == token) {
            return true;
        }
    }
    return false;
}

}  // namespace shell_script_interpreter::detail

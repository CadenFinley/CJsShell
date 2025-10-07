#include "multiline_checker.h"

#include <cctype>
#include <cstring>
#include <string>

namespace {

// Check if we're inside a quote (single or double)
bool has_unclosed_quotes(const char* input) {
    bool in_single_quote = false;
    bool in_double_quote = false;
    bool escaped = false;

    for (const char* p = input; *p != '\0'; ++p) {
        if (escaped) {
            escaped = false;
            continue;
        }

        if (*p == '\\') {
            escaped = true;
            continue;
        }

        if (*p == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
        } else if (*p == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
        }
    }

    return in_single_quote || in_double_quote;
}

// Check if we have unclosed brackets, braces, or parentheses
bool has_unclosed_delimiters(const char* input) {
    int paren_count = 0;
    int brace_count = 0;
    int bracket_count = 0;
    bool in_single_quote = false;
    bool in_double_quote = false;
    bool escaped = false;

    for (const char* p = input; *p != '\0'; ++p) {
        if (escaped) {
            escaped = false;
            continue;
        }

        if (*p == '\\') {
            escaped = true;
            continue;
        }

        // Skip counting delimiters inside quotes
        if (*p == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            continue;
        } else if (*p == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            continue;
        }

        if (in_single_quote || in_double_quote) {
            continue;
        }

        // Count delimiters
        switch (*p) {
            case '(':
                paren_count++;
                break;
            case ')':
                paren_count--;
                break;
            case '{':
                brace_count++;
                break;
            case '}':
                brace_count--;
                break;
            case '[':
                bracket_count++;
                break;
            case ']':
                bracket_count--;
                break;
        }
    }

    return paren_count > 0 || brace_count > 0 || bracket_count > 0;
}

// Check if we have an unclosed here document
bool has_unclosed_heredoc(const char* input) {
    const char* p = input;
    std::string heredoc_delimiter;

    while (*p != '\0') {
        // Look for << not followed by another <
        if (*p == '<' && *(p + 1) == '<' && *(p + 2) != '<') {
            p += 2;

            // Skip optional dash for <<-
            if (*p == '-') {
                p++;
            }

            // Skip whitespace
            while (*p != '\0' && std::isspace(*p)) {
                p++;
            }

            // Extract delimiter
            heredoc_delimiter.clear();
            bool quoted_delimiter = false;
            char quote_char = '\0';

            // Check if delimiter is quoted
            if (*p == '\'' || *p == '"') {
                quoted_delimiter = true;
                quote_char = *p;
                p++;
            }

            // Read delimiter
            while (*p != '\0' && !std::isspace(*p) && *p != ';' && *p != '&' && *p != '|') {
                if (quoted_delimiter && *p == quote_char) {
                    p++;
                    break;
                }
                heredoc_delimiter += *p;
                p++;
            }

            if (heredoc_delimiter.empty()) {
                continue;
            }

            // Now look for the closing delimiter on its own line
            const char* line_start = p;
            while (*p != '\0') {
                if (*p == '\n') {
                    line_start = p + 1;

                    // Check if the line matches the heredoc delimiter
                    const char* line_p = line_start;

                    // Skip leading tabs if using <<-
                    while (*line_p == '\t') {
                        line_p++;
                    }

                    // Check if this line is the delimiter
                    if (strncmp(line_p, heredoc_delimiter.c_str(), heredoc_delimiter.length()) ==
                        0) {
                        const char* after_delim = line_p + heredoc_delimiter.length();
                        if (*after_delim == '\0' || *after_delim == '\n' ||
                            *after_delim == '\r') {
                            // Found closing delimiter
                            break;
                        }
                    }
                }
                p++;
            }

            // If we reached the end without finding the closing delimiter
            if (*p == '\0') {
                return true;
            }
        } else {
            p++;
        }
    }

    return false;
}

}  // namespace

extern "C" bool multiline_continuation_check(const char* input, void* /*arg*/) {
    if (input == nullptr || input[0] == '\0') {
        return false;
    }

    // Check for unclosed quotes
    if (has_unclosed_quotes(input)) {
        return true;
    }

    // Check for unclosed brackets/braces/parentheses
    if (has_unclosed_delimiters(input)) {
        return true;
    }

    // Check for unclosed here documents
    if (has_unclosed_heredoc(input)) {
        return true;
    }

    return false;
}

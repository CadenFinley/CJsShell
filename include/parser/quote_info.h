#pragma once

#include <string>
#include <vector>

extern const char QUOTE_PREFIX;
extern const char QUOTE_SINGLE;
extern const char QUOTE_DOUBLE;

std::string create_quote_tag(char quote_type, const std::string& content);

/**
 * Check if a position in a string is inside quotes (single or double).
 * Takes into account escape sequences.
 *
 * @param text The text to check
 * @param pos The position to check
 * @return true if the position is inside quotes, false otherwise
 */
bool is_inside_quotes(const std::string& text, size_t pos);

struct QuoteInfo {
    bool is_single;
    bool is_double;
    std::string value;

    QuoteInfo(const std::string& token);

    bool is_quoted() const;
    bool is_unquoted() const;

   private:
    static bool is_single_quoted_token(const std::string& s);
    static bool is_double_quoted_token(const std::string& s);
    static std::string strip_quote_tag(const std::string& s);
};

std::vector<std::string> expand_tilde_tokens(const std::vector<std::string>& tokens);

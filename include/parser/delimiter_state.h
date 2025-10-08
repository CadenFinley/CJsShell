#pragma once

struct DelimiterState {
    bool in_quotes = false;
    char quote_char = '\0';
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;

    bool update_quote(char c);
    void reset();
};

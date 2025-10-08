#include "parser/delimiter_state.h"

bool DelimiterState::update_quote(char c) {
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

void DelimiterState::reset() {
    *this = {};
}

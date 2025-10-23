#pragma once

namespace utils {

enum class QuoteAdvanceResult {
    Continue,
    Process
};

struct QuoteState {
    bool in_single_quote = false;
    bool in_double_quote = false;
    bool escaped = false;

    QuoteAdvanceResult consume_forward(char c) {
        if (escaped) {
            escaped = false;
            return QuoteAdvanceResult::Continue;
        }

        if (c == '\\' && !in_single_quote) {
            escaped = true;
            return QuoteAdvanceResult::Continue;
        }

        if (c == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            return QuoteAdvanceResult::Continue;
        }

        if (c == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            return QuoteAdvanceResult::Continue;
        }

        return QuoteAdvanceResult::Process;
    }

    bool inside_quotes() const {
        return in_single_quote || in_double_quote;
    }
};

}  // namespace utils

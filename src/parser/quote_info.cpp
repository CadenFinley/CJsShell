#include "parser/quote_info.h"

#include <cstdlib>

const char QUOTE_PREFIX = '\x1F';
const char QUOTE_SINGLE = 'S';
const char QUOTE_DOUBLE = 'D';

std::string create_quote_tag(char quote_type, const std::string& content) {
    std::string result;
    result.reserve(content.size() + 2);
    result += QUOTE_PREFIX;
    result += quote_type;
    result += content;
    return result;
}

bool is_inside_quotes(const std::string& text, size_t pos) {
    bool in_single = false;
    bool in_double = false;
    bool escaped = false;

    for (size_t i = 0; i < pos && i < text.length(); ++i) {
        if (escaped) {
            escaped = false;
            continue;
        }

        if (text[i] == '\\') {
            escaped = true;
        } else if (text[i] == '\'' && !in_double) {
            in_single = !in_single;
        } else if (text[i] == '"' && !in_single) {
            in_double = !in_double;
        }
    }

    return in_single || in_double;
}

QuoteInfo::QuoteInfo(const std::string& token)
    : is_single(is_single_quoted_token(token)),
      is_double(is_double_quoted_token(token)),
      value(strip_quote_tag(token)) {
}

bool QuoteInfo::is_quoted() const {
    return is_single || is_double;
}

bool QuoteInfo::is_unquoted() const {
    return !is_single && !is_double;
}

bool QuoteInfo::is_single_quoted_token(const std::string& s) {
    return s.size() >= 2 && s[0] == '\x1F' && s[1] == 'S';
}

bool QuoteInfo::is_double_quoted_token(const std::string& s) {
    return s.size() >= 2 && s[0] == '\x1F' && s[1] == 'D';
}

std::string QuoteInfo::strip_quote_tag(const std::string& s) {
    if (s.size() >= 2 && s[0] == '\x1F' && (s[1] == 'S' || s[1] == 'D')) {
        return s.substr(2);
    }
    return s;
}

std::vector<std::string> expand_tilde_tokens(const std::vector<std::string>& tokens) {
    std::vector<std::string> result;
    result.reserve(tokens.size());

    const char* home_dir = std::getenv("HOME");
    const bool has_home = home_dir != nullptr;
    const std::string home = has_home ? std::string(home_dir) : std::string();

    auto contains_tilde = [](const std::string& value) {
        if (value.empty()) {
            return false;
        }
        if (value.front() == '~') {
            return true;
        }
        return value.find('~', 1) != std::string::npos;
    };

    auto expand_tilde_value = [](const std::string& value, const std::string& home) {
        if (!value.empty() && value.front() == '~') {
            std::string result;
            result.reserve(home.size() + value.size() - 1);
            result = home;
            result.append(value, 1, std::string::npos);
            return result;
        }
        return value;
    };

    for (const auto& raw : tokens) {
        QuoteInfo qi(raw);

        if (qi.is_unquoted() && has_home && contains_tilde(qi.value)) {
            result.push_back(expand_tilde_value(qi.value, home));
        } else if (qi.is_unquoted()) {
            result.push_back(qi.value);
        } else if (qi.is_single) {
            result.push_back(create_quote_tag(QUOTE_SINGLE, qi.value));
        } else {
            result.push_back(create_quote_tag(QUOTE_DOUBLE, qi.value));
        }
    }

    return result;
}

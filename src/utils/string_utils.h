#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace string_utils {

inline std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

inline std::string trim_ascii_whitespace_copy(const std::string& input) {
    const size_t begin = input.find_first_not_of(" \t\n\r");
    if (begin == std::string::npos) {
        return "";
    }
    const size_t end = input.find_last_not_of(" \t\n\r");
    return input.substr(begin, end - begin + 1);
}

inline void rstrip_newlines(std::string& value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
        value.pop_back();
    }
}

}  // namespace string_utils

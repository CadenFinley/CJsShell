#pragma once

#include <string>

class PatternMatcher {
   public:
    PatternMatcher() = default;
    ~PatternMatcher() = default;

    PatternMatcher(const PatternMatcher&) = delete;
    PatternMatcher& operator=(const PatternMatcher&) = delete;
    PatternMatcher(PatternMatcher&&) = default;
    PatternMatcher& operator=(PatternMatcher&&) = default;

    bool matches_pattern(const std::string& text, const std::string& pattern) const;

   private:
    bool matches_char_class(char c, const std::string& char_class) const;
    bool matches_single_pattern(const std::string& text, const std::string& pattern) const;
};

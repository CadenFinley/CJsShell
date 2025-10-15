#pragma once

#include <string>
#include <utility>

extern const std::string SUBST_LITERAL_START;
extern const std::string SUBST_LITERAL_END;

std::string trim_trailing_whitespace(std::string s);
std::string trim_leading_whitespace(std::string s);
std::string trim_whitespace(const std::string& s);
bool is_valid_identifier(const std::string& name);
bool looks_like_assignment(const std::string& value);
std::pair<std::string, bool> strip_noenv_sentinels(const std::string& s);
bool strip_subst_literal_markers(std::string& value);
bool is_char_escaped(const char* str, size_t pos);
bool is_char_escaped(const std::string& str, size_t pos);
size_t find_matching_paren(const std::string& text, size_t start_pos);
size_t find_matching_brace(const std::string& text, size_t start_pos);

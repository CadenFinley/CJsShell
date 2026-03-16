/*
  parser_utils.h

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#pragma once

#include <string>
#include <string_view>
#include <utility>

const std::string& subst_literal_start();
const std::string& subst_literal_end();
const std::string& noenv_start();
const std::string& noenv_end();
const std::string& subst_literal_start_plain();
const std::string& subst_literal_end_plain();
const std::string& noenv_start_plain();
const std::string& noenv_end_plain();
const std::string& substitution_placeholder();

std::string trim_trailing_whitespace(std::string s);
std::string trim_leading_whitespace(const std::string& s);
std::string trim_whitespace(const std::string& s);
bool is_valid_identifier_start(char c);
bool is_valid_identifier_char(char c);
bool is_valid_identifier(const std::string& name);
bool parse_assignment(const std::string& arg, std::string& name, std::string& value,
                      bool strip_surrounding_quotes = false);
bool parse_env_assignment(const std::string& arg, std::string& name, std::string& value,
                          bool strip_surrounding_quotes = false);

struct AssignmentOperand {
    std::string name;
    std::string value;
    bool has_assignment = false;
};

bool parse_assignment_operand(const std::string& arg, AssignmentOperand& operand,
                              bool strip_surrounding_quotes = false);
size_t find_token_end_with_quotes(const std::string& text, size_t start, size_t end,
                                  const std::string& delimiter_chars,
                                  bool stop_on_whitespace = true);

bool split_on_first_equals(const std::string& value, std::string& left, std::string& right,
                           bool require_nonempty_left = true);
bool looks_like_assignment(const std::string& value);
bool has_line_continuation_suffix(const std::string& text, bool trim_newlines = false);
std::pair<std::string, bool> strip_noenv_sentinels(const std::string& s);
bool strip_subst_literal_markers(std::string& value);

bool is_hex_digit(char c);

bool is_char_escaped(const char* str, size_t pos);

bool is_char_escaped(const std::string& str, size_t pos);

bool parser_starts_with_keyword_token(std::string_view text, std::string_view keyword,
                                      bool allow_open_paren_boundary = false);

bool parser_is_word_boundary(const std::string& text, size_t start, size_t length);
size_t parser_find_keyword_token(const std::string& text, const std::string& keyword,
                                 size_t search_from = 0);
size_t parser_find_inline_do_position(const std::string& text, size_t search_from = 0);
bool parser_find_matching_command_substitution_end(const std::string& text, size_t start_index,
                                                   size_t& end_out);

size_t find_matching_paren(const std::string& text, size_t start_pos);
size_t find_matching_brace(const std::string& text, size_t start_pos);

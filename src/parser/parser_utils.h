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

#include <cctype>
#include <string>
#include <utility>

const std::string& subst_literal_start();
const std::string& subst_literal_end();

std::string trim_trailing_whitespace(std::string s);
std::string trim_leading_whitespace(const std::string& s);
std::string trim_whitespace(const std::string& s);
bool is_valid_identifier_start(char c);
bool is_valid_identifier_char(char c);
bool is_valid_identifier(const std::string& name);
bool looks_like_assignment(const std::string& value);
std::pair<std::string, bool> strip_noenv_sentinels(const std::string& s);
bool strip_subst_literal_markers(std::string& value);

bool is_hex_digit(char c);

bool is_char_escaped(const char* str, size_t pos);

bool is_char_escaped(const std::string& str, size_t pos);

size_t find_matching_paren(const std::string& text, size_t start_pos);
size_t find_matching_brace(const std::string& text, size_t start_pos);

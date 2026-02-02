/*
  suggestion_utils.h

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
#include <unordered_set>
#include <vector>

namespace suggestion_utils {

std::vector<std::string> generate_command_suggestions(const std::string& command);

std::vector<std::string> generate_cd_suggestions(const std::string& target_dir,
                                                 const std::string& current_dir);

int edit_distance(const std::string& str1, const std::string& str2);

std::vector<std::string> find_similar_entries(const std::string& target_name,
                                              const std::string& directory,
                                              int max_suggestions = 3);

std::vector<std::string> generate_fuzzy_suggestions(
    const std::string& command, const std::vector<std::string>& available_commands);

int calculate_fuzzy_score(const std::string& input, const std::string& candidate);

}  // namespace suggestion_utils

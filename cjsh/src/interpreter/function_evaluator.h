/*
  function_evaluator.h

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

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace function_evaluator {

using FunctionMap = std::unordered_map<std::string, std::vector<std::string>>;
using LocalVariableStack = std::vector<std::unordered_map<std::string, std::string>>;

struct FunctionParseResult {
    bool found;
    std::string remaining_line;
};

FunctionParseResult parse_and_register_functions(
    const std::string& line, const std::vector<std::string>& lines, size_t& line_index,
    FunctionMap& functions, const std::function<std::string(const std::string&)>& trim_func,
    const std::function<std::string(const std::string&)>& strip_comment_func);

bool has_function(const FunctionMap& functions, const std::string& name);

std::vector<std::string> get_function_names(const FunctionMap& functions);

void push_function_scope(LocalVariableStack& stack);

void pop_function_scope(LocalVariableStack& stack);

void set_local_variable(
    LocalVariableStack& stack, const std::string& name, const std::string& value,
    const std::function<void(const std::string&, const std::string&)>& set_global_var);

bool is_local_variable(const LocalVariableStack& stack, const std::string& name);

}  // namespace function_evaluator

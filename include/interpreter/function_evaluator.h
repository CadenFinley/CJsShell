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

}  

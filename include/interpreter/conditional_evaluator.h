#pragma once

#include <functional>
#include <string>
#include <vector>

class Parser;

namespace conditional_evaluator {

int handle_if_block(const std::vector<std::string>& src_lines, size_t& idx,
                    const std::function<int(const std::vector<std::string>&)>& execute_block,
                    const std::function<int(const std::string&)>& execute_simple_or_pipeline,
                    const std::function<int(const std::string&)>& evaluate_logical_condition,
                    Parser* shell_parser);

std::string simplify_parentheses_in_condition(
    const std::string& condition, const std::function<int(const std::string&)>& evaluator);

int evaluate_logical_condition(const std::string& condition,
                               const std::function<int(const std::string&)>& executor);

}  

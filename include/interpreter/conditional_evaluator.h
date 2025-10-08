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

}  // namespace conditional_evaluator

#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

class Parser;

namespace loop_evaluator {

enum class LoopFlow : std::uint8_t {
    None,
    Continue,
    Break
};

struct LoopCommandOutcome {
    LoopFlow flow;
    int code;
};

LoopCommandOutcome handle_loop_command_result(int rc, int break_consumed_rc, int break_propagate_rc,
                                              int continue_consumed_rc, int continue_propagate_rc,
                                              bool allow_error_continue);

int handle_for_block(const std::vector<std::string>& src_lines, size_t& idx,
                     const std::function<int(const std::vector<std::string>&)>& execute_block,
                     Parser* shell_parser);

int handle_while_block(const std::vector<std::string>& src_lines, size_t& idx,
                       const std::function<int(const std::vector<std::string>&)>& execute_block,
                       const std::function<int(const std::string&)>& execute_simple_or_pipeline,
                       Parser* shell_parser);

int handle_until_block(const std::vector<std::string>& src_lines, size_t& idx,
                       const std::function<int(const std::vector<std::string>&)>& execute_block,
                       const std::function<int(const std::string&)>& execute_simple_or_pipeline,
                       Parser* shell_parser);

}  // namespace loop_evaluator

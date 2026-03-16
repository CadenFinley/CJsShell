#pragma once

#include <optional>
#include <string>

namespace test_expression_utils {

bool is_basic_unary_operator(const std::string& op);
bool evaluate_basic_unary_operator(const std::string& op, const std::string& arg);

bool is_numeric_comparison_operator(const std::string& op);
std::optional<bool> evaluate_numeric_comparison(const std::string& left, const std::string& op,
                                                const std::string& right);

}  // namespace test_expression_utils

#include "parameter_expansion_evaluator.h"

#include <cctype>
#include <stdexcept>

#include "shell_script_interpreter_error_reporter.h"

ParameterExpansionEvaluator::ParameterExpansionEvaluator(VariableReader var_reader,
                                                         VariableWriter var_writer,
                                                         VariableChecker var_checker,
                                                         PatternMatcher pattern_matcher)
    : read_variable(std::move(var_reader)),
      write_variable(std::move(var_writer)),
      is_variable_set(std::move(var_checker)),
      matches_pattern(std::move(pattern_matcher)) {
}

std::string ParameterExpansionEvaluator::expand(const std::string& param_expr) {
    if (param_expr.empty()) {
        return "";
    }

    // Indirect expansion: ${!var}
    if (param_expr[0] == '!') {
        std::string var_name = param_expr.substr(1);
        std::string indirect_name = read_variable(var_name);
        return read_variable(indirect_name);
    }

    // Length expansion: ${#var}
    if (param_expr[0] == '#') {
        std::string var_name = param_expr.substr(1);
        std::string value = read_variable(var_name);
        return std::to_string(value.length());
    }

    // Find the operator in the expression
    size_t op_pos = std::string::npos;
    std::string op;

    // Check for colon operators first: :- := :? :+
    for (size_t i = 1; i < param_expr.length(); ++i) {
        if (param_expr[i] == ':' && i + 1 < param_expr.length()) {
            char next = param_expr[i + 1];
            if (next == '-' || next == '=' || next == '?' || next == '+') {
                op_pos = i;
                op = param_expr.substr(i, 2);
                break;
            }
        }

        // Check for substitution operators: / and //
        if (param_expr[i] == '/') {
            if (i + 1 < param_expr.length() && param_expr[i + 1] == '/') {
                op_pos = i;
                op = "//";
                break;
            }
            op_pos = i;
            op = "/";
            break;
        }
    }

    // Check for case conversion operators: ^ ^^ , ,,
    if (op_pos == std::string::npos) {
        for (size_t i = 1; i < param_expr.length(); ++i) {
            if (param_expr[i] == '^') {
                if (i + 1 < param_expr.length() && param_expr[i + 1] == '^') {
                    op_pos = i;
                    op = "^^";
                    break;
                }
                op_pos = i;
                op = "^";
                break;
            }
            if (param_expr[i] == ',') {
                if (i + 1 < param_expr.length() && param_expr[i + 1] == ',') {
                    op_pos = i;
                    op = ",,";
                    break;
                }
                op_pos = i;
                op = ",";
                break;
            }
        }
    }

    // Check for pattern matching and simple operators: # ## % %% - = ? +
    if (op_pos == std::string::npos) {
        for (size_t i = 1; i < param_expr.length(); ++i) {
            if (param_expr[i] == '#' || param_expr[i] == '%') {
                if (i + 1 < param_expr.length() && param_expr[i + 1] == param_expr[i]) {
                    op_pos = i;
                    op = param_expr.substr(i, 2);
                    break;
                }
                op_pos = i;
                op = param_expr.substr(i, 1);
                break;
            }
            if (param_expr[i] == '-' || param_expr[i] == '=' || param_expr[i] == '?' ||
                param_expr[i] == '+') {
                op_pos = i;
                op = param_expr.substr(i, 1);
                break;
            }
        }
    }

    std::string var_name = param_expr.substr(0, op_pos);
    std::string var_value = read_variable(var_name);
    bool is_set = is_variable_set(var_name);

    // No operator found, just return the variable value
    if (op_pos == std::string::npos) {
        return var_value;
    }

    std::string operand = param_expr.substr(op_pos + op.length());

    // Default value operators
    if (op == ":-") {
        return (is_set && !var_value.empty()) ? var_value : operand;
    }
    if (op == "-") {
        return is_set ? var_value : operand;
    }

    // Assignment operators
    if (op == ":=") {
        if (!is_set || var_value.empty()) {
            write_variable(var_name, operand);
            return operand;
        }
        return var_value;
    }
    if (op == "=") {
        if (!is_set) {
            write_variable(var_name, operand);
            return operand;
        }
        return var_value;
    }

    // Error operators
    if (op == ":?") {
        if (!is_set || var_value.empty()) {
            std::string error_msg = "cjsh: " + var_name + ": " +
                                    (operand.empty() ? "parameter null or not set" : operand);
            shell_script_interpreter::print_runtime_error(error_msg,
                                                          "${" + var_name + op + operand + "}");
            throw std::runtime_error(error_msg);
        }
        return var_value;
    }
    if (op == "?") {
        if (!is_set) {
            std::string error_msg =
                "cjsh: " + var_name + ": " + (operand.empty() ? "parameter not set" : operand);
            shell_script_interpreter::print_runtime_error(error_msg,
                                                          "${" + var_name + op + operand + "}");
            throw std::runtime_error(error_msg);
        }
        return var_value;
    }

    // Alternative value operators
    if (op == ":+") {
        return (is_set && !var_value.empty()) ? operand : "";
    }
    if (op == "+") {
        return is_set ? operand : "";
    }

    // Pattern matching prefix removal
    if (op == "#") {
        return pattern_match_prefix(var_value, operand, false);
    }
    if (op == "##") {
        return pattern_match_prefix(var_value, operand, true);
    }

    // Pattern matching suffix removal
    if (op == "%") {
        return pattern_match_suffix(var_value, operand, false);
    }
    if (op == "%%") {
        return pattern_match_suffix(var_value, operand, true);
    }

    // Pattern substitution
    if (op == "/") {
        return pattern_substitute(var_value, operand, false);
    }
    if (op == "//") {
        return pattern_substitute(var_value, operand, true);
    }

    // Case conversion
    if (op == "^") {
        return case_convert(var_value, operand, true, false);
    }
    if (op == "^^") {
        return case_convert(var_value, operand, true, true);
    }
    if (op == ",") {
        return case_convert(var_value, operand, false, false);
    }
    if (op == ",,") {
        return case_convert(var_value, operand, false, true);
    }

    return var_value;
}

std::string ParameterExpansionEvaluator::pattern_match_prefix(const std::string& value,
                                                              const std::string& pattern,
                                                              bool longest) {
    if (value.empty() || pattern.empty()) {
        return value;
    }

    size_t best_match = 0;

    for (size_t i = 0; i <= value.length(); ++i) {
        std::string prefix = value.substr(0, i);
        if (matches_pattern(prefix, pattern)) {
            if (longest) {
                best_match = i;
            } else {
                return value.substr(i);
            }
        }
    }

    return value.substr(best_match);
}

std::string ParameterExpansionEvaluator::pattern_match_suffix(const std::string& value,
                                                              const std::string& pattern,
                                                              bool longest) {
    if (value.empty() || pattern.empty()) {
        return value;
    }

    size_t best_match = value.length();

    for (size_t i = 0; i <= value.length(); ++i) {
        std::string suffix = value.substr(value.length() - i);
        if (matches_pattern(suffix, pattern)) {
            if (longest) {
                best_match = value.length() - i;
            } else {
                return value.substr(0, value.length() - i);
            }
        }
    }

    return value.substr(0, best_match);
}

std::string ParameterExpansionEvaluator::pattern_substitute(const std::string& value,
                                                            const std::string& replacement_expr,
                                                            bool global) {
    if (value.empty() || replacement_expr.empty()) {
        return value;
    }

    size_t slash_pos = replacement_expr.find('/');
    if (slash_pos == std::string::npos) {
        return value;
    }

    std::string pattern = replacement_expr.substr(0, slash_pos);
    std::string replacement = replacement_expr.substr(slash_pos + 1);

    if (pattern.empty()) {
        return value;
    }

    std::string result = value;

    // Handle literal string replacement (no wildcards)
    if (pattern.find('*') == std::string::npos && pattern.find('?') == std::string::npos) {
        if (global) {
            size_t pos = 0;
            while ((pos = result.find(pattern, pos)) != std::string::npos) {
                result.replace(pos, pattern.length(), replacement);
                pos += replacement.length();
            }
        } else {
            size_t pos = result.find(pattern);
            if (pos != std::string::npos) {
                result.replace(pos, pattern.length(), replacement);
            }
        }
    } else {
        // Handle pattern matching (wildcards)
        if (!global && matches_pattern(result, pattern)) {
            result = replacement;
        }
    }

    return result;
}

std::string ParameterExpansionEvaluator::case_convert(const std::string& value,
                                                      const std::string& pattern, bool uppercase,
                                                      bool all_chars) {
    if (value.empty()) {
        return value;
    }

    std::string result = value;

    if (pattern.empty()) {
        if (all_chars) {
            for (char& c : result) {
                if (uppercase) {
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                } else {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
            }
        } else {
            if (!result.empty()) {
                if (uppercase) {
                    result[0] =
                        static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
                } else {
                    result[0] =
                        static_cast<char>(std::tolower(static_cast<unsigned char>(result[0])));
                }
            }
        }
    } else {
        // Pattern-based case conversion (currently simplified to match empty pattern behavior)
        if (all_chars) {
            for (char& c : result) {
                if (uppercase) {
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                } else {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
            }
        } else {
            if (!result.empty()) {
                if (uppercase) {
                    result[0] =
                        static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
                } else {
                    result[0] =
                        static_cast<char>(std::tolower(static_cast<unsigned char>(result[0])));
                }
            }
        }
    }

    return result;
}

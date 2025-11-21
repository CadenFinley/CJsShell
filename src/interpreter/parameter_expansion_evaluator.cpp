#include "parameter_expansion_evaluator.h"

#include <cctype>
#include <cstdlib>
#include <stdexcept>


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

    if (param_expr[0] == '!') {
        std::string var_name = param_expr.substr(1);
        std::string indirect_name = read_variable(var_name);
        return read_variable(indirect_name);
    }

    if (param_expr[0] == '#') {
        std::string var_name = param_expr.substr(1);
        std::string value = read_variable(var_name);
        return std::to_string(value.length());
    }

    std::string substring_result;
    if (try_evaluate_substring(param_expr, substring_result)) {
        return substring_result;
    }

    size_t op_pos = std::string::npos;
    std::string op;

    auto is_operator_start = [](char c) {
        switch (c) {
            case ':':
            case '#':
            case '%':
            case '/':
            case '^':
            case ',':
            case '-':
            case '=':
            case '?':
            case '+':
                return true;
            default:
                return false;
        }
    };

    for (size_t i = 1; i < param_expr.length(); ++i) {
        if (is_operator_start(param_expr[i])) {
            op_pos = i;
            break;
        }
    }

    if (op_pos != std::string::npos) {
        char op_char = param_expr[op_pos];
        switch (op_char) {
            case ':': {
                if (op_pos + 1 < param_expr.length()) {
                    char next = param_expr[op_pos + 1];
                    if (next == '-' || next == '=' || next == '?' || next == '+') {
                        op = param_expr.substr(op_pos, 2);
                    }
                }
                break;
            }
            case '#': {
                if (op_pos + 1 < param_expr.length() && param_expr[op_pos + 1] == '#') {
                    op = "##";
                } else {
                    op = "#";
                }
                break;
            }
            case '%': {
                if (op_pos + 1 < param_expr.length() && param_expr[op_pos + 1] == '%') {
                    op = "%%";
                } else {
                    op = "%";
                }
                break;
            }
            case '/': {
                if (op_pos + 1 < param_expr.length() && param_expr[op_pos + 1] == '/') {
                    op = "//";
                } else {
                    op = "/";
                }
                break;
            }
            case '^': {
                if (op_pos + 1 < param_expr.length() && param_expr[op_pos + 1] == '^') {
                    op = "^^";
                } else {
                    op = "^";
                }
                break;
            }
            case ',': {
                if (op_pos + 1 < param_expr.length() && param_expr[op_pos + 1] == ',') {
                    op = ",,";
                } else {
                    op = ",";
                }
                break;
            }
            case '-':
            case '=':
            case '?':
            case '+': {
                op = param_expr.substr(op_pos, 1);
                break;
            }
            default:
                break;
        }
    }

    if (op.empty()) {
        op_pos = std::string::npos;
    }

    std::string var_name = param_expr.substr(0, op_pos);
    std::string var_value = read_variable(var_name);
    bool is_set = is_variable_set(var_name);

    if (op_pos == std::string::npos) {
        return var_value;
    }

    std::string operand = param_expr.substr(op_pos + op.length());

    if (op == ":-") {
        return (is_set && !var_value.empty()) ? var_value : operand;
    }
    if (op == "-") {
        return is_set ? var_value : operand;
    }

    if (op == ":=") {
        if (!is_set) {
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

    if (op == ":?") {
        if (!is_set || var_value.empty()) {
            std::string error_msg = var_name + ": " +
                                    (operand.empty() ? "parameter null or not set" : operand);
            throw std::runtime_error("parameter expansion error: " + error_msg +
                                     " in ${" + var_name + op + operand + "}");
        }
        return var_value;
    }
    if (op == "?") {
        if (!is_set) {
            std::string error_msg =
                var_name + ": " + (operand.empty() ? "parameter not set" : operand);
            throw std::runtime_error("parameter expansion error: " + error_msg +
                                     " in ${" + var_name + op + operand + "}");
        }
        return var_value;
    }

    if (op == ":+") {
        return (is_set && !var_value.empty()) ? operand : "";
    }
    if (op == "+") {
        return is_set ? operand : "";
    }

    if (op == "#") {
        return pattern_match_prefix(var_value, operand, false);
    }
    if (op == "##") {
        return pattern_match_prefix(var_value, operand, true);
    }

    if (op == "%") {
        return pattern_match_suffix(var_value, operand, false);
    }
    if (op == "%%") {
        return pattern_match_suffix(var_value, operand, true);
    }

    if (op == "/") {
        return pattern_substitute(var_value, operand, false);
    }
    if (op == "//") {
        return pattern_substitute(var_value, operand, true);
    }

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

    bool anchor_prefix = false;
    bool anchor_suffix = false;
    if (!pattern.empty() && (pattern[0] == '#' || pattern[0] == '%')) {
        anchor_prefix = pattern[0] == '#';
        anchor_suffix = pattern[0] == '%';
        pattern.erase(0, 1);
        if (pattern.empty()) {
            return value;
        }
    }

    std::string result = value;

    auto has_wildcards = [](const std::string& text) {
        return text.find('*') != std::string::npos || text.find('?') != std::string::npos ||
               text.find('[') != std::string::npos;
    };

    if (anchor_prefix) {
        std::string remainder = pattern_match_prefix(value, pattern, true);
        if (remainder.length() != value.length()) {
            return replacement + remainder;
        }
        return value;
    }

    if (anchor_suffix) {
        std::string prefix = pattern_match_suffix(value, pattern, true);
        if (prefix.length() != value.length()) {
            return prefix + replacement;
        }
        return value;
    }

    if (!has_wildcards(pattern)) {
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
        if (!global && matches_pattern(result, pattern)) {
            result = replacement;
        }
    }

    return result;
}

bool ParameterExpansionEvaluator::try_evaluate_substring(const std::string& param_expr,
                                                         std::string& result) {
    size_t colon_pos = param_expr.find(':');
    if (colon_pos == std::string::npos || colon_pos + 1 >= param_expr.length()) {
        return false;
    }

    if (colon_pos + 2 <= param_expr.length()) {
        std::string possible_op = param_expr.substr(colon_pos, 2);
        if (possible_op == ":-" || possible_op == ":=" || possible_op == ":?" ||
            possible_op == ":+") {
            return false;
        }
    }

    auto is_digit = [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; };

    size_t pos = colon_pos + 1;
    while (pos < param_expr.length() && std::isspace(static_cast<unsigned char>(param_expr[pos]))) {
        pos++;
    }

    if (pos >= param_expr.length()) {
        return false;
    }

    char marker = param_expr[pos];
    if (!is_digit(marker) && ((marker != '+' && marker != '-') || pos + 1 >= param_expr.length() ||
                              !is_digit(param_expr[pos + 1]))) {
        return false;
    }

    std::string var_name = param_expr.substr(0, colon_pos);
    std::string var_value = read_variable(var_name);

    int offset_sign = 1;
    if (pos < param_expr.length() && (param_expr[pos] == '+' || param_expr[pos] == '-')) {
        offset_sign = (param_expr[pos] == '-') ? -1 : 1;
        pos++;
    }

    const char* start_ptr = param_expr.c_str() + pos;
    char* endptr_raw = nullptr;
    long offset_value = std::strtol(start_ptr, &endptr_raw, 10);
    const char* endptr = endptr_raw;
    if (start_ptr == endptr) {
        offset_value = 0;
        endptr = start_ptr;
    }
    size_t consumed = static_cast<size_t>(endptr - param_expr.c_str());
    pos = consumed;
    offset_value *= offset_sign;

    while (pos < param_expr.length() && std::isspace(static_cast<unsigned char>(param_expr[pos]))) {
        pos++;
    }

    bool length_specified = false;
    long length_value = 0;
    if (pos < param_expr.length() && param_expr[pos] == ':') {
        length_specified = true;
        pos++;
        while (pos < param_expr.length() &&
               std::isspace(static_cast<unsigned char>(param_expr[pos]))) {
            pos++;
        }

        int length_sign = 1;
        if (pos < param_expr.length() && (param_expr[pos] == '+' || param_expr[pos] == '-')) {
            length_sign = (param_expr[pos] == '-') ? -1 : 1;
            pos++;
        }

        const char* length_ptr = param_expr.c_str() + pos;
        char* length_endptr_raw = nullptr;
        length_value = std::strtol(length_ptr, &length_endptr_raw, 10);
        const char* length_endptr = length_endptr_raw;
        if (length_ptr == length_endptr) {
            length_value = 0;
            length_endptr = length_ptr;
        }
        pos = static_cast<size_t>(length_endptr - param_expr.c_str());
        length_value *= length_sign;
    }

    long value_len = static_cast<long>(var_value.length());
    long start_index = offset_value;
    if (start_index < 0) {
        start_index = value_len + start_index;
    }

    if (start_index < 0) {
        start_index = 0;
    }
    if (start_index > value_len) {
        result = "";
        return true;
    }

    long slice_length;
    if (length_specified) {
        if (length_value <= 0) {
            result = "";
            return true;
        }
        slice_length = length_value;
    } else {
        slice_length = value_len - start_index;
    }

    if (start_index + slice_length > value_len) {
        slice_length = value_len - start_index;
    }

    result = var_value.substr(static_cast<size_t>(start_index), static_cast<size_t>(slice_length));
    return true;
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
